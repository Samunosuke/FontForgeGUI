#include "backend/FontForgeBridge.hpp"
#include "model/BitmapTracer.hpp"
#include "model/GlyphMetadataUtils.hpp"
#include "model/SvgSanitizer.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTransform>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

// FontForge C headers are isolated inside this translation unit to avoid macro/typedef
// collisions (e.g. real, Color, Point) with Qt headers.
extern "C" {
#include <fontforge-config.h>
#include "baseviews.h"
#include "cvundoes.h"
#include "encoding.h"
#include "fontforge.h"
#include "fvfonts.h"
#include "lookups.h"
#include "namelist.h"
#include "savefont.h"
#include "sd.h"
#include "sfd.h"
#include "splinechar.h"
#include "splinefont.h"
#include "splineorder2.h"
#include "splineoverlap.h"
#include "splineutil.h"
#include "splineutil2.h"
#include "start.h"
#include "svg.h"
#include "tottf.h"
#include "uiinterface.h"
#include "ustring.h"
}

#ifdef vsnprintf
#undef vsnprintf
#endif

namespace ffgui {

namespace {

void assignAltUnicodes(SplineChar* sc, const QList<int32_t>& altUnicodes) {
    struct altuni* head = nullptr;
    struct altuni* tail = nullptr;
    for (int32_t alt : altUnicodes) {
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
}

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

bool FontForgeBridge::createNewFont(const NewFontOptions& options, FontDocumentSnapshot* outSnapshot,
                                    QString* outError) {
    initialize();
    closeFont();
    g_lastError.clear();

    SplineFont* sf = SplineFontNew();
    if (sf == nullptr) {
        if (outError) {
            *outError = QStringLiteral("Failed to allocate new blank font.");
        }
        return false;
    }
    sf->onlybitmaps = false;

    const QString fontName = options.fontName.trimmed().isEmpty() ? QStringLiteral("Untitled")
                                                                  : options.fontName.trimmed();
    const QString familyName = options.familyName.trimmed().isEmpty() ? fontName
                                                                      : options.familyName.trimmed();
    const QString fullName = options.fullName.trimmed().isEmpty() ? familyName
                                                                  : options.fullName.trimmed();
    const QString weight = options.weight.trimmed().isEmpty() ? QStringLiteral("Regular")
                                                              : options.weight.trimmed();

    replaceCString(&sf->fontname, fontName, false);
    replaceCString(&sf->familyname, familyName, false);
    replaceCString(&sf->fullname, fullName, false);
    replaceCString(&sf->weight, weight, false);
    replaceCString(&sf->version, options.version.trimmed().isEmpty() ? QStringLiteral("1.0")
                                                                    : options.version.trimmed(), true);
    if (!options.copyright.isEmpty()) {
        replaceCString(&sf->copyright, options.copyright, true);
    }

    const int ascent = options.ascent > 0 ? options.ascent : 800;
    const int descent = options.descent >= 0 ? options.descent : 200;
    sf->ascent = ascent;
    sf->descent = descent;

    FontViewBase* fv = FVAppend(_FontViewCreate(sf));
    if (fv == nullptr) {
        SplineFontFree(sf);
        if (outError) {
            *outError = QStringLiteral("Failed to create FontForge view for new font.");
        }
        return false;
    }

    m_impl->fv = fv;
    m_impl->sf = fv->sf ? fv->sf : sf;
    m_impl->currentPath.clear();

    if (outSnapshot != nullptr) {
        *outSnapshot = buildSnapshot(m_impl->sf, m_impl->fv, QString());
    }
    return true;
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

    const QFileInfo targetInfo(options.outputPath);
    const QFileInfo currentInfo(m_impl->currentPath);
    const bool isInPlaceSave = !m_impl->currentPath.isEmpty() && targetInfo.exists() &&
        (targetInfo.canonicalFilePath() == currentInfo.canonicalFilePath() ||
         targetInfo.absoluteFilePath() == currentInfo.absoluteFilePath());

    // When saving over the currently open font file (e.g. Ctrl+S), write to a temporary
    // sibling file with the same extension first so a generation failure never corrupts
    // the original file on disk.
    const bool isDirFormat = m_impl->sf->save_to_dir != 0 ||
        options.outputPath.endsWith(QStringLiteral(".ufo"), Qt::CaseInsensitive);
    const QString writePath = (isInPlaceSave && !isDirFormat)
        ? QDir(targetInfo.absolutePath())
              .filePath(QStringLiteral(".%1.tmp.%2")
                            .arg(targetInfo.completeBaseName(), targetInfo.suffix()))
        : options.outputPath;

    if (writePath != options.outputPath && QFile::exists(writePath)) {
        QFile::remove(writePath);
    }

    QByteArray outBytes = writePath.toUtf8();

    if (options.outputPath.endsWith(QStringLiteral(".sfd"), Qt::CaseInsensitive)) {
        const int ok = SFDWrite(outBytes.data(), m_impl->sf, m_impl->fv->map, m_impl->fv->normal,
                                m_impl->sf->save_to_dir);
        if (!ok) {
            if (writePath != options.outputPath) {
                QFile::remove(writePath);
            }
            if (outError) {
                *outError = g_lastError.isEmpty()
                                ? QStringLiteral("Failed to write SFD file: %1")
                                      .arg(options.outputPath)
                                : g_lastError;
            }
            return false;
        }
    } else {
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

        const int ok = GenerateScript(m_impl->sf, outBytes.data(), "", fmFlags, -1, nullptr,
                                      nullptr, exportMap, nullptr, layer);
        if (!ok) {
            if (writePath != options.outputPath) {
                QFile::remove(writePath);
            }
            if (outError) {
                *outError = g_lastError.isEmpty()
                                ? QStringLiteral("Failed to export font to: %1")
                                      .arg(options.outputPath)
                                : g_lastError;
            }
            return false;
        }
    }

    if (writePath != options.outputPath) {
        if (QFile::exists(options.outputPath) && !QFile::remove(options.outputPath)) {
            QFile::remove(writePath);
            if (outError) {
                *outError = QStringLiteral("Failed to replace existing font file: %1")
                                .arg(options.outputPath);
            }
            return false;
        }
        if (!QFile::rename(writePath, options.outputPath)) {
            if (outError) {
                *outError = QStringLiteral("Failed to finalize saved font file: %1")
                                .arg(options.outputPath);
            }
            return false;
        }
    }

    if (isInPlaceSave || m_impl->currentPath.isEmpty() || options.isSaveAs) {
        m_impl->currentPath = options.outputPath;
        replaceCString(&m_impl->sf->filename, options.outputPath, false);
        replaceCString(&m_impl->sf->origname, options.outputPath, false);
        m_impl->sf->changed = false;
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

    assignAltUnicodes(sc, update.altUnicodes);
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

bool FontForgeBridge::moveGlyphs(const MoveGlyphsRequest& request, MoveGlyphsResult* outResult,
                                 QString* outError) {
    if (!hasOpenFont()) {
        if (outError) {
            *outError = QStringLiteral("No font is currently loaded.");
        }
        return false;
    }
    if (request.gids.isEmpty()) {
        if (outError) {
            *outError = QStringLiteral("No glyphs selected to move.");
        }
        return false;
    }
    if (request.targetStartUnicode < 0 || request.targetStartUnicode > 0x10FFFF) {
        if (outError) {
            *outError = QStringLiteral("Target start Unicode must be between U+0000 and U+10FFFF.");
        }
        return false;
    }

    const int totalMove = request.gids.size();
    if (static_cast<int64_t>(request.targetStartUnicode) + totalMove - 1 > 0x10FFFF) {
        if (outError) {
            *outError = QStringLiteral("Target Unicode range exceeds maximum U+10FFFF.");
        }
        return false;
    }

    SplineFont* sf = m_impl->sf;
    const int activeLayer =
        (m_impl->fv && m_impl->fv->active_layer >= 0) ? m_impl->fv->active_layer : ly_fore;

    // Check which target slots are already occupied by glyphs that are NOT part of the moving set
    QSet<int> movingGidSet(request.gids.begin(), request.gids.end());
    QList<QPair<int32_t, int>> collisions; // (targetUnicode, occupiedGid)
    for (int i = 0; i < totalMove; ++i) {
        const int32_t targetCp = request.targetStartUnicode + i;
        for (int gid = 0; gid < sf->glyphcnt; ++gid) {
            const SplineChar* sc = sf->glyphs[gid];
            if (sc != nullptr && sc->unicodeenc == targetCp && !movingGidSet.contains(gid)) {
                collisions.append({targetCp, gid});
                break;
            }
        }
    }

    if (!collisions.isEmpty() && !request.overwriteOccupied) {
        if (outError) {
            QStringList conflictMsgs;
            for (const auto& c : collisions) {
                const SplineChar* occ = sf->glyphs[c.second];
                conflictMsgs.append(QStringLiteral("U+%1 (%2)")
                                        .arg(c.first, 4, 16, QLatin1Char('0'))
                                        .toUpper()
                                        .arg(occ ? qstr(occ->name) : QStringLiteral("#%1").arg(c.second)));
            }
            *outError = QStringLiteral("%1 target code point(s) already occupied: %2")
                            .arg(collisions.size())
                            .arg(conflictMsgs.join(QStringLiteral(", ")));
        }
        return false;
    }

    // If overwriteOccupied is true, unencode any non-moving glyphs currently using target slots
    if (request.overwriteOccupied) {
        for (const auto& c : collisions) {
            SplineChar* occ = sf->glyphs[c.second];
            if (occ != nullptr) {
                occ->unicodeenc = -1;
                occ->changed = true;
                SCRefreshTitles(occ);
            }
        }
    }

    MoveGlyphsResult result;
    result.movedCount = 0;

    for (int i = 0; i < totalMove; ++i) {
        const int gid = request.gids[i];
        if (gid < 0 || gid >= sf->glyphcnt || sf->glyphs[gid] == nullptr) {
            continue;
        }
        SplineChar* sc = sf->glyphs[gid];
        const int32_t targetCp = request.targetStartUnicode + i;
        sc->unicodeenc = targetCp;
        sc->changed = true;
        sf->changed = true;
        SCRefreshTitles(sc);

        for (FontViewBase* fvs = sf->fv; fvs != nullptr; fvs = fvs->nextsame) {
            if (fvs->map && fvs->map->enc && fvs->map->enc != &custom) {
                fvs->map->enc = &custom;
            }
        }

        if (activeLayer >= 0 && activeLayer < sc->layer_cnt) {
            sc->layers[activeLayer].validation_state = vs_unknown;
        }

        result.updatedGlyphs.append(extractGlyphSummary(sc, gid, activeLayer));
        ++result.movedCount;
    }

    GlyphHashFree(sf);
    if (outResult != nullptr) {
        *outResult = std::move(result);
    }
    return true;
}

bool FontForgeBridge::createGlyph(const CreateGlyphRequest& request, CreateGlyphResult* outResult,
                                  QString* outError) {
    if (!hasOpenFont()) {
        if (outError) {
            *outError = QStringLiteral("No font is currently loaded.");
        }
        return false;
    }

    if (request.unicode < -1 || request.unicode > 0x10FFFF) {
        if (outError) {
            *outError = QStringLiteral("Unicode code point must be -1 (unencoded) or U+0000..U+10FFFF.");
        }
        return false;
    }

    const QString trimmedName = request.name.trimmed();
    if (trimmedName.isEmpty() && request.unicode < 0) {
        if (outError) {
            *outError = QStringLiteral("Specify at least a glyph name or a Unicode code point.");
        }
        return false;
    }
    for (QChar c : trimmedName) {
        if (c.isSpace()) {
            if (outError) {
                *outError = QStringLiteral("Glyph name must not contain whitespace.");
            }
            return false;
        }
    }

    SplineFont* sf = m_impl->sf;
    if (request.unicode >= 0) {
        for (int gid = 0; gid < sf->glyphcnt; ++gid) {
            const SplineChar* existing = sf->glyphs[gid];
            if (existing != nullptr && existing->unicodeenc == request.unicode) {
                if (outError) {
                    *outError = QStringLiteral("Unicode U+%1 is already assigned to glyph #%2 (%3).")
                                    .arg(request.unicode, 4, 16, QLatin1Char('0'))
                                    .toUpper()
                                    .arg(gid)
                                    .arg(qstr(existing->name));
                }
                return false;
            }
        }
    }

    QByteArray nameBytes = trimmedName.toUtf8();
    if (nameBytes.isEmpty()) {
        char autoBuf[64]{};
        const char* stdName = StdGlyphName(autoBuf, request.unicode, sf->uni_interp, sf->for_new_glyphs);
        nameBytes = QByteArray(stdName != nullptr ? stdName : "glyph");
    }

    if (SplineChar* dup = SFHashName(sf, nameBytes.constData()); dup != nullptr) {
        if (outError) {
            *outError = QStringLiteral("A glyph named '%1' already exists (#%2).")
                            .arg(QString::fromUtf8(nameBytes))
                            .arg(dup->orig_pos);
        }
        return false;
    }

    SplineChar* sc = nullptr;
    EncMap* map = m_impl->fv->map;
    const int enc = SFFindSlot(sf, map, request.unicode, nameBytes.constData());
    const bool slotFree = (enc >= 0 && enc < map->enccount)
        && (map->map[enc] == -1 || map->map[enc] >= sf->glyphcnt || sf->glyphs[map->map[enc]] == nullptr);

    if (slotFree) {
        sc = SFMakeChar(sf, map, enc);
        if (sc != nullptr) {
            free(sc->name);
            sc->name = copy(nameBytes.constData());
            sc->unicodeenc = request.unicode;
            GlyphHashFree(sf);
            SFHashGlyph(sf, sc);
        }
    }

    if (sc == nullptr) {
        sc = SFSplineCharCreate(sf);
        if (sf->strokedfont) {
            sc->layers[ly_fore].dostroke = true;
            sc->layers[ly_fore].dofill = false;
        }
        sc->unicodeenc = request.unicode;
        sc->name = copy(nameBytes.constData());
        SFAddGlyphAndEncode(sf, sc, nullptr, -1);
    }

    if (request.advanceWidth >= 0) {
        sc->width = static_cast<int16_t>(std::clamp(request.advanceWidth, 0, 32767));
        sc->widthset = true;
    }
    assignAltUnicodes(sc, request.altUnicodes);
    SCRefreshTitles(sc);
    sc->changed = true;
    sf->changed = true;

    const int activeLayer =
        (m_impl->fv && m_impl->fv->active_layer >= 0) ? m_impl->fv->active_layer : ly_fore;
    if (activeLayer >= 0 && activeLayer < sc->layer_cnt) {
        sc->layers[activeLayer].validation_state = vs_unknown;
    }

    if (outResult != nullptr) {
        outResult->glyph = extractGlyphSummary(sc, sc->orig_pos, activeLayer);
        outResult->fontSummary = extractFontSummary(sf, m_impl->fv, m_impl->currentPath);
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

bool FontForgeBridge::importSvgToGlyph(const SvgImportOptions& options, SvgImportResult* outResult,
                                       QString* outError) {
    if (!hasOpenFont()) {
        if (outError) {
            *outError = QStringLiteral("No font is currently loaded.");
        }
        return false;
    }
    if (options.svgData.trimmed().isEmpty() && options.strokeSvgData.trimmed().isEmpty()) {
        if (outError) {
            *outError = QStringLiteral("SVG input buffer is empty.");
        }
        return false;
    }

    SplineFont* sf = m_impl->sf;
    if (options.gid < 0 || options.gid >= sf->glyphcnt || sf->glyphs[options.gid] == nullptr) {
        if (outError) {
            *outError = QStringLiteral("Target glyph GID %1 does not exist.").arg(options.gid);
        }
        return false;
    }

    SplineChar* sc = sf->glyphs[options.gid];
    const int activeLayer =
        (m_impl->fv && m_impl->fv->active_layer >= 0 && m_impl->fv->active_layer < sc->layer_cnt)
            ? m_impl->fv->active_layer
            : ly_fore;
    const int emSize = sf->ascent + sf->descent;

    ImportParams ip;
    InitImportParams(&ip);
    ip.scale = options.scaleToEm ? 1 : 0;
    ip.correct_direction = options.correctDirection ? 1 : 0;
    ip.simplify = options.simplify ? 1 : 0;
    ip.clip = 1;
    ip.dimensions = 0;

    auto interpretBuffer = [&](const QByteArray& rawXml) -> SplineSet* {
        QByteArray buf = rawXml;
        SplineSet* parsed = SplinePointListInterpretSVG(
            nullptr, buf.data(), static_cast<int>(buf.size()), emSize, sf->ascent,
            sf->strokedfont, &ip);
        if (parsed == nullptr) {
            return nullptr;
        }
        SplineSet* probe = parsed;
        while (probe != nullptr && probe->first != nullptr && probe->first->next == nullptr) {
            probe = probe->next;
        }
        if (probe != nullptr && probe->first != nullptr && probe->first->next != nullptr
            && probe->first->next->order2 != sc->layers[activeLayer].order2) {
            parsed = SplineSetsConvertOrder(parsed, sc->layers[activeLayer].order2);
        }
        return parsed;
    };

    SplineSet* fillSpl = !options.svgData.trimmed().isEmpty() ? interpretBuffer(options.svgData) : nullptr;
    SplineSet* strokeSpl = !options.strokeSvgData.trimmed().isEmpty()
        ? interpretBuffer(options.strokeSvgData)
        : nullptr;

    if (strokeSpl != nullptr) {
        // Stroke-only SVG outlines are skipped by FontForge's EntityCharCorrectDir and reflect
        // across Y; normalize outer/inner ring winding before merging with fills.
        int changed = 0;
        strokeSpl = SplineSetsCorrect(strokeSpl, &changed);
    }

    auto appendSpl = [](SplineSet* a, SplineSet* b) -> SplineSet* {
        if (a == nullptr) return b;
        if (b == nullptr) return a;
        SplineSet* t = a;
        while (t->next != nullptr) t = t->next;
        t->next = b;
        return a;
    };

    SplineSet* spl = appendSpl(fillSpl, strokeSpl);
    if (spl == nullptr) {
        if (outError) {
            *outError = QStringLiteral(
                "FontForge could not extract vector outlines from the filtered SVG.");
        }
        return false;
    }

    SplineSet* cutoutSpl = nullptr;
    if (!options.cutoutSvgData.trimmed().isEmpty()) {
        cutoutSpl = interpretBuffer(options.cutoutSvgData);
        if (cutoutSpl != nullptr) {
            if (options.removeOverlap) {
                cutoutSpl = SplineSetRemoveOverlap(sc, cutoutSpl, over_remove);
            }
            int changed = 0;
            cutoutSpl = SplineSetsCorrect(cutoutSpl, &changed);
            for (SplineSet* s = cutoutSpl; s != nullptr; s = s->next) {
                if (SplinePointListIsClockwise(s) == 1) {
                    SplineSetReverse(s);
                }
            }
        }
    }

    if (options.removeOverlap) {
        spl = SplineSetRemoveOverlap(sc, spl, over_remove);
    }
    spl = appendSpl(spl, cutoutSpl);

    if (options.updateAdvanceWidth && options.svgWidth > 0.0) {
        const double rawWidth =
            (options.scaleToEm && options.svgHeight > 0.0)
                ? options.svgWidth * (static_cast<double>(emSize) / options.svgHeight)
                : options.svgWidth;
        sc->width = static_cast<int16_t>(std::clamp<long>(std::lround(rawWidth), 0L, 32767L));
        sc->widthset = true;
    }

    // Apply automatic aspect-ratio / baseline / center positioning and manual scale/offset
    if (spl != nullptr) {
        DBounds b{};
        SplineSetFindBounds(spl, &b);
        if (b.maxx > b.minx && b.maxy > b.miny) {
            const IconPlacementTransform pt = computeIconPlacement(
                b.minx, b.maxx, b.miny, b.maxy, static_cast<double>(sc->width), sf->ascent,
                sf->descent, options);
            if (std::abs(pt.scale - 1.0) > 1e-6 || std::abs(pt.tx) > 1e-6 ||
                std::abs(pt.ty) > 1e-6) {
                real transform[6] = {
                    static_cast<real>(pt.scale), 0.0, 0.0, static_cast<real>(pt.scale),
                    static_cast<real>(pt.tx),    static_cast<real>(pt.ty),
                };
                spl = SplinePointListTransform(spl, transform, tpt_AllPoints);
            }
        }
    }

    int importedContours = 0;
    SplineSet* tail = nullptr;
    for (SplineSet* s = spl; s != nullptr; s = s->next) {
        ++importedContours;
        tail = s;
    }

    SCPreserveLayer(sc, activeLayer, false);
    SplineSet** head = &sc->layers[activeLayer].splines;
    if (options.replaceExistingContours) {
        SplinePointListsFree(*head);
        *head = nullptr;
    }
    if (tail != nullptr) {
        tail->next = *head;
        *head = spl;
    }

    if (options.removeOverlap && !options.replaceExistingContours && cutoutSpl == nullptr && *head != nullptr) {
        *head = SplineSetRemoveOverlap(sc, *head, over_remove);
    }
    if (options.correctDirection && *head != nullptr) {
        int changed = 0;
        *head = SplineSetsCorrect(*head, &changed);
    }
    if (options.roundToInt) {
        SCRound2Int(sc, activeLayer, 1.0);
    }

    sc->layers[activeLayer].validation_state = vs_unknown;
    sc->changed = true;
    sf->changed = true;
    SCCharChangedUpdate(sc, activeLayer);

    if (outResult != nullptr) {
        outResult->glyph = extractGlyphSummary(sc, options.gid, activeLayer);
        outResult->importedContours = importedContours;
    }
    return true;
}

bool FontForgeBridge::batchImport(const BatchImportJob& job, BatchImportResult* outResult,
                                  QString* outError) {
    if (!hasOpenFont()) {
        if (outError) {
            *outError = QStringLiteral("No font is currently loaded.");
        }
        return false;
    }
    if (job.items.isEmpty()) {
        if (outError) {
            *outError = QStringLiteral("Batch import job contains no items.");
        }
        return false;
    }

    SplineFont* sf = m_impl->sf;
    const int totalItems = job.items.size();
    BatchImportResult res;

    for (int i = 0; i < totalItems; ++i) {
        const BatchImportItem& item = job.items[i];
        if (m_impl->progressCb) {
            m_impl->progressCb(i, totalItems,
                               QStringLiteral("Importing %1 (%2/%3)")
                                   .arg(QFileInfo(item.filePath).fileName())
                                   .arg(i + 1)
                                   .arg(totalItems));
        }

        const QFileInfo fi(item.filePath);
        if (!fi.exists() || !fi.isFile()) {
            res.errors.append(QStringLiteral("File not found: %1").arg(item.filePath));
            ++res.skippedCount;
            continue;
        }

        const QString ext = fi.suffix().toLower();
        const bool isPng = (ext == QStringLiteral("png"));
        const bool isSvg = (ext == QStringLiteral("svg"));
        if (!isPng && !isSvg) {
            res.errors.append(QStringLiteral("Unsupported file extension: %1").arg(item.filePath));
            ++res.skippedCount;
            continue;
        }

        int32_t targetUnicode = item.unicode;
        QString glyphName = item.glyphName.trimmed();
        if (glyphName.isEmpty()) {
            glyphName = fi.completeBaseName().replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")),
                                                     QStringLiteral("_"));
        }

        // Determine or create target glyph
        int targetGid = -1;
        if (targetUnicode >= 0) {
            for (int gid = 0; gid < sf->glyphcnt; ++gid) {
                if (sf->glyphs[gid] != nullptr && sf->glyphs[gid]->unicodeenc == targetUnicode) {
                    targetGid = gid;
                    break;
                }
            }
        }
        if (targetGid == -1 && !glyphName.isEmpty()) {
            for (int gid = 0; gid < sf->glyphcnt; ++gid) {
                if (sf->glyphs[gid] != nullptr && sf->glyphs[gid]->name != nullptr &&
                    glyphName == QString::fromUtf8(sf->glyphs[gid]->name)) {
                    targetGid = gid;
                    break;
                }
            }
        }

        if (targetGid >= 0 && !job.overwriteExisting) {
            res.errors.append(QStringLiteral("Code point / glyph already exists: %1 (U+%2)")
                                  .arg(glyphName)
                                  .arg(targetUnicode, 4, 16, QLatin1Char('0'))
                                  .toUpper());
            ++res.skippedCount;
            continue;
        }

        if (targetGid == -1) {
            CreateGlyphRequest createReq;
            createReq.name = glyphName;
            createReq.unicode = targetUnicode;
            createReq.advanceWidth = sf->ascent + sf->descent;
            CreateGlyphResult createRes;
            QString createErr;
            if (!createGlyph(createReq, &createRes, &createErr)) {
                res.errors.append(QStringLiteral("Failed to create glyph '%1': %2")
                                      .arg(glyphName, createErr));
                ++res.skippedCount;
                continue;
            }
            targetGid = createRes.glyph.gid;
        }

        SvgImportOptions importOpts = item.hasCustomOptions ? item.customOptions : job.defaultOptions;
        importOpts.gid = targetGid;

        if (isPng) {
            BitmapTraceOptions traceOpts;
            traceOpts.useAlpha = true;
            traceOpts.threshold = 128;
            traceOpts.curveSmoothing = 0.75;
            BitmapTraceResult traceRes = BitmapTracer::traceFile(item.filePath, traceOpts);
            if (!traceRes.valid || traceRes.totalContours == 0) {
                res.errors.append(QStringLiteral("Failed to trace PNG '%1': %2")
                                      .arg(item.filePath, traceRes.errorMessage));
                ++res.skippedCount;
                continue;
            }
            importOpts.svgData = traceRes.solidSvg;
            importOpts.cutoutSvgData = traceRes.cutoutSvg;
            importOpts.svgWidth = traceRes.viewBox.width();
            importOpts.svgHeight = traceRes.viewBox.height();
        } else {
            SvgFilterOptions filterOpts;
            filterOpts.stripEditorMetadata = true;
            filterOpts.removeHiddenElements = true;
            filterOpts.removeBackgroundRects = true;
            filterOpts.stripNoFillElements = true;
            filterOpts.keepStrokes = false;
            filterOpts.normalizeToBlackFill = true;
            filterOpts.whiteAsCutout = true;
            filterOpts.stripTinyPaths = true;
            SvgSanitizeResult sanRes = SvgSanitizer::sanitizeFile(item.filePath, filterOpts);
            if (!sanRes.valid || sanRes.keptGeometryCount == 0) {
                res.errors.append(QStringLiteral("Failed to parse SVG '%1': %2")
                                      .arg(item.filePath, sanRes.errorMessage));
                ++res.skippedCount;
                continue;
            }
            importOpts.svgData = !sanRes.solidSvg.isEmpty() ? sanRes.solidSvg : sanRes.cleanedSvg;
            importOpts.strokeSvgData = sanRes.strokeSvg;
            importOpts.cutoutSvgData = sanRes.cutoutSvg;
            importOpts.svgWidth = sanRes.width > 0.0 ? sanRes.width : sanRes.viewBox.width();
            importOpts.svgHeight = sanRes.height > 0.0 ? sanRes.height : sanRes.viewBox.height();
        }

        SvgImportResult importRes;
        QString importErr;
        if (!importSvgToGlyph(importOpts, &importRes, &importErr)) {
            res.errors.append(QStringLiteral("Failed to import '%1' into glyph: %2")
                                  .arg(item.filePath, importErr));
            ++res.skippedCount;
            continue;
        }

        res.updatedGlyphs.append(importRes.glyph);
        ++res.importedCount;
    }

    if (m_impl->progressCb) {
        m_impl->progressCb(totalItems, totalItems, QStringLiteral("Batch import completed."));
    }

    res.updatedFontSummary = extractFontSummary(sf, m_impl->fv, m_impl->currentPath);
    if (outResult != nullptr) {
        *outResult = std::move(res);
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

QString FontForgeBridge::currentFontPath() const {
    return m_impl->currentPath;
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
