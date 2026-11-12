#include "backend/FontForgeBridge.hpp"
#include "model/BitmapTracer.hpp"
#include "model/SvgSanitizer.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#define CHECK(expr)                                                                     \
    do {                                                                                \
        if (!(expr)) {                                                                  \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__,     \
                         #expr);                                                        \
            return 1;                                                                   \
        }                                                                               \
    } while (0)

using namespace ffgui;

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    const QString fontsDir = QStringLiteral(TEST_FONTS_DIR);
    const QString srcFontPath = QDir(fontsDir).filePath(QStringLiteral("Ambrosia.sfd"));
    CHECK(QFileInfo::exists(srcFontPath));

    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());

    // -------------------------------------------------------------------------
    // 1. In-Place Save (Ctrl+S workflow) test
    // -------------------------------------------------------------------------
    const QString savedSfdPath = QDir(tempDir.path()).filePath(QStringLiteral("EditableCopy.sfd"));
    CHECK(QFile::copy(srcFontPath, savedSfdPath));

    FontForgeBridge bridge;
    bridge.initialize();

    FontDocumentSnapshot snap;
    QString err;
    CHECK(bridge.openFont(savedSfdPath, &snap, &err));
    CHECK(snap.glyphs.size() > 0);

    CreateGlyphRequest createReq;
    createReq.name = QStringLiteral("icon_v04_save");
    createReq.unicode = 0xE040;
    createReq.advanceWidth = 1000;
    CreateGlyphResult createRes;
    CHECK(bridge.createGlyph(createReq, &createRes, &err));
    int targetGid = createRes.glyph.gid;
    CHECK(targetGid >= 0);

    // Save in-place to the exact same path that is currently open
    ExportOptions saveOpts;
    saveOpts.outputPath = savedSfdPath;
    CHECK(bridge.exportFont(saveOpts, &err));
    CHECK(QFileInfo::exists(savedSfdPath));

    // Re-open the saved file and verify the newly created glyph was persisted
    FontDocumentSnapshot reloadedSnap;
    CHECK(bridge.openFont(savedSfdPath, &reloadedSnap, &err));
    bool foundSavedGlyph = false;
    for (const GlyphSummary& g : reloadedSnap.glyphs) {
        if (g.name == QStringLiteral("icon_v04_save") && g.unicode == 0xE040) {
            foundSavedGlyph = true;
            targetGid = g.gid;
            break;
        }
    }
    CHECK(foundSavedGlyph);

    // -------------------------------------------------------------------------
    // 2. Auto Aspect-Ratio Positioning (Square/Tall vs. Wide Icons)
    // -------------------------------------------------------------------------
    const int ascent = reloadedSnap.summary.ascent;
    const int descent = reloadedSnap.summary.descent;
    const double glyphWidth = 1000.0;

    // 2a. Square icon (W/H = 1.0 <= 1.25): should sit on baseline (y = 0) and be H-centered
    const QByteArray squareSvg = R"SVG(
        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="100" height="100">
            <rect x="20" y="20" width="60" height="60" fill="#000000"/>
        </svg>
    )SVG";
    const SvgSanitizeResult sanSquare = SvgSanitizer::sanitize(squareSvg);
    CHECK(sanSquare.valid);

    SvgImportOptions squareOpts;
    squareOpts.gid = targetGid;
    squareOpts.svgData = sanSquare.solidSvg;
    squareOpts.placementMode = GlyphPlacementMode::AutoAspectRatio;
    squareOpts.scaleToEm = true;
    squareOpts.roundToInt = true;

    SvgImportResult squareImport;
    CHECK(bridge.importSvgToGlyph(squareOpts, &squareImport, &err));
    CHECK(squareImport.glyph.hasOutline);
    // In Y-up font coordinates, bounds.top() is minY and bounds.bottom() is maxY
    CHECK(std::abs(squareImport.glyph.bounds.top() - 0.0) <= 2.0);
    CHECK(std::abs(squareImport.glyph.bounds.center().x() - glyphWidth * 0.5) <= 2.0);

    // 2b. Wide icon (W/H = 4.0 > 1.25): should be vertically centered at ascent/2 and H-centered
    const QByteArray wideSvg = R"SVG(
        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200" width="200" height="200">
            <rect x="20" y="80" width="160" height="40" fill="#000000"/>
        </svg>
    )SVG";
    const SvgSanitizeResult sanWide = SvgSanitizer::sanitize(wideSvg);
    CHECK(sanWide.valid);

    SvgImportOptions wideOpts;
    wideOpts.gid = targetGid;
    wideOpts.svgData = sanWide.solidSvg;
    wideOpts.placementMode = GlyphPlacementMode::AutoAspectRatio;
    wideOpts.scaleToEm = true;
    wideOpts.roundToInt = true;

    SvgImportResult wideImport;
    CHECK(bridge.importSvgToGlyph(wideOpts, &wideImport, &err));
    CHECK(wideImport.glyph.hasOutline);
    CHECK(std::abs(wideImport.glyph.bounds.center().x() - glyphWidth * 0.5) <= 2.0);
    CHECK(std::abs(wideImport.glyph.bounds.center().y() - ascent * 0.5) <= 2.0);
    CHECK(wideImport.glyph.bounds.top() > 50.0); // Elevated above baseline

    // -------------------------------------------------------------------------
    // 3. Manual Positioning (Scale % + Offset X / Offset Y)
    // -------------------------------------------------------------------------
    SvgImportOptions manualOpts = squareOpts;
    manualOpts.scaleFactor = 0.5;
    manualOpts.offsetX = 120.0;
    manualOpts.offsetY = -45.0;

    SvgImportResult manualImport;
    CHECK(bridge.importSvgToGlyph(manualOpts, &manualImport, &err));
    CHECK(manualImport.glyph.hasOutline);
    CHECK(std::abs(manualImport.glyph.bounds.width() - squareImport.glyph.bounds.width() * 0.5) <= 2.0);
    CHECK(std::abs(manualImport.glyph.bounds.center().x() - (glyphWidth * 0.5 + 120.0)) <= 2.0);
    CHECK(std::abs(manualImport.glyph.bounds.top() - (-45.0)) <= 2.0);

    // -------------------------------------------------------------------------
    // 4. PNG Contour Tracer (Transparent Donut Icon + Opaque Luminance Icon)
    // -------------------------------------------------------------------------
    QImage donutImg(128, 128, QImage::Format_ARGB32);
    donutImg.fill(Qt::transparent);
    {
        QPainter p(&donutImg);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 255));
        p.drawEllipse(QPoint(64, 64), 46, 46);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.drawEllipse(QPoint(64, 64), 20, 20);
    }

    BitmapTraceOptions traceOpts;
    traceOpts.useAlpha = true;
    traceOpts.threshold = 128;
    traceOpts.curveSmoothing = 0.75;

    const BitmapTraceResult traceRes = BitmapTracer::trace(donutImg, traceOpts);
    CHECK(traceRes.valid);
    CHECK(traceRes.usedAlphaChannel);
    CHECK(traceRes.totalContours == 2);
    CHECK(traceRes.holeContours == 1);
    CHECK(!traceRes.solidSvg.isEmpty());
    CHECK(!traceRes.cutoutSvg.isEmpty());

    SvgImportOptions pngImportOpts;
    pngImportOpts.gid = targetGid;
    pngImportOpts.svgData = traceRes.solidSvg;
    pngImportOpts.cutoutSvgData = traceRes.cutoutSvg;
    pngImportOpts.placementMode = GlyphPlacementMode::AutoAspectRatio;
    pngImportOpts.scaleToEm = true;

    SvgImportResult pngImportRes;
    CHECK(bridge.importSvgToGlyph(pngImportOpts, &pngImportRes, &err));
    CHECK(pngImportRes.importedContours == 2);
    CHECK(pngImportRes.glyph.contourCount == 2);
    CHECK(pngImportRes.glyph.hasOutline);

    // Opaque white background with black rectangle (auto-fallback to luminance)
    QImage opaqueImg(100, 100, QImage::Format_RGB32);
    opaqueImg.fill(Qt::white);
    {
        QPainter p(&opaqueImg);
        p.fillRect(QRect(20, 30, 60, 40), Qt::black);
    }
    const BitmapTraceResult opaqueTrace = BitmapTracer::trace(opaqueImg, traceOpts);
    CHECK(opaqueTrace.valid);
    CHECK(!opaqueTrace.usedAlphaChannel);
    CHECK(opaqueTrace.totalContours == 1);
    CHECK(opaqueTrace.holeContours == 0);

    bridge.closeFont();
    return 0;
}

