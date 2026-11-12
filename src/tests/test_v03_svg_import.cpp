#include "backend/FontForgeBridge.hpp"
#include "model/GlyphMetadataUtils.hpp"
#include "model/SvgSanitizer.hpp"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
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

    // 1. Color normalization checks
    CHECK(SvgSanitizer::normalizeColor(QStringLiteral("#abc")) == QStringLiteral("#aabbcc"));
    CHECK(SvgSanitizer::normalizeColor(QStringLiteral("#FF0080")) == QStringLiteral("#ff0080"));
    CHECK(SvgSanitizer::normalizeColor(QStringLiteral("#ff000000")).isEmpty());
    CHECK(SvgSanitizer::normalizeColor(QStringLiteral("rgb(255, 0, 128)")) == QStringLiteral("#ff0080"));
    CHECK(SvgSanitizer::normalizeColor(QStringLiteral("rgba(10, 20, 30, 0)")).isEmpty());
    CHECK(SvgSanitizer::normalizeColor(QStringLiteral("white")) == QStringLiteral("#ffffff"));
    CHECK(SvgSanitizer::normalizeColor(QStringLiteral("none")).isEmpty());
    CHECK(SvgSanitizer::normalizeColor(QStringLiteral("url(#grad1)")).isEmpty());

    // 2. Inkscape-style SVG sanitization + color filtering
    const QByteArray inkscapeSvg = R"SVG(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg xmlns="http://www.w3.org/2000/svg"
     xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape"
     xmlns:sodipodi="http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd"
     viewBox="0 0 100 100" width="100" height="100"
     inkscape:version="1.3">
  <title>Test Icon</title>
  <sodipodi:namedview inkscape:zoom="1.0" />
  <metadata><rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#" /></metadata>
  <script>alert(1)</script>
  <rect x="0" y="0" width="100" height="100" style="fill:#ffffff;stroke:none" />
  <rect x="2" y="2" width="96" height="96" style="fill:none;stroke:#cccccc;stroke-width:1" />
  <g inkscape:groupmode="layer" inkscape:label="HiddenGuides" style="display:none">
    <path d="M 10,10 L 90,90" style="fill:#ff00ff" />
  </g>
  <g inkscape:groupmode="layer" inkscape:label="Artwork">
    <path id="dust" d="M 5,5 l 0.2,0.1 l -0.2,0.1 z" style="fill:#111111" />
    <circle id="badge" cx="80" cy="20" r="10" style="fill:#ff0000" />
    <path id="glyphA" d="M 20,85 L 50,15 L 80,85 L 65,85 L 58,68 L 42,68 L 35,85 Z M 46,56 L 54,56 L 50,40 Z"
          style="fill:#1e3a8a;fill-rule:evenodd;stroke:#94a3b8;stroke-width:2" />
  </g>
