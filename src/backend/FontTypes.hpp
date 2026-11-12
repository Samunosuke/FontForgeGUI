#pragma once

#include <QList>
#include <QPainterPath>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>

namespace ffgui {

struct SfntNameEntry {
    int langId = 0x0409;
    QString langName;
    int stringId = 0;
    QString stringIdName;
    QString value;
};

struct FontSummary {
    QString filePath;
    QString fontName;
    QString familyName;
    QString fullName;
    QString weight;
    QString version;
    QString copyright;
    int ascent = 800;
    int descent = 200;
    int emSize = 1000;
    double italicAngle = 0.0;
    double underlinePos = -100.0;
    double underlineWidth = 50.0;
    int glyphCount = 0;
    bool isOrder2 = false;
    QVector<SfntNameEntry> sfntNames;
};

struct GlyphSummary {
    int gid = -1;
    int32_t unicode = -1; // -1 if unencoded
    QList<int32_t> altUnicodes;
    QString name;
    int width = 0;
    int vwidth = 0;
    int contourCount = 0;
    int pointCount = 0;
    int referenceCount = 0;
    QPainterPath outline; // In font coordinates (Y-up), fillRule = Qt::WindingFill
    QRectF bounds;
    bool hasOutline = false;
};

struct FontDocumentSnapshot {
    FontSummary summary;
    QVector<GlyphSummary> glyphs;
};

struct ExportOptions {
    QString outputPath;
    bool roundToInt = true;
    bool opentypeTables = true;
    bool omitHints = false;
    bool dummyDsig = true;
    bool generateAfm = false;
};

struct GlyphMetadataUpdate {
    int gid = -1;
    QString name;
    int32_t unicode = -1;
    QList<int32_t> altUnicodes;
};

struct FontMetadataUpdate {
    QString fontName;
    QString familyName;
    QString fullName;
    QString weight;
    QString version;
    QString copyright;
    int ascent = 800;
    int descent = 200;
    double italicAngle = 0.0;
    double underlinePos = -100.0;
    double underlineWidth = 50.0;
    bool updateSfntNames = false;
    QVector<SfntNameEntry> sfntNames;
};

struct GlyphValidationIssue {
    int gid = -1;
    int32_t unicode = -1;
    QString glyphName;
    uint32_t flags = 0;
    QStringList descriptions;
};

struct FontValidationReport {
    uint32_t combinedFlags = 0;
    int totalGlyphs = 0;
    int issueGlyphCount = 0;
    QVector<GlyphValidationIssue> issues;
};

struct SvgImportOptions {
    int gid = -1;
    QByteArray svgData;
    QByteArray strokeSvgData;
    QByteArray cutoutSvgData;
    bool replaceExistingContours = true;
    bool scaleToEm = true;
    bool simplify = true;
    bool correctDirection = true;
    bool removeOverlap = false;
    bool roundToInt = true;
    bool updateAdvanceWidth = false;
    double svgWidth = 0.0;
    double svgHeight = 0.0;
};

struct SvgImportResult {
    GlyphSummary glyph;
    int importedContours = 0;
};

struct CreateGlyphRequest {
    QString name;
    int32_t unicode = -1;
    QList<int32_t> altUnicodes;
    int advanceWidth = 1000;
};

struct CreateGlyphResult {
    GlyphSummary glyph;
    FontSummary fontSummary;
};

} // namespace ffgui
