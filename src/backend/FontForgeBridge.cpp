#include "backend/FontForgeBridge.hpp"

#include <QByteArray>
#include <QFileInfo>
#include <QTransform>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

// FontForge C headers are isolated inside this translation unit to avoid macro/typedef
// collisions (e.g. real, Color, Point) with Qt headers.
extern "C" {
#include <fontforge-config.h>
#include "baseviews.h"
#include "savefont.h"
#include "sfd.h"
#include "splinechar.h"
#include "splinefont.h"
#include "splineutil.h"
#include "start.h"
#include "uiinterface.h"
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

FontDocumentSnapshot buildSnapshot(SplineFont* sf, FontViewBase* fv, const QString& path) {
    FontDocumentSnapshot snap;
    FontSummary& fs = snap.summary;
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

    const int activeLayer = (fv != nullptr && fv->active_layer >= 0) ? fv->active_layer : ly_fore;
    if (sf->grid.order2 || (sf->glyphcnt > 0 && sf->glyphs[0] != nullptr &&
                            activeLayer < sf->glyphs[0]->layer_cnt &&
                            sf->glyphs[0]->layers[activeLayer].order2)) {
        fs.isOrder2 = true;
    }

    snap.glyphs.reserve(sf->glyphcnt);
    for (int gid = 0; gid < sf->glyphcnt; ++gid) {
        const SplineChar* sc = sf->glyphs[gid];
        if (sc == nullptr) {
            continue;
        }
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

        appendLayerOutlines(sc, activeLayer, QTransform(), gs.outline, gs.contourCount,
                            gs.pointCount, gs.referenceCount, 0);
        gs.hasOutline = !gs.outline.isEmpty();
        if (gs.hasOutline) {
            gs.bounds = gs.outline.boundingRect();
        }
        snap.glyphs.append(std::move(gs));
    }

    fs.glyphCount = snap.glyphs.size();
    return snap;
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
    if (options.opentypeTables) {
        // In FontForge's GenerateScript/fmflag2ttfflag, setting ONLY fm_flag_apple
        // turns on Apple mode; setting neither defaults to OpenType; setting
        // fm_flag_opentype sets both applemode+otmode. Keep clean OpenType default (0),
        // or explicitly set fm_flag_opentype when requested? Look at savefont.c:1054-1066:
        // case 0x00 => ttf_flag_otmode! Wait: for PS/PFB fm_flag_afm controls AFM.
    } else {
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

} // namespace ffgui