</svg>
)SVG";

    SvgFilterOptions discoveryOpts;
    const SvgSanitizeResult discovery = SvgSanitizer::sanitize(inkscapeSvg, discoveryOpts);
    CHECK(discovery.valid);
    CHECK(discovery.viewBox == QRectF(0, 0, 100, 100));
    CHECK(discovery.removedMetadataCount >= 4);
    CHECK(discovery.removedHiddenCount >= 3); // white bg rect + fill:none helper + hidden layer
    CHECK(discovery.removedTinyOrEmptyCount >= 1); // tiny dust path
    CHECK(discovery.detectedColors.contains(QStringLiteral("#ff0000")));
    CHECK(discovery.detectedColors.contains(QStringLiteral("#1e3a8a")));
    CHECK(!discovery.detectedColors.contains(QStringLiteral("#ff00ff"))); // hidden layer excluded
    CHECK(discovery.keptGeometryCount == 2);

    SvgFilterOptions filteredOpts = discoveryOpts;
    filteredOpts.excludedColors = {QStringLiteral("#ff0000")};
    const SvgSanitizeResult filtered = SvgSanitizer::sanitize(inkscapeSvg, filteredOpts);
    CHECK(filtered.valid);
    CHECK(filtered.keptGeometryCount == 1);
    CHECK(filtered.removedColorFilteredCount == 1);
    CHECK(filtered.cleanedSvg.contains("#000000"));
    CHECK(!filtered.cleanedSvg.contains("sodipodi:namedview"));
    CHECK(!filtered.cleanedSvg.contains("<script>"));
    CHECK(!filtered.cleanedSvg.contains("badge"));

    // 3. Backend SVG glyph import + merge + export roundtrip
    const QString ambrosia = QStringLiteral(TEST_FONTS_DIR "/Ambrosia.sfd");
    CHECK(QFileInfo::exists(ambrosia));

    FontForgeBridge bridge;
    bridge.initialize();

    FontDocumentSnapshot snap;
    QString err;
    CHECK(bridge.openFont(ambrosia, &snap, &err));

    int targetGid = -1;
    for (const GlyphSummary& g : snap.glyphs) {
        if (g.unicode == 0x0041) { // 'A'
            targetGid = g.gid;
            break;
        }
    }
    CHECK(targetGid >= 0);

    SvgImportOptions replaceOpts;
    replaceOpts.gid = targetGid;
    replaceOpts.svgData = filtered.cleanedSvg;
    replaceOpts.replaceExistingContours = true;
    replaceOpts.scaleToEm = true;
    replaceOpts.correctDirection = true;
    replaceOpts.roundToInt = true;
    replaceOpts.updateAdvanceWidth = true;
    replaceOpts.svgWidth = 100.0;
    replaceOpts.svgHeight = 100.0;

    SvgImportResult importRes;
    CHECK(bridge.importSvgToGlyph(replaceOpts, &importRes, &err));
    CHECK(importRes.importedContours == 2); // outer + inner triangle hole
    CHECK(importRes.glyph.hasOutline);
    CHECK(importRes.glyph.contourCount == 2);
    CHECK(importRes.glyph.width == snap.summary.emSize);

    // Merge another square contour into the same glyph
    const QByteArray extraSquare =
        R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><rect x="10" y="10" width="15" height="15" fill="#000000"/></svg>)SVG";
    const SvgSanitizeResult extraClean = SvgSanitizer::sanitize(extraSquare);
    CHECK(extraClean.valid);
    SvgImportOptions mergeOpts = replaceOpts;
    mergeOpts.svgData = extraClean.cleanedSvg;
    mergeOpts.replaceExistingContours = false;
    mergeOpts.updateAdvanceWidth = false;

    SvgImportResult mergedRes;
    CHECK(bridge.importSvgToGlyph(mergeOpts, &mergedRes, &err));
    CHECK(mergedRes.glyph.contourCount == 3);

    // Export to SFD and re-open to verify persistence
    QTemporaryDir tmpDir;
    CHECK(tmpDir.isValid());
    const QString outSfd = QDir(tmpDir.path()).filePath(QStringLiteral("svg_imported.sfd"));
    ExportOptions expOpts;
    expOpts.outputPath = outSfd;
    CHECK(bridge.exportFont(expOpts, &err));
    bridge.closeFont();

    FontDocumentSnapshot reopened;
    CHECK(bridge.openFont(outSfd, &reopened, &err));
    bool foundImportedA = false;
    for (const GlyphSummary& g : reopened.glyphs) {
        if (g.unicode == 0x0041) {
            CHECK(g.hasOutline);
            CHECK(g.contourCount == 3);
            CHECK(g.width == snap.summary.emSize);
            foundImportedA = true;
            break;
        }
    }
    CHECK(foundImportedA);

    // 4. White foreground cutout regression test (circle + stroke + white rounded bar rect)
    const QByteArray cutoutIconSvg = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">
  <circle cx="16" cy="16" r="14" fill="#dc2626" stroke="#b91c1c" stroke-width="1.3"/>
  <rect x="7.5" y="14" width="17" height="4" rx="2" fill="#ffffff"/>
