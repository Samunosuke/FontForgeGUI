#include "backend/FontForgeBridge.hpp"

#include <QByteArray>
#include <QFileInfo>
#include <QTransform>

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

// FontForge C headers are isolated inside this translation unit to avoid macro/typedef
// collisions (e.g. real, Color, Point) with Qt headers.
extern "C" {
#include <fontforge-config.h>
#include "baseviews.h"
#include "fontforge.h"
#include "fvfonts.h"
#include "lookups.h"
#include "savefont.h"
#include "sfd.h"
#include "splinechar.h"
#include "splinefont.h"
#include "splineutil.h"
#include "start.h"
#include "tottf.h"
#include "uiinterface.h"
#include "ustring.h"
}

namespace ffgui {

namespace {

FontForgeBridge::LogCallback* g_logCallback = nullptr;
FontForgeBridge::ProgressCallback* g_progressCallback = nullptr;
QString g_lastError;
struct ui_interface g_defaultUi {};
struct ui_interface g_customUi {};
bool g_uiHooked = false;
int g_progressTotal = 0;
int g_progressCurrent = 0;
QString g_progressTitle;

QString formatVa(const char* fmt, va_list ap) {
    if (!fmt) {
        return {};
    }
    char stackBuf[1024];
    va_list apCopy;
    va_copy(apCopy, ap);
    int needed = std::vsnprintf(stackBuf, sizeof(stackBuf), fmt, apCopy);
    va_end(apCopy);

    if (needed < 0) {
        return QString::fromUtf8(fmt);
    }
    if (static_cast<size_t>(needed) < sizeof(stackBuf)) {
        return QString::fromUtf8(stackBuf, needed);
    }

    std::vector<char> heapBuf(static_cast<size_t>(needed) + 1);
    std::vsnprintf(heapBuf.data(), heapBuf.size(), fmt, ap);
    return QString::fromUtf8(heapBuf.data(), needed);
}

void emitLog(const QString& level, const QString& msg) {
    const QString trimmed = msg.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (level == QStringLiteral("error")) {
        g_lastError = trimmed;
    }
    if (g_logCallback && *g_logCallback) {
        (*g_logCallback)(level, trimmed);
    }
}

void hookIError(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const QString msg = formatVa(fmt, ap);
    va_end(ap);
    emitLog(QStringLiteral("error"), QStringLiteral("Internal: ") + msg);
}

void hookPostError(const char* title, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const QString msg = formatVa(fmt, ap);
    va_end(ap);
    const QString prefix = title ? QString::fromUtf8(title) + QStringLiteral(": ") : QString();
    emitLog(QStringLiteral("error"), prefix + msg);
}

void hookLogWarning(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const QString msg = formatVa(fmt, ap);
    va_end(ap);
    emitLog(QStringLiteral("warning"), msg);
}

void hookPostWarning(const char* title, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const QString msg = formatVa(fmt, ap);
    va_end(ap);
    const QString prefix = title ? QString::fromUtf8(title) + QStringLiteral(": ") : QString();
    emitLog(QStringLiteral("warning"), prefix + msg);
}

void hookProgressStart(int /*delay*/, const char* title, const char* line1,
                       const char* /*line2*/, int tot, int /*stages*/) {
    g_progressCurrent = 0;
    g_progressTotal = std::max(tot, 0);
    g_progressTitle = title ? QString::fromUtf8(title)
                            : (line1 ? QString::fromUtf8(line1) : QString());
    if (g_progressCallback && *g_progressCallback) {
        (*g_progressCallback)(g_progressCurrent, g_progressTotal, g_progressTitle);
    }
}

void hookProgressEnd(void) {
    if (g_progressCallback && *g_progressCallback) {
        (*g_progressCallback)(g_progressTotal, g_progressTotal, QString());
    }
}

int hookProgressIncrement(int delta) {
    g_progressCurrent += delta;
    if (g_progressCallback && *g_progressCallback) {
        (*g_progressCallback)(g_progressCurrent, g_progressTotal, g_progressTitle);
    }
    return true;
}

int hookProgressNext(void) {
    return hookProgressIncrement(1);
}

void hookProgressChangeLine1(const char* line1) {
    if (line1) {
        g_progressTitle = QString::fromUtf8(line1);
    }
    if (g_progressCallback && *g_progressCallback) {
        (*g_progressCallback)(g_progressCurrent, g_progressTotal, g_progressTitle);
    }
}

void installUiHooks() {
    if (g_uiHooked) {
        return;
    }
    if (ui_interface != nullptr) {
        g_defaultUi = *ui_interface;
    }
    g_customUi = g_defaultUi;
    g_customUi.ierror = hookIError;
    g_customUi.post_error = hookPostError;
    g_customUi.logwarning = hookLogWarning;
    g_customUi.post_warning = hookPostWarning;
    g_customUi.progress_start = hookProgressStart;
    g_customUi.progress_end = hookProgressEnd;
    g_customUi.progress_increment = hookProgressIncrement;
    g_customUi.progress_next = hookProgressNext;
    g_customUi.progress_change_line1 = hookProgressChangeLine1;
    FF_SetUiInterface(&g_customUi);
    g_uiHooked = true;
}

QString qstr(const char* s) {
    return s ? QString::fromUtf8(s) : QString();
}

void replaceCString(char** dst, const QString& value, bool allowNull) {
    free(*dst);
    if (allowNull && value.isEmpty()) {
        *dst = nullptr;
    } else {
        const QByteArray bytes = value.toUtf8();
        *dst = copy(bytes.constData());
    }
}

QString sfntStringIdName(int id) {
    if (id >= 0 && id < ttf_namemax) {
        const char* label = TTFNameIds(id);
        if (label) {
            return QString::fromUtf8(label);
        }
    }
    return QStringLiteral("ID %1").arg(id);
}

QString sfntLanguageName(int lang) {
    const char* label = MSLangString(lang);
    if (label && label[0] != '\0') {
        return QString::fromUtf8(label);
    }
    return QStringLiteral("0x%1").arg(lang, 4, 16, QLatin1Char('0')).toUpper();
}

void appendSplineSet(const SplineSet* head, const QTransform& transform, QPainterPath& outPath,
                     int& contourCount, int& pointCount) {
    constexpr int kMaxSplinesPerContour = 200000;
    for (const SplineSet* spl = head; spl != nullptr; spl = spl->next) {
        if (spl->first == nullptr || spl->is_clip_path) {
            continue;
        }
        ++contourCount;

        QPainterPath subPath;
        const QPointF startPt = transform.map(QPointF(spl->first->me.x, spl->first->me.y));
        subPath.moveTo(startPt);
        ++pointCount;

        const Spline* firstSpline = nullptr;
        int guard = 0;
        for (const Spline* sp = spl->first->next;
             sp != nullptr && sp != firstSpline && guard < kMaxSplinesPerContour;
             sp = sp->to ? sp->to->next : nullptr, ++guard) {
            if (firstSpline == nullptr) {
                firstSpline = sp;
            }
            if (sp->from == nullptr || sp->to == nullptr) {
                break;
            }
            const QPointF endPt = transform.map(QPointF(sp->to->me.x, sp->to->me.y));
            if (sp->knownlinear) {
                subPath.lineTo(endPt);
            } else if (sp->order2) {
                const QPointF cp = transform.map(QPointF(sp->to->prevcp.x, sp->to->prevcp.y));
                subPath.quadTo(cp, endPt);
            } else {
                const QPointF cp1 =
                    transform.map(QPointF(sp->from->nextcp.x, sp->from->nextcp.y));
                const QPointF cp2 =
                    transform.map(QPointF(sp->to->prevcp.x, sp->to->prevcp.y));
                subPath.cubicTo(cp1, cp2, endPt);
            }
            ++pointCount;
        }

        if (spl->first->prev != nullptr) {
            subPath.closeSubpath();
        }
        outPath.addPath(subPath);
    }
}

void appendLayerOutlines(const SplineChar* sc, int layerIdx, const QTransform& transform,
                         QPainterPath& outPath, int& contourCount, int& pointCount,
                         int& refCount, int depth) {
    if (sc == nullptr || depth > 8 || layerIdx < 0 || layerIdx >= sc->layer_cnt) {
        return;
    }
    const Layer& layer = sc->layers[layerIdx];
    appendSplineSet(layer.splines, transform, outPath, contourCount, pointCount);

    for (const RefChar* ref = layer.refs; ref != nullptr; ref = ref->next) {
        if (depth == 0) {
            ++refCount;
        }
        if (ref->layers != nullptr && ref->layer_cnt > 0) {
            for (int i = 0; i < ref->layer_cnt; ++i) {
                appendSplineSet(ref->layers[i].splines, transform, outPath, contourCount,
                                pointCount);
            }
        } else if (ref->sc != nullptr) {
            const QTransform refTx(ref->transform[0], ref->transform[1], ref->transform[2],
                                   ref->transform[3], ref->transform[4], ref->transform[5]);
            appendLayerOutlines(ref->sc, layerIdx, refTx * transform, outPath, contourCount,
                                pointCount, refCount, depth + 1);
        }
    }
}

QVector<SfntNameEntry> extractSfntNames(SplineFont* sf) {
    QVector<SfntNameEntry> entries;
    struct ttflangname dummy {};
    DefaultTTFEnglishNames(&dummy, sf);

    struct ttflangname* english = nullptr;
    for (struct ttflangname* cur = sf->names; cur != nullptr; cur = cur->next) {
        if (cur->lang == 0x409) {
            english = cur;
            break;
        }
    }

    for (int i = 0; i < ttf_namemax; ++i) {
        const char* nm = (english != nullptr && english->names[i] != nullptr) ? english->names[i]
                                                                              : dummy.names[i];
        if (nm != nullptr && nm[0] != '\0') {
            entries.append(
                {0x409, sfntLanguageName(0x409), i, sfntStringIdName(i), QString::fromUtf8(nm)});
        }
    }

    for (struct ttflangname* cur = sf->names; cur != nullptr; cur = cur->next) {
        if (cur == english) {
            continue;
        }
        for (int i = 0; i < ttf_namemax; ++i) {
            if (cur->names[i] != nullptr && cur->names[i][0] != '\0') {
                entries.append({cur->lang, sfntLanguageName(cur->lang), i, sfntStringIdName(i),
                                QString::fromUtf8(cur->names[i])});
            }
        }
    }

    for (int i = 0; i < ttf_namemax; ++i) {
        free(dummy.names[i]);
    }
    return entries;
}

FontSummary extractFontSummary(SplineFont* sf, FontViewBase* fv, const QString& path) {
    FontSummary fs;
    fs.filePath = path;
    fs.fontName = qstr(sf->fontname);
    fs.familyName = qstr(sf->familyname);
    fs.fullName = qstr(sf->fullname);
    fs.weight = qstr(sf->weight);
    fs.version = qstr(sf->version);
    fs.copyright = qstr(sf->copyright);
    fs.ascent = sf->ascent;
    fs.descent = sf->descent;
    fs.emSize = std::max(1, sf->ascent + sf->descent);
    fs.italicAngle = static_cast<double>(sf->italicangle);
    fs.underlinePos = static_cast<double>(sf->upos);
    fs.underlineWidth = static_cast<double>(sf->uwidth);

    const int activeLayer = (fv != nullptr && fv->active_layer >= 0) ? fv->active_layer : ly_fore;
    if (sf->grid.order2 || (sf->glyphcnt > 0 && sf->glyphs[0] != nullptr &&
                            activeLayer < sf->glyphs[0]->layer_cnt &&
                            sf->glyphs[0]->layers[activeLayer].order2)) {
        fs.isOrder2 = true;
    }

    int count = 0;
    for (int gid = 0; gid < sf->glyphcnt; ++gid) {
        if (sf->glyphs[gid] != nullptr) {
            ++count;
        }
    }
    fs.glyphCount = count;
    fs.sfntNames = extractSfntNames(sf);
    return fs;
}

GlyphSummary extractGlyphSummary(const SplineChar* sc, int gid, int activeLayer) {
    GlyphSummary gs;
    gs.gid = gid;
    gs.unicode = sc->unicodeenc;
    for (const struct altuni* au = sc->altuni; au != nullptr; au = au->next) {
        gs.altUnicodes.append(au->unienc);
    }
    gs.name = qstr(sc->name);
    gs.width = sc->width;
    gs.vwidth = sc->vwidth;
    gs.outline.setFillRule(Qt::WindingFill);

    appendLayerOutlines(sc, activeLayer, QTransform(), gs.outline, gs.contourCount, gs.pointCount,
                        gs.referenceCount, 0);
    gs.hasOutline = !gs.outline.isEmpty();
    if (gs.hasOutline) {
        gs.bounds = gs.outline.boundingRect();
    }
    return gs;
}

FontDocumentSnapshot buildSnapshot(SplineFont* sf, FontViewBase* fv, const QString& path) {
    FontDocumentSnapshot snap;
    snap.summary = extractFontSummary(sf, fv, path);

    const int activeLayer = (fv != nullptr && fv->active_layer >= 0) ? fv->active_layer : ly_fore;
    snap.glyphs.reserve(sf->glyphcnt);
    for (int gid = 0; gid < sf->glyphcnt; ++gid) {
        const SplineChar* sc = sf->glyphs[gid];
        if (sc == nullptr) {
            continue;
        }
        snap.glyphs.append(extractGlyphSummary(sc, gid, activeLayer));
    }

    snap.summary.glyphCount = snap.glyphs.size();
    return snap;
}

QStringList describeValidationFlags(uint32_t flags) {
    struct FlagDesc {
        uint32_t mask;
        const char* text;
    };
    static constexpr std::array<FlagDesc, 22> kTable = {{
        {vs_opencontour, "Open contour"},
        {vs_selfintersects, "Self-intersecting contours"},
        {vs_wrongdirection, "Wrong contour direction"},
        {vs_flippedreferences, "Flipped reference(s)"},
        {vs_missingextrema, "Missing points at extrema"},
        {vs_missingglyphnameingsub, "Missing glyph name in GSUB"},
        {vs_toomanypoints, "Too many points (>1500)"},
        {vs_toomanyhints, "Too many hints (>96)"},
        {vs_badglyphname, "Invalid glyph name"},
        {vs_maxp_toomanypoints, "Exceeds maxp point limit"},
        {vs_maxp_toomanypaths, "Exceeds maxp contour limit"},
        {vs_maxp_toomanycomppoints, "Exceeds maxp composite point limit"},
        {vs_maxp_toomanycomppaths, "Exceeds maxp composite contour limit"},
        {vs_maxp_instrtoolong, "TrueType instructions too long"},
        {vs_maxp_toomanyrefs, "Too many component references"},
        {vs_maxp_refstoodeep, "Component references nested too deep"},
        {vs_pointstoofarapart, "Points too far apart (>32767)"},
        {vs_nonintegral, "Non-integral coordinates"},
        {vs_missinganchor, "Missing anchor point"},
        {vs_dupname, "Duplicate glyph name"},
        {vs_dupunicode, "Duplicate Unicode code point"},
        {vs_overlappedhints, "Overlapping hints"},
    }};

    QStringList out;
    for (const FlagDesc& entry : kTable) {
        if ((flags & entry.mask) != 0) {
            out.append(QString::fromLatin1(entry.text));
        }
    }
    return out;
}

} // namespace

struct FontForgeBridge::Impl {
    bool initialized = false;
    FontViewBase* fv = nullptr;
    SplineFont* sf = nullptr;
    QString currentPath;
    LogCallback logCb;
    ProgressCallback progressCb;
};

FontForgeBridge::FontForgeBridge() : m_impl(std::make_unique<Impl>()) {}

FontForgeBridge::~FontForgeBridge() {
    closeFont();
    if (g_logCallback == &m_impl->logCb) {
        g_logCallback = nullptr;
    }
    if (g_progressCallback == &m_impl->progressCb) {
        g_progressCallback = nullptr;
    }
}

void FontForgeBridge::setLogCallback(LogCallback cb) {
    m_impl->logCb = std::move(cb);
    g_logCallback = &m_impl->logCb;
}

void FontForgeBridge::setProgressCallback(ProgressCallback cb) {
    m_impl->progressCb = std::move(cb);
    g_progressCallback = &m_impl->progressCb;
}

void FontForgeBridge::initialize() {
    if (m_impl->initialized) {
        return;
    }
    doinitFontForgeMain();
    installUiHooks();
    m_impl->initialized = true;
}

bool FontForgeBridge::openFont(const QString& path, FontDocumentSnapshot* outSnapshot,
                               QString* outError) {
    initialize();
    closeFont();
    g_lastError.clear();

    if (path.isEmpty() || !QFileInfo::exists(path)) {
        if (outError) {
            *outError = QStringLiteral("Font file does not exist: %1").arg(path);
        }
        return false;
    }

    const QByteArray pathBytes = path.toUtf8();
    SplineFont* loaded = LoadSplineFont(pathBytes.constData(), static_cast<enum openflags>(0));
    if (loaded == nullptr) {
        if (outError) {
            *outError = g_lastError.isEmpty()
                            ? QStringLiteral("Failed to load font: %1").arg(path)
                            : g_lastError;
        }
        return false;
    }

    // Headless noui_fv does not append to fv_list in FontViewCreate; FVAppend is
    // required so FontViewClose() cleanly unlinks and frees fv.
    FontViewBase* fv = FVAppend(_FontViewCreate(loaded));
    if (fv == nullptr) {
        SplineFontFree(loaded);
        if (outError) {
            *outError = QStringLiteral("Failed to create FontForge view for: %1").arg(path);
        }
        return false;
    }

    m_impl->fv = fv;
    m_impl->sf = fv->sf ? fv->sf : loaded;
    m_impl->currentPath = path;

    if (outSnapshot != nullptr) {
        *outSnapshot = buildSnapshot(m_impl->sf, m_impl->fv, path);
    }
    return true;
}

bool FontForgeBridge::exportFont(const ExportOptions& options, QString* outError) {
    if (!hasOpenFont()) {
        if (outError) {
            *outError = QStringLiteral("No font is currently loaded.");
        }
        return false;
    }
    if (options.outputPath.isEmpty()) {
        if (outError) {
            *outError = QStringLiteral("Output path must not be empty.");
        }
        return false;
    }

    g_lastError.clear();
    QByteArray outBytes = options.outputPath.toUtf8();

    if (options.outputPath.endsWith(QStringLiteral(".sfd"), Qt::CaseInsensitive)) {
        const int ok = SFDWrite(outBytes.data(), m_impl->sf, m_impl->fv->map, m_impl->fv->normal,
                                m_impl->sf->save_to_dir);
        if (!ok) {
            if (outError) {
                *outError = g_lastError.isEmpty()
                                ? QStringLiteral("Failed to write SFD file: %1")
                                      .arg(options.outputPath)
                                : g_lastError;
            }
            return false;
        }
        return true;
    }

    int fmFlags = 0;
    if (options.roundToInt) {
        fmFlags |= fm_flag_round;
    }
    if (!options.opentypeTables) {
        fmFlags |= fm_flag_apple;
    }
    if (options.omitHints) {
        fmFlags |= fm_flag_nottfhints | fm_flag_nopshints;
    }
    if (options.dummyDsig) {
        fmFlags |= fm_flag_dummyDSIG;
    }
    if (options.generateAfm) {
        fmFlags |= fm_flag_afm;
    }

    EncMap* exportMap = m_impl->fv->normal != nullptr ? m_impl->fv->normal : m_impl->fv->map;
    const int layer = m_impl->fv->active_layer >= 0 ? m_impl->fv->active_layer : ly_fore;

    const int ok = GenerateScript(m_impl->sf, outBytes.data(), "", fmFlags, -1, nullptr, nullptr,
                                  exportMap, nullptr, layer);
    if (!ok) {
        if (outError) {
            *outError = g_lastError.isEmpty()
                            ? QStringLiteral("Failed to export font to: %1")
                                  .arg(options.outputPath)
                            : g_lastError;
        }
        return false;
    }
    return true;
}

bool FontForgeBridge::updateGlyphMetadata(const GlyphMetadataUpdate& update, GlyphSummary* outGlyph,
                                          QString* outError) {
    if (!hasOpenFont()) {
        if (outError) {
            *outError = QStringLiteral("No font is currently loaded.");
        }
        return false;
    }

    SplineFont* sf = m_impl->sf;
    if (update.gid < 0 || update.gid >= sf->glyphcnt || sf->glyphs[update.gid] == nullptr) {
        if (outError) {
            *outError = QStringLiteral("Invalid glyph ID: %1").arg(update.gid);
        }
        return false;
    }

    const QString trimmedName = update.name.trimmed();
    if (trimmedName.isEmpty()) {
        if (outError) {
            *outError = QStringLiteral("Glyph name must not be empty.");
        }
        return false;
    }
    for (QChar ch : trimmedName) {
        if (ch.isSpace()) {
            if (outError) {
                *outError = QStringLiteral("Glyph name must not contain whitespace.");
            }
            return false;
        }
    }
    if (update.unicode < -1 || update.unicode > 0x10FFFF) {
        if (outError) {
            *outError = QStringLiteral("Unicode code point is out of range.");
        }
        return false;
    }
    for (int32_t alt : update.altUnicodes) {
        if (alt < 0 || alt > 0x10FFFF) {
            if (outError) {
                *outError = QStringLiteral("Alternate Unicode code point is out of range.");
            }
            return false;
        }
    }

    SplineChar* sc = sf->glyphs[update.gid];
    const QByteArray newNameBytes = trimmedName.toUtf8();
    if (sc->name == nullptr || std::strcmp(sc->name, newNameBytes.constData()) != 0) {
        SFGlyphRenameFixup(sf, sc->name, newNameBytes.constData(), false);
        free(sc->name);
        sc->name = copy(newNameBytes.constData());
        sc->namechanged = sc->changed = true;
        GlyphHashFree(sf);
        SCRefreshTitles(sc);
        for (FontViewBase* fvs = sf->fv; fvs != nullptr; fvs = fvs->nextsame) {
            if (fvs->map && fvs->map->enc && fvs->map->enc->psnames != nullptr &&
                fvs->map->enc != &custom) {
                fvs->map->enc = &custom;
            }
        }
    }

    if (sc->unicodeenc != update.unicode) {
        sc->unicodeenc = update.unicode;
        sc->changed = true;
        SCRefreshTitles(sc);
        for (FontViewBase* fvs = sf->fv; fvs != nullptr; fvs = fvs->nextsame) {
            if (fvs->map && fvs->map->enc && fvs->map->enc->psnames == nullptr &&
                fvs->map->enc != &custom) {
                fvs->map->enc = &custom;
            }
        }
    }

    struct altuni* head = nullptr;
    struct altuni* tail = nullptr;
    for (int32_t alt : update.altUnicodes) {
        auto* node = static_cast<struct altuni*>(chunkalloc(sizeof(struct altuni)));
        node->unienc = alt;
        node->vs = -1;
        node->fid = 0;
        if (head == nullptr) {
            head = node;
        } else {
            tail->next = node;
        }
        tail = node;
    }
    AltUniFree(sc->altuni);
    sc->altuni = head;
    sc->changed = true;
    sf->changed = true;

    const int activeLayer =
        (m_impl->fv && m_impl->fv->active_layer >= 0) ? m_impl->fv->active_layer : ly_fore;
    if (activeLayer >= 0 && activeLayer < sc->layer_cnt) {
        sc->layers[activeLayer].validation_state = vs_unknown;
    }

    if (outGlyph != nullptr) {
        *outGlyph = extractGlyphSummary(sc, update.gid, activeLayer);
    }
    return true;
}

bool FontForgeBridge::updateFontMetadata(const FontMetadataUpdate& update, FontSummary* outSummary,
                                         QString* outError) {
    if (!hasOpenFont()) {
        if (outError) {
            *outError = QStringLiteral("No font is currently loaded.");
        }
        return false;
    }

    const QString trimmedFontName = update.fontName.trimmed();
    if (trimmedFontName.isEmpty()) {
        if (outError) {
            *outError = QStringLiteral("PostScript Font Name must not be empty.");
        }
        return false;
    }
    if (update.ascent < 0 || update.descent < 0 || (update.ascent + update.descent) <= 0) {
        if (outError) {
            *outError = QStringLiteral("Ascent + Descent (EM size) must be positive.");
        }
        return false;
    }

    SplineFont* sf = m_impl->sf;
    replaceCString(&sf->fontname, trimmedFontName, false);
    replaceCString(&sf->familyname, update.familyName.trimmed(), false);
    replaceCString(&sf->fullname, update.fullName.trimmed(), false);
    replaceCString(&sf->weight, update.weight.trimmed(), false);
    replaceCString(&sf->version, update.version.trimmed(), true);
    replaceCString(&sf->copyright, update.copyright, true);

    sf->ascent = update.ascent;
    sf->descent = update.descent;
    sf->italicangle = update.italicAngle;
    sf->upos = update.underlinePos;
    sf->uwidth = update.underlineWidth;

    if (update.updateSfntNames) {
        for (struct ttflangname* cur = sf->names; cur != nullptr; cur = cur->next) {
            for (int i = 0; i < ttf_namemax; ++i) {
                free(cur->names[i]);
                cur->names[i] = nullptr;
            }
        }

        struct ttflangname dummy {};
        DefaultTTFEnglishNames(&dummy, sf);

        for (const SfntNameEntry& entry : update.sfntNames) {
            if (entry.stringId < 0 || entry.stringId >= ttf_namemax || entry.value.isEmpty()) {
                continue;
            }
            const QByteArray valBytes = entry.value.toUtf8();
            if (entry.langId == 0x409 && dummy.names[entry.stringId] != nullptr &&
                std::strcmp(valBytes.constData(), dummy.names[entry.stringId]) == 0) {
                continue;
            }
            struct ttflangname* target = nullptr;
            for (struct ttflangname* cur = sf->names; cur != nullptr; cur = cur->next) {
                if (cur->lang == entry.langId) {
                    target = cur;
                    break;
                }
            }
            if (target == nullptr) {
                target = static_cast<struct ttflangname*>(chunkalloc(sizeof(struct ttflangname)));
                target->lang = entry.langId;
                target->next = sf->names;
                sf->names = target;
            }
            free(target->names[entry.stringId]);
            target->names[entry.stringId] = copy(valBytes.constData());
        }

        for (int i = 0; i < ttf_namemax; ++i) {
            free(dummy.names[i]);
        }
    }

    sf->changed = true;

    if (outSummary != nullptr) {
        *outSummary = extractFontSummary(sf, m_impl->fv, m_impl->currentPath);
    }
    return true;
}

bool FontForgeBridge::validateFont(bool force, FontValidationReport* outReport, QString* outError) {
    if (!hasOpenFont()) {
        if (outError) {
            *outError = QStringLiteral("No font is currently loaded.");
        }
        return false;
    }

    SplineFont* sf = m_impl->sf;
    const int activeLayer =
        (m_impl->fv && m_impl->fv->active_layer >= 0) ? m_impl->fv->active_layer : ly_fore;

    const int result = SFValidate(sf, activeLayer, force ? 1 : 0);
    if (result == -1) {
        if (outError) {
            *outError = QStringLiteral("Validation was cancelled.");
        }
        return false;
    }

    FontValidationReport report;
    report.combinedFlags = static_cast<uint32_t>(result);

    for (int gid = 0; gid < sf->glyphcnt; ++gid) {
        const SplineChar* sc = sf->glyphs[gid];
        if (sc == nullptr) {
            continue;
        }
        ++report.totalGlyphs;
        if (activeLayer < 0 || activeLayer >= sc->layer_cnt) {
            continue;
        }
        uint32_t state = sc->layers[activeLayer].validation_state & ~static_cast<uint32_t>(vs_known);
        if (sc->unlink_rm_ovrlp_save_undo) {
            state &= ~static_cast<uint32_t>(vs_selfintersects);
        }
        if (state != 0) {
            GlyphValidationIssue issue;
            issue.gid = gid;
            issue.unicode = sc->unicodeenc;
            issue.glyphName = qstr(sc->name);
            issue.flags = state;
            issue.descriptions = describeValidationFlags(state);
            if (issue.descriptions.isEmpty()) {
                issue.descriptions.append(
                    QStringLiteral("Validation flags: 0x%1").arg(state, 0, 16));
            }
            report.issues.append(std::move(issue));
        }
    }

    report.issueGlyphCount = report.issues.size();
    if (outReport != nullptr) {
        *outReport = std::move(report);
    }
    return true;
}

void FontForgeBridge::closeFont() {
    if (m_impl->fv != nullptr) {
        FontViewClose(m_impl->fv);
        m_impl->fv = nullptr;
        m_impl->sf = nullptr;
    } else if (m_impl->sf != nullptr) {
        SplineFontFree(m_impl->sf);
        m_impl->sf = nullptr;
    }
    m_impl->currentPath.clear();
}

bool FontForgeBridge::hasOpenFont() const {
    return m_impl->sf != nullptr && m_impl->fv != nullptr;
}

QStringList FontForgeBridge::availableSfntStringIds() {
    QStringList ids;
    ids.reserve(ttf_namemax);
    for (int i = 0; i < ttf_namemax; ++i) {
        ids.append(sfntStringIdName(i));
    }
    return ids;
}

QVector<QPair<int, QString>> FontForgeBridge::commonSfntLanguages() {
    static constexpr std::array<int, 12> kLangIds = {
        0x0409, // English (US)
        0x0809, // English (UK)
        0x0407, // German (Standard)
        0x0C07, // German (Austria)
        0x0807, // German (Switzerland)
        0x040C, // French (Standard)
        0x040A, // Spanish
        0x0410, // Italian
        0x0413, // Dutch
        0x0411, // Japanese
        0x0804, // Chinese (PRC)
        0x0419, // Russian
    };
    QVector<QPair<int, QString>> out;
    out.reserve(static_cast<qsizetype>(kLangIds.size()));
    for (int id : kLangIds) {
        out.append({id, sfntLanguageName(id)});
    }
    return out;
}

} // namespace ffgui
