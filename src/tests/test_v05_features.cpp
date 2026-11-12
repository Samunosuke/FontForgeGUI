#include "backend/FontForgeBridge.hpp"
#include "model/BatchImportDefinition.hpp"
#include "model/BitmapTracer.hpp"
#include "model/FontSettingsUtils.hpp"
#include "model/UnicodeRangeFilter.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>

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
    app.setOrganizationName(QStringLiteral("FontForgeGUI"));
    app.setApplicationName(QStringLiteral("FontForgeGUI"));

    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());

    FontForgeBridge bridge;
    bridge.initialize();

    // -------------------------------------------------------------------------
    // 1. Create New Blank Font & SPUA Range Checks
    // -------------------------------------------------------------------------
    NewFontOptions newOpts;
    newOpts.fontName = QStringLiteral("V05IconFont");
    newOpts.familyName = QStringLiteral("V05IconFont");
    newOpts.ascent = 800;
    newOpts.descent = 200;

    FontDocumentSnapshot newSnap;
    QString err;
    CHECK(bridge.createNewFont(newOpts, &newSnap, &err));
    CHECK(bridge.hasOpenFont());
    CHECK(bridge.currentFontPath().isEmpty());
    CHECK(newSnap.summary.fontName == QStringLiteral("V05IconFont"));
    CHECK(newSnap.summary.emSize == 1000);

    // Verify SPUA-A and SPUA-B filter presets
    const auto presets = UnicodeRangeFilter::defaultPresets();
    bool hasSpuaA = false;
    bool hasSpuaB = false;
    for (const auto& p : presets) {
        if (p.name.contains(QStringLiteral("SPUA-A"))) hasSpuaA = true;
        if (p.name.contains(QStringLiteral("SPUA-B"))) hasSpuaB = true;
    }
    CHECK(hasSpuaA);
    CHECK(hasSpuaB);

    QString filterErr;
    UnicodeRangeFilter spuaAFilter = UnicodeRangeFilter::parse(QStringLiteral("U+F0000-FFFFD"), false, &filterErr);
    CHECK(spuaAFilter.isValid());
    GlyphSummary dummySpuaGlyph;
    dummySpuaGlyph.unicode = 0xF0005;
    CHECK(spuaAFilter.matches(dummySpuaGlyph));

    // -------------------------------------------------------------------------
    // 2. Batch Import (SVGs & PNGs) into SPUA-A
    // -------------------------------------------------------------------------
    const QString svg1Path = QDir(tempDir.path()).filePath(QStringLiteral("icon1.svg"));
    const QString svg2Path = QDir(tempDir.path()).filePath(QStringLiteral("icon2.svg"));
    const QString png1Path = QDir(tempDir.path()).filePath(QStringLiteral("icon3.png"));

    {
        QFile f1(svg1Path);
        CHECK(f1.open(QIODevice::WriteOnly));
        f1.write(R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><rect x="10" y="10" width="80" height="80" fill="#000"/></svg>)SVG");

        QFile f2(svg2Path);
        CHECK(f2.open(QIODevice::WriteOnly));
        f2.write(R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><circle cx="50" cy="50" r="40" fill="#000"/></svg>)SVG");

        QImage img(64, 64, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.fillRect(QRect(16, 16, 32, 32), Qt::black);
        CHECK(img.save(png1Path));
    }

    BatchImportJob job;
    job.overwriteExisting = true;
    job.defaultOptions.placementMode = GlyphPlacementMode::AutoAspectRatio;
    job.defaultOptions.scaleToEm = true;

    BatchImportItem item1; item1.filePath = svg1Path; item1.unicode = 0xF0001; item1.glyphName = QStringLiteral("square_icon");
    BatchImportItem item2; item2.filePath = svg2Path; item2.unicode = 0xF0002; item2.glyphName = QStringLiteral("circle_icon");
    BatchImportItem item3; item3.filePath = png1Path; item3.unicode = 0xF0003; item3.glyphName = QStringLiteral("png_icon");
    job.items = {item1, item2, item3};

    BatchImportResult bRes;
    CHECK(bridge.batchImport(job, &bRes, &err));
    CHECK(bRes.importedCount == 3);
    CHECK(bRes.skippedCount == 0);
    CHECK(!bRes.snapshot.glyphs.isEmpty());
    CHECK(bRes.snapshot.glyphs.size() >= 3);

    // Save and re-load to verify persistence
    const QString fontOutPath = QDir(tempDir.path()).filePath(QStringLiteral("test_v05.sfd"));
    ExportOptions expOpts;
    expOpts.outputPath = fontOutPath;
    CHECK(bridge.exportFont(expOpts, &err));
    CHECK(bridge.currentFontPath() == fontOutPath);

    // Verify in-place re-save works directly on the newly established path
    CHECK(bridge.exportFont(expOpts, &err));

    FontDocumentSnapshot reloadedSnap;
    CHECK(bridge.openFont(fontOutPath, &reloadedSnap, &err));
    CHECK(reloadedSnap.glyphs.size() >= 3);

    // -------------------------------------------------------------------------
    // 3. Move Glyphs Function (with collision checking)
    // -------------------------------------------------------------------------
    QList<int> gidsToMove;
    for (const auto& g : reloadedSnap.glyphs) {
        if (g.unicode == 0xF0001 || g.unicode == 0xF0002) {
            gidsToMove.append(g.gid);
        }
    }
    CHECK(gidsToMove.size() == 2);

    // Move to U+F0002..U+F0003 without overwrite -> collision with 0xF0003 (png_icon)!
    MoveGlyphsRequest failMoveReq;
    failMoveReq.gids = gidsToMove;
    failMoveReq.targetStartUnicode = 0xF0002;
    failMoveReq.overwriteOccupied = false;
    MoveGlyphsResult failRes;
    CHECK(!bridge.moveGlyphs(failMoveReq, &failRes, &err));
    CHECK(err.contains(QStringLiteral("already occupied")));

    // Move to U+F0010 (all free)
    MoveGlyphsRequest okMoveReq;
    okMoveReq.gids = gidsToMove;
    okMoveReq.targetStartUnicode = 0xF0010;
    okMoveReq.overwriteOccupied = false;
    MoveGlyphsResult okRes;
    CHECK(bridge.moveGlyphs(okMoveReq, &okRes, &err));
    CHECK(okRes.movedCount == 2);
    CHECK(okRes.updatedGlyphs[0].unicode == 0xF0010);
    CHECK(okRes.updatedGlyphs[1].unicode == 0xF0011);
    CHECK(!okRes.snapshot.glyphs.isEmpty());
    CHECK(okRes.snapshot.glyphs.size() >= 3);

    // -------------------------------------------------------------------------
    // 4. Batch Import JSON Definition File Load/Save
    // -------------------------------------------------------------------------
    const QString jsonDefPath = QDir(tempDir.path()).filePath(QStringLiteral("batch_def.json"));
    CHECK(BatchImportDefinition::saveToFile(jsonDefPath, job, &err));
    CHECK(QFileInfo::exists(jsonDefPath));

    BatchImportJob loadedJob;
    CHECK(BatchImportDefinition::loadFromFile(jsonDefPath, &loadedJob, &err));
    CHECK(loadedJob.items.size() == 3);
    CHECK(loadedJob.items[0].unicode == 0xF0001);
    CHECK(loadedJob.items[1].glyphName == QStringLiteral("circle_icon"));

    // -------------------------------------------------------------------------
    // 5. Default Import Options Persistence via QSettings
    // -------------------------------------------------------------------------
    SvgImportOptions testDefaults;
    testDefaults.scaleFactor = 1.35;
    testDefaults.bitmapThreshold = 185;
    testDefaults.normalizeToBlackFill = false;
    saveDefaultImportOptions(testDefaults);

    SvgImportOptions loadedDefaults = loadDefaultImportOptions();
    CHECK(std::abs(loadedDefaults.scaleFactor - 1.35) < 1e-4);
    CHECK(loadedDefaults.bitmapThreshold == 185);
    CHECK(loadedDefaults.normalizeToBlackFill == false);
    resetDefaultImportOptions();

    // -------------------------------------------------------------------------
    // 6. Multicolored PNG Tracing & Color Distance (v0.5.2)
    // -------------------------------------------------------------------------
    // Test 6a: Create an opaque white image with a bright yellow square in the center.
    // In Luminance mode, yellow luma = ~226 which is > 128 threshold, so it would fail/invert.
    // In ColorDistance mode, distance from white = ~0.60 > 0.15, so it is detected as foreground!
    QImage yellowOnWhite(64, 64, QImage::Format_RGB32);
    yellowOnWhite.fill(qRgb(255, 255, 255));
    {
        QPainter p(&yellowOnWhite);
        p.fillRect(16, 16, 32, 32, QColor(255, 255, 0));
    }
    const QRgb detectedBg = BitmapTracer::detectBackgroundColor(yellowOnWhite);
    CHECK(qRed(detectedBg) == 255 && qGreen(detectedBg) == 255 && qBlue(detectedBg) == 255);

    BitmapTraceOptions colorDistOpts;
    colorDistOpts.binarizationMode = BitmapBinarizationMode::ColorDistance;
    colorDistOpts.autoDetectBgColor = true;
    colorDistOpts.colorTolerance = 0.15;
    const BitmapTraceResult yellowTrace = BitmapTracer::trace(yellowOnWhite, colorDistOpts);
    CHECK(yellowTrace.valid);
    CHECK(yellowTrace.totalContours >= 1);
    CHECK(yellowTrace.inkBounds.width() > 20.0);

    // Test 6b: Extract palette from multicolored image
    QImage multiColor(64, 64, QImage::Format_RGB32);
    multiColor.fill(qRgb(255, 255, 255));
    {
        QPainter p(&multiColor);
        p.fillRect(0, 0, 32, 32, QColor(255, 0, 0));
        p.fillRect(32, 0, 32, 32, QColor(0, 255, 0));
        p.fillRect(0, 32, 32, 32, QColor(0, 0, 255));
    }
    const QVector<QRgb> pal = BitmapTracer::extractPalette(multiColor, 8);
    CHECK(pal.size() >= 4); // White, Red, Green, Blue

    // Test 6c: Trace a real-world sample icon from sample_icons if present
    const QString sampleIconPath = QStringLiteral("src/sample_icons/appointment-new.png");
    if (QFileInfo::exists(sampleIconPath)) {
        BitmapTraceOptions sampleOpts;
        sampleOpts.binarizationMode = BitmapBinarizationMode::ColorDistance;
        sampleOpts.autoDetectBgColor = true;
        sampleOpts.colorTolerance = 0.15;
        const BitmapTraceResult sampleRes = BitmapTracer::traceFile(sampleIconPath, sampleOpts);
        CHECK(sampleRes.valid);
        CHECK(sampleRes.totalContours > 0);
    }

    // Test 6d: Persistence of v0.5.2 options
    SvgImportOptions v052Options;
    v052Options.binarizationMode = BitmapBinarizationMode::ColorDistance;
    v052Options.autoDetectBgColor = false;
    v052Options.customBgColor = QStringLiteral("#123456");
    v052Options.colorTolerance = 0.28;
    saveDefaultImportOptions(v052Options);

    SvgImportOptions loadedV052 = loadDefaultImportOptions();
    CHECK(loadedV052.binarizationMode == BitmapBinarizationMode::ColorDistance);
    CHECK(loadedV052.autoDetectBgColor == false);
    CHECK(loadedV052.customBgColor == QStringLiteral("#123456"));
    CHECK(std::abs(loadedV052.colorTolerance - 0.28) < 1e-4);
    resetDefaultImportOptions();

    bridge.closeFont();
    return 0;
}