</svg>
)SVG";
    SvgFilterOptions cutoutFilterOpts;
    cutoutFilterOpts.keepStrokes = true;
    cutoutFilterOpts.normalizeToBlackFill = true;
    cutoutFilterOpts.whiteAsCutout = true;
    const SvgSanitizeResult cutoutClean = SvgSanitizer::sanitize(cutoutIconSvg, cutoutFilterOpts);
    CHECK(cutoutClean.valid);
    CHECK(cutoutClean.removedHiddenCount == 0);
    CHECK(cutoutClean.detectedColors.contains(QStringLiteral("#dc2626")));
    CHECK(cutoutClean.detectedColors.contains(QStringLiteral("#ffffff")));
    CHECK(cutoutClean.keptGeometryCount == 2);
    CHECK(cutoutClean.cutoutGeometryCount == 1);
    CHECK(!cutoutClean.solidSvg.isEmpty());
    CHECK(!cutoutClean.cutoutSvg.isEmpty());

    SvgImportOptions cutoutImportOpts;
    cutoutImportOpts.gid = targetGid;
    cutoutImportOpts.svgData = cutoutClean.solidSvg;
    cutoutImportOpts.strokeSvgData = cutoutClean.strokeSvg;
    cutoutImportOpts.cutoutSvgData = cutoutClean.cutoutSvg;
    cutoutImportOpts.replaceExistingContours = true;
    cutoutImportOpts.scaleToEm = true;
    cutoutImportOpts.correctDirection = true;
    cutoutImportOpts.removeOverlap = true;
    cutoutImportOpts.roundToInt = true;

    SvgImportResult cutoutRes;
    CHECK(bridge.importSvgToGlyph(cutoutImportOpts, &cutoutRes, &err));
    CHECK(cutoutRes.glyph.hasOutline);
    CHECK(cutoutRes.glyph.contourCount == 2);

    // GlyphMetadataUtils unit checks
    int32_t parsedCp = -99;
    CHECK(parseSingleCodePoint(QStringLiteral("U+E001"), &parsedCp) && parsedCp == 0xE001);
    CHECK(parseSingleCodePoint(QStringLiteral("0xe002"), &parsedCp) && parsedCp == 0xE002);
    CHECK(parseSingleCodePoint(QStringLiteral(""), &parsedCp) && parsedCp == -1);
    CHECK(!parseSingleCodePoint(QStringLiteral("U+ZZZZ"), &parsedCp));
    CHECK(formatCodePoint(0xE001) == QStringLiteral("U+E001"));
    CHECK(suggestedGlyphName(0xE001) == QStringLiteral("uniE001"));
    QList<int32_t> parsedAlts;
    CHECK(parseAltUnicodesList(QStringLiteral("U+E002, 0xE003"), &parsedAlts, &err));
    CHECK(parsedAlts.size() == 2 && parsedAlts[0] == 0xE002 && parsedAlts[1] == 0xE003);

    // Create New Glyph (PUA U+E001 with custom name + alt Unicode + SVG import + export round-trip)
    CreateGlyphRequest newPuaReq;
    newPuaReq.name = QStringLiteral("icon_bl");
    newPuaReq.unicode = 0xE001;
    newPuaReq.altUnicodes = {0xE002};
    newPuaReq.advanceWidth = 1000;

    CreateGlyphResult createdPua;
    CHECK(bridge.createGlyph(newPuaReq, &createdPua, &err));
    CHECK(createdPua.glyph.gid >= 0);
    CHECK(createdPua.glyph.name == QStringLiteral("icon_bl"));
    CHECK(createdPua.glyph.unicode == 0xE001);
    CHECK(createdPua.glyph.altUnicodes == QList<int32_t>({0xE002}));
    CHECK(createdPua.glyph.width == 1000);
    CHECK(!createdPua.glyph.hasOutline);

    // Reject duplicate Unicode & duplicate name cleanly
    CreateGlyphRequest dupUniReq;
    dupUniReq.name = QStringLiteral("icon_dup");
    dupUniReq.unicode = 0xE001;
    CreateGlyphResult dupRes;
    CHECK(!bridge.createGlyph(dupUniReq, &dupRes, &err));

    CreateGlyphRequest dupNameReq;
    dupNameReq.name = QStringLiteral("icon_bl");
    dupNameReq.unicode = 0xE004;
    CHECK(!bridge.createGlyph(dupNameReq, &dupRes, &err));

    // Create unencoded glyph with auto/explicit name
    CreateGlyphRequest unencReq;
    unencReq.name = QStringLiteral("icon_unencoded");
    unencReq.unicode = -1;
    unencReq.advanceWidth = 800;
    CreateGlyphResult createdUnenc;
    CHECK(bridge.createGlyph(unencReq, &createdUnenc, &err));
    CHECK(createdUnenc.glyph.unicode == -1);
    CHECK(createdUnenc.glyph.name == QStringLiteral("icon_unencoded"));
    CHECK(createdUnenc.glyph.width == 800);

    // Create glyph with empty name -> auto-derived from Unicode
    CreateGlyphRequest autoNameReq;
    autoNameReq.name.clear();
    autoNameReq.unicode = 0xE010;
    autoNameReq.advanceWidth = 1000;
    CreateGlyphResult createdAuto;
    CHECK(bridge.createGlyph(autoNameReq, &createdAuto, &err));
    CHECK(createdAuto.glyph.name == QStringLiteral("uniE010"));
    CHECK(createdAuto.glyph.unicode == 0xE010);

    // Import cutout SVG directly into newly created PUA glyph and verify SFD round-trip
    cutoutImportOpts.gid = createdPua.glyph.gid;
    SvgImportResult newGlyphImportRes;
    CHECK(bridge.importSvgToGlyph(cutoutImportOpts, &newGlyphImportRes, &err));
    CHECK(newGlyphImportRes.glyph.hasOutline);
    CHECK(newGlyphImportRes.glyph.contourCount == 2);

    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());
    const QString exportedSfd = tempDir.filePath(QStringLiteral("created_glyph_roundtrip.sfd"));
    ExportOptions exportOpts;
    exportOpts.outputPath = exportedSfd;
    CHECK(bridge.exportFont(exportOpts, &err));
    bridge.closeFont();

    FontDocumentSnapshot reloadedSnap;
    CHECK(bridge.openFont(exportedSfd, &reloadedSnap, &err));
    bool foundCreatedIcon = false;
    for (const GlyphSummary& g : reloadedSnap.glyphs) {
        if (g.name == QStringLiteral("icon_bl")) {
            foundCreatedIcon = true;
            CHECK(g.unicode == 0xE001);
            CHECK(g.width == 1000);
            CHECK(g.hasOutline);
            CHECK(g.contourCount == 2);
            break;
        }
    }
    CHECK(foundCreatedIcon);

    bridge.closeFont();

    std::printf("v0.3 SVG filter + import test passed (%d -> %d contours)\n",
                importRes.importedContours, mergedRes.glyph.contourCount);
    return 0;
}

