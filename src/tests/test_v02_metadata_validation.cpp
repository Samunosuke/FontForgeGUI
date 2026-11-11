#include "backend/FontForgeBridge.hpp"
#include "model/GlyphListModel.hpp"
#include "model/UnicodeRangeFilter.hpp"

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
    bridge.initialize();

    const QString sfdPath = QStringLiteral(TEST_FONTS_DIR "/Ambrosia.sfd");
    CHECK(QFileInfo::exists(sfdPath));

    ffgui::FontDocumentSnapshot snap;
    QString err;
    CHECK(bridge.openFont(sfdPath, &snap, &err));
    CHECK(!snap.glyphs.isEmpty());

    // Find an encoded glyph with an outline
    int targetIdx = -1;
    for (int i = 0; i < snap.glyphs.size(); ++i) {
        if (snap.glyphs[i].unicode == 0x0041) { // 'A'
            targetIdx = i;
            break;
        }
    }
    if (targetIdx < 0) {
        targetIdx = 0;
    }
    const int targetGid = snap.glyphs[targetIdx].gid;

    // 1. Validation check on invalid glyph metadata
    ffgui::GlyphMetadataUpdate badGlyph;
    badGlyph.gid = targetGid;
    badGlyph.name = QStringLiteral("invalid name");
    CHECK(!bridge.updateGlyphMetadata(badGlyph, nullptr, &err));
    CHECK(!err.isEmpty());

    // 2. Edit glyph name, primary unicode, and altUnicodes
    ffgui::GlyphMetadataUpdate glyphEdit;
    glyphEdit.gid = targetGid;
    glyphEdit.name = QStringLiteral("CustomGlyphA");
    glyphEdit.unicode = 0xE123;
    glyphEdit.altUnicodes = {0xE124, 0xE125};

    ffgui::GlyphSummary updatedGlyph;
    CHECK(bridge.updateGlyphMetadata(glyphEdit, &updatedGlyph, &err));
    CHECK(updatedGlyph.name == QStringLiteral("CustomGlyphA"));
    CHECK(updatedGlyph.unicode == 0xE123);
    CHECK(updatedGlyph.altUnicodes.size() == 2);

    // 3. Edit font metadata + SFNT name
    ffgui::FontMetadataUpdate fontEdit;
    fontEdit.fontName = QStringLiteral("TestEditedPS");
    fontEdit.familyName = QStringLiteral("TestEditedFamily");
    fontEdit.fullName = QStringLiteral("TestEditedFamily Regular");
    fontEdit.weight = QStringLiteral("Medium");
    fontEdit.version = QStringLiteral("2.001");
    fontEdit.copyright = QStringLiteral("Copyright 2026 FontForgeGUI");
    fontEdit.ascent = 820;
    fontEdit.descent = 180;
    fontEdit.italicAngle = -5.5;
    fontEdit.underlinePos = -120.0;
    fontEdit.underlineWidth = 45.0;
    fontEdit.updateSfntNames = true;
    fontEdit.sfntNames = {
        {0x0409, QString(), 8, QString(), QStringLiteral("FontForgeGUI Vendor")},
    };

    ffgui::FontSummary updatedSummary;
    CHECK(bridge.updateFontMetadata(fontEdit, &updatedSummary, &err));
    CHECK(updatedSummary.fontName == QStringLiteral("TestEditedPS"));
    CHECK(updatedSummary.familyName == QStringLiteral("TestEditedFamily"));
    CHECK(updatedSummary.ascent == 820);
    CHECK(updatedSummary.descent == 180);
    CHECK(updatedSummary.emSize == 1000);

    // Export to SFD and OTF and verify round-trip persistence
    QTemporaryDir tmpDir;
    CHECK(tmpDir.isValid());
    const QString outSfd = tmpDir.filePath(QStringLiteral("edited.sfd"));
    const QString outOtf = tmpDir.filePath(QStringLiteral("edited.otf"));

    ffgui::ExportOptions sfdOpts;
    sfdOpts.outputPath = outSfd;
    CHECK(bridge.exportFont(sfdOpts, &err));

    ffgui::ExportOptions otfOpts;
    otfOpts.outputPath = outOtf;
    CHECK(bridge.exportFont(otfOpts, &err));

    // 4. Validate font — create a duplicate name/unicode first to guarantee detection
    if (snap.glyphs.size() > 1) {
        const int secondGid =
            (snap.glyphs[0].gid == targetGid) ? snap.glyphs[1].gid : snap.glyphs[0].gid;
        ffgui::GlyphMetadataUpdate dupEdit;
        dupEdit.gid = secondGid;
        dupEdit.name = QStringLiteral("CustomGlyphA");
        dupEdit.unicode = 0xE123;
        CHECK(bridge.updateGlyphMetadata(dupEdit, nullptr, &err));
    }

    ffgui::FontValidationReport report;
    CHECK(bridge.validateFont(true, &report, &err));
    CHECK(report.totalGlyphs > 0);
    CHECK(report.issueGlyphCount > 0);
    bool sawDupOrIssue = false;
    for (const auto& issue : report.issues) {
        if (issue.gid == targetGid && !issue.descriptions.isEmpty()) {
            sawDupOrIssue = true;
            break;
        }
    }
    CHECK(sawDupOrIssue);
    bridge.closeFont();

    // Re-open exported SFD and check edited glyph & font metadata
    ffgui::FontDocumentSnapshot sfdSnap;
    CHECK(bridge.openFont(outSfd, &sfdSnap, &err));
    CHECK(sfdSnap.summary.fontName == QStringLiteral("TestEditedPS"));
    CHECK(sfdSnap.summary.familyName == QStringLiteral("TestEditedFamily"));
    CHECK(sfdSnap.summary.ascent == 820);
    CHECK(sfdSnap.summary.descent == 180);

    bool foundEditedInSfd = false;
    for (const auto& g : sfdSnap.glyphs) {
        if (g.name == QStringLiteral("CustomGlyphA") && g.unicode == 0xE123) {
            foundEditedInSfd = true;
            CHECK(g.altUnicodes.contains(0xE124));
            CHECK(g.altUnicodes.contains(0xE125));
            break;
        }
    }
    CHECK(foundEditedInSfd);
    bridge.closeFont();

    // Re-open exported OTF and check font metadata & encoded glyph
    ffgui::FontDocumentSnapshot otfSnap;
    CHECK(bridge.openFont(outOtf, &otfSnap, &err));
    CHECK(otfSnap.summary.fontName == QStringLiteral("TestEditedPS"));
    CHECK(otfSnap.summary.familyName == QStringLiteral("TestEditedFamily"));
    bool foundInOtf = false;
    for (const auto& g : otfSnap.glyphs) {
        if (g.unicode == 0xE123) {
            foundInOtf = true;
            break;
        }
    }
    CHECK(foundInOtf);
    bridge.closeFont();

    // 5. Verify GlyphListModel incremental update + filter re-evaluation
    ffgui::GlyphListModel model;
    model.setRangeFilter(ffgui::UnicodeRangeFilter::parse(QStringLiteral("U+E000-EFFF"), false));
    model.setDocument(snap);
    const int beforeCount = model.rowCount();
    CHECK(model.updateGlyph(updatedGlyph));
    CHECK(model.rowCount() == beforeCount + 1);

    std::cout << "test_v02_metadata_validation passed (checked=" << report.totalGlyphs
              << ", issues=" << report.issueGlyphCount << ")\n";
    return 0;
}
