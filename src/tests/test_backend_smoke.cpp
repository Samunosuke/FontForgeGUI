#include "backend/FontForgeBridge.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <cassert>
#include <iostream>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    ffgui::FontForgeBridge bridge;
    bridge.setLogCallback([](const QString& level, const QString& msg) {
        std::cerr << "[" << level.toStdString() << "] " << msg.toStdString() << "\n";
    });
    bridge.initialize();

    const QString sfdPath = QStringLiteral(TEST_FONTS_DIR "/Ambrosia.sfd");
    assert(QFileInfo::exists(sfdPath));

    ffgui::FontDocumentSnapshot snap;
    QString err;
    const bool opened = bridge.openFont(sfdPath, &snap, &err);
    if (!opened) {
        std::cerr << "openFont(Ambrosia.sfd) failed: " << err.toStdString() << "\n";
        return 1;
    }

    assert(bridge.hasOpenFont());
    assert(!snap.glyphs.isEmpty());
    assert(snap.summary.emSize > 0);

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
    assert(glyphsWithOutline > 0);
    assert(encodedGlyphs > 0);

    QTemporaryDir tmpDir;
    assert(tmpDir.isValid());

    const QString outOtf = tmpDir.filePath(QStringLiteral("test_export.otf"));
    const QString outTtf = tmpDir.filePath(QStringLiteral("test_export.ttf"));
    const QString outSfd = tmpDir.filePath(QStringLiteral("test_export.sfd"));

    ffgui::ExportOptions otfOpts;
    otfOpts.outputPath = outOtf;
    assert(bridge.exportFont(otfOpts, &err));
    assert(QFileInfo::exists(outOtf));
    assert(QFileInfo(outOtf).size() > 0);

    ffgui::ExportOptions ttfOpts;
    ttfOpts.outputPath = outTtf;
    assert(bridge.exportFont(ttfOpts, &err));
    assert(QFileInfo::exists(outTtf));
    assert(QFileInfo(outTtf).size() > 0);

    ffgui::ExportOptions sfdOpts;
    sfdOpts.outputPath = outSfd;
    assert(bridge.exportFont(sfdOpts, &err));
    assert(QFileInfo::exists(outSfd));
    assert(QFileInfo(outSfd).size() > 0);

    bridge.closeFont();
    assert(!bridge.hasOpenFont());

    // Round-trip re-open exported OTF and TTF
    ffgui::FontDocumentSnapshot otfSnap;
    assert(bridge.openFont(outOtf, &otfSnap, &err));
    assert(!otfSnap.glyphs.isEmpty());
    bridge.closeFont();

    ffgui::FontDocumentSnapshot ttfSnap;
    assert(bridge.openFont(outTtf, &ttfSnap, &err));
    assert(!ttfSnap.glyphs.isEmpty());
    assert(ttfSnap.summary.isOrder2);
    bridge.closeFont();

    // Also test WOFF2 load
    const QString woff2Path = QStringLiteral(TEST_FONTS_DIR "/CantarellMin.woff2");
    if (QFileInfo::exists(woff2Path)) {
        ffgui::FontDocumentSnapshot woff2Snap;
        assert(bridge.openFont(woff2Path, &woff2Snap, &err));
        assert(!woff2Snap.glyphs.isEmpty());
        bridge.closeFont();
    }

    std::cout << "test_backend_smoke passed (glyphs=" << snap.glyphs.size()
              << ", outlines=" << glyphsWithOutline << ")\n";
    return 0;
}

