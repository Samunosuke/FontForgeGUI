#include "backend/FontForgeBridge.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QTemporaryDir>
#include <cstdlib>
#include <iostream>

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            std::cerr << "CHECK failed at " << __FILE__ << ":" << __LINE__ << ": " << #expr        \
                      << "\n";                                                                     \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    ffgui::FontForgeBridge bridge;
    bridge.setLogCallback([](const QString& level, const QString& msg) {
        std::cerr << "[" << level.toStdString() << "] " << msg.toStdString() << "\n";
    });
    bridge.initialize();

    const QString sfdPath = QStringLiteral(TEST_FONTS_DIR "/Ambrosia.sfd");
    CHECK(QFileInfo::exists(sfdPath));

    ffgui::FontDocumentSnapshot snap;
    QString err;
    CHECK(bridge.openFont(sfdPath, &snap, &err));

    CHECK(bridge.hasOpenFont());
    CHECK(!snap.glyphs.isEmpty());
    CHECK(snap.summary.emSize > 0);

    int glyphsWithOutline = 0;
    int encodedGlyphs = 0;
    for (const ffgui::GlyphSummary& g : snap.glyphs) {
        if (g.hasOutline) {
            ++glyphsWithOutline;
        }
        if (g.unicode >= 0) {
            ++encodedGlyphs;
        }
    }
    CHECK(glyphsWithOutline > 0);
    CHECK(encodedGlyphs > 0);

    QTemporaryDir tmpDir;
    CHECK(tmpDir.isValid());

    const QString outOtf = tmpDir.filePath(QStringLiteral("test_export.otf"));
    const QString outTtf = tmpDir.filePath(QStringLiteral("test_export.ttf"));
    const QString outSfd = tmpDir.filePath(QStringLiteral("test_export.sfd"));

    ffgui::ExportOptions otfOpts;
    otfOpts.outputPath = outOtf;
    CHECK(bridge.exportFont(otfOpts, &err));
    CHECK(QFileInfo::exists(outOtf));
    CHECK(QFileInfo(outOtf).size() > 0);

    ffgui::ExportOptions ttfOpts;
    ttfOpts.outputPath = outTtf;
    CHECK(bridge.exportFont(ttfOpts, &err));
    CHECK(QFileInfo::exists(outTtf));
    CHECK(QFileInfo(outTtf).size() > 0);

    ffgui::ExportOptions sfdOpts;
    sfdOpts.outputPath = outSfd;
    CHECK(bridge.exportFont(sfdOpts, &err));
    CHECK(QFileInfo::exists(outSfd));
    CHECK(QFileInfo(outSfd).size() > 0);

    bridge.closeFont();
    CHECK(!bridge.hasOpenFont());

    // Round-trip re-open exported OTF and TTF
    ffgui::FontDocumentSnapshot otfSnap;
    CHECK(bridge.openFont(outOtf, &otfSnap, &err));
    CHECK(!otfSnap.glyphs.isEmpty());
    bridge.closeFont();

    ffgui::FontDocumentSnapshot ttfSnap;
    CHECK(bridge.openFont(outTtf, &ttfSnap, &err));
    CHECK(!ttfSnap.glyphs.isEmpty());
    CHECK(ttfSnap.summary.isOrder2);
    bridge.closeFont();

    // Also test WOFF2 load
    const QString woff2Path = QStringLiteral(TEST_FONTS_DIR "/CantarellMin.woff2");
    if (QFileInfo::exists(woff2Path)) {
        ffgui::FontDocumentSnapshot woff2Snap;
        CHECK(bridge.openFont(woff2Path, &woff2Snap, &err));
        CHECK(!woff2Snap.glyphs.isEmpty());
        bridge.closeFont();
    }

    std::cout << "test_backend_smoke passed (glyphs=" << snap.glyphs.size()
              << ", outlines=" << glyphsWithOutline << ")\n";
    return 0;
}
