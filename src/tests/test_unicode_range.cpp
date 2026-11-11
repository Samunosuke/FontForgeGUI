#include "model/CustomGlyphAreaConfig.hpp"
#include "model/GlyphListModel.hpp"
#include "model/UnicodeRangeFilter.hpp"

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

int main() {
    // 1. Parse ranges
    QString err;
    const auto allFilter = ffgui::UnicodeRangeFilter::parse(QStringLiteral("*"), true, &err);
    CHECK(allFilter.isValid());
    CHECK(allFilter.isMatchAll());

    const auto rangeFilter =
        ffgui::UnicodeRangeFilter::parse(QStringLiteral("U+0041-005A, E000..E00F, 20AC"), false, &err);
    CHECK(rangeFilter.isValid());
    CHECK(!rangeFilter.isMatchAll());
    CHECK(rangeFilter.ranges().size() == 3);

    ffgui::GlyphSummary glyphA;
    glyphA.gid = 1;
    glyphA.unicode = 0x0041;
    glyphA.name = QStringLiteral("A");

    ffgui::GlyphSummary glyphLowerA;
    glyphLowerA.gid = 2;
    glyphLowerA.unicode = 0x0061;
    glyphLowerA.name = QStringLiteral("a");

    ffgui::GlyphSummary glyphIcon;
    glyphIcon.gid = 3;
    glyphIcon.unicode = 0xE005;
    glyphIcon.name = QStringLiteral("icon_home");

    ffgui::GlyphSummary glyphUnencoded;
    glyphUnencoded.gid = 4;
    glyphUnencoded.unicode = -1;
    glyphUnencoded.name = QStringLiteral("a.sc");

    ffgui::GlyphSummary glyphAlt;
    glyphAlt.gid = 5;
    glyphAlt.unicode = 0x0391; // Greek Alpha
    glyphAlt.altUnicodes = {0x0041};
    glyphAlt.name = QStringLiteral("Alpha");

    CHECK(rangeFilter.matches(glyphA));
    CHECK(!rangeFilter.matches(glyphLowerA));
    CHECK(rangeFilter.matches(glyphIcon));
    CHECK(!rangeFilter.matches(glyphUnencoded));
    CHECK(rangeFilter.matches(glyphAlt));

    // Invalid syntax checks
    const auto badOrder = ffgui::UnicodeRangeFilter::parse(QStringLiteral("007E-0020"), false, &err);
    CHECK(!badOrder.isValid());
    CHECK(!err.isEmpty());

    const auto badHex = ffgui::UnicodeRangeFilter::parse(QStringLiteral("ZZZZ"), false, &err);
    CHECK(!badHex.isValid());

    // 2. Sidecar config persistence
    QTemporaryDir tmpDir;
    CHECK(tmpDir.isValid());
    const QString fakeFont = tmpDir.filePath(QStringLiteral("DemoFont.otf"));

    ffgui::CustomGlyphAreaConfig cfg = ffgui::CustomGlyphAreaConfig::createDefault();
    cfg.areas.append({QStringLiteral("Symbols"), QStringLiteral("U+2000-206F"), true});
    cfg.activeIndex = 3;
    CHECK(cfg.saveForFont(fakeFont, &err));

    ffgui::CustomGlyphAreaConfig loaded;
    CHECK(ffgui::CustomGlyphAreaConfig::loadForFont(fakeFont, &loaded, &err));
    CHECK(loaded.activeIndex == 3);
    CHECK(loaded.areas.size() == cfg.areas.size());
    CHECK(loaded.areas[3].name == QStringLiteral("Symbols"));
    CHECK(loaded.areas[3].rangeSpec == QStringLiteral("U+2000-206F"));
    CHECK(loaded.areas[3].includeUnencoded == true);

    // 3. GlyphListModel filtering + search
    ffgui::FontDocumentSnapshot snap;
    snap.glyphs = {glyphA, glyphLowerA, glyphIcon, glyphUnencoded};

    ffgui::GlyphListModel model;
    model.setRangeFilter(rangeFilter);
    model.setDocument(snap);
    CHECK(model.rowCount() == 2); // A and icon_home

    model.setSearchText(QStringLiteral("#3"));
    CHECK(model.rowCount() == 1);
    CHECK(model.glyphAtRow(0)->gid == 3);

    model.setSearchText(QStringLiteral("U+0041"));
    CHECK(model.rowCount() == 1);
    CHECK(model.glyphAtRow(0)->gid == 1);

    std::cout << "test_unicode_range passed\n";
    return 0;
}
