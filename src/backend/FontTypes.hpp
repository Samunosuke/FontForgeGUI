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
    bool isSaveAs = false;
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

enum class GlyphPlacementMode {
    AutoAspectRatio = 0, // Square/tall (W/H <= 1.25) on baseline y=0; wide (W/H > 1.25) vertically centered
    BaselineCentered = 1, // Bottom on baseline (y = 0), horizontally centered in advance width
    CenterInAscent = 2,   // Horizontally & vertically centered between baseline (0) and ascent
    CenterInEm = 3,       // Horizontally & vertically centered in full EM box (-descent .. +ascent)
    OriginalSvgCoords = 4 // Keep raw FontForge SVG coordinates
};

enum class BitmapBinarizationMode {
    AlphaOrLuminance = 0, // Traditional: uses alpha if present, else luminance threshold
    ColorDistance = 1,    // Background keying: distance from background color > tolerance
    ColorPalette = 2,     // Segment by dominant colors: select which colors form the foreground
    EdgeGradient = 3      // Multi-channel Sobel gradient: contour lines at color boundaries
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

    GlyphPlacementMode placementMode = GlyphPlacementMode::AutoAspectRatio;
    double scaleFactor = 1.0;  // Additional manual scale multiplier (1.0 = 100%)
    double marginRatio = 0.04; // Default side/top padding ratio when fitting
    double offsetX = 0.0;      // Manual X offset in font units
    double offsetY = 0.0;      // Manual Y offset in font units

    // SVG & Vector cleaning options
    bool normalizeToBlackFill = true;
    bool whiteAsCutout = true;
    bool keepStrokes = false;
    bool stripTinyPaths = true;
    double minGeometryExtent = 1.0;
    QStringList excludedColors;
    QStringList includedColors;

    // Bitmap tracing options (for PNG)
    bool useAlpha = true;
    int bitmapThreshold = 128; // 1..254
    bool invertBitmap = false;
    int minSpeckleArea = 4;
    double curveSmoothing = 0.75;

    // Multicolored PNG additions (v0.5.2)
    BitmapBinarizationMode binarizationMode = BitmapBinarizationMode::AlphaOrLuminance;
    bool autoDetectBgColor = true;
    QString customBgColor = QStringLiteral("#FFFFFF");
    double colorTolerance = 0.15; // 0.01 .. 1.00 (15%)
};

struct IconPlacementTransform {
    double scale = 1.0;
    double tx = 0.0;
    double ty = 0.0;
    bool isWideAspect = false;
    double aspectRatio = 1.0;
};

inline IconPlacementTransform computeIconPlacement(double minX, double maxX, double minY,
                                                   double maxY, double advanceWidth, int ascent,
                                                   int descent, const SvgImportOptions& opts) {
    IconPlacementTransform result;
    const double w = maxX - minX;
    const double h = maxY - minY;
    if (w <= 1e-6 || h <= 1e-6) {
        return result;
    }

    result.aspectRatio = w / h;
    result.isWideAspect = (result.aspectRatio > 1.25);

    const double userScale = (opts.scaleFactor > 0.01) ? opts.scaleFactor : 1.0;
    if (opts.placementMode == GlyphPlacementMode::OriginalSvgCoords) {
        const double cx = minX + w * 0.5;
        const double cy = minY + h * 0.5;
        result.scale = userScale;
        result.tx = (cx - cx * userScale) + opts.offsetX;
        result.ty = (cy - cy * userScale) + opts.offsetY;
        return result;
    }

    const double emSize = static_cast<double>((ascent + descent) > 0 ? (ascent + descent) : 1000);
    const double effAscent = (ascent > 0) ? static_cast<double>(ascent) : emSize * 0.8;
    const double effDescent = (descent >= 0) ? static_cast<double>(descent) : emSize * 0.2;
    const double availWidth = (advanceWidth > 0.0) ? advanceWidth : emSize;
    const double margin = (opts.marginRatio >= 0.0 && opts.marginRatio < 0.45) ? opts.marginRatio : 0.04;

    GlyphPlacementMode effectiveMode = opts.placementMode;
    if (effectiveMode == GlyphPlacementMode::AutoAspectRatio) {
        effectiveMode = result.isWideAspect ? GlyphPlacementMode::CenterInAscent
                                            : GlyphPlacementMode::BaselineCentered;
    }

    double baseScale = 1.0;
    if (opts.scaleToEm) {
        const double boxW = availWidth * (1.0 - 2.0 * margin);
        double boxH = effAscent * (1.0 - margin);
        if (effectiveMode == GlyphPlacementMode::CenterInAscent) {
            boxH = effAscent * (1.0 - 2.0 * margin);
        } else if (effectiveMode == GlyphPlacementMode::CenterInEm) {
            boxH = emSize * (1.0 - 2.0 * margin);
        }
        if (boxW > 1.0 && boxH > 1.0) {
            const double sx = boxW / w;
            const double sy = boxH / h;
            baseScale = (sx < sy) ? sx : sy;
        }
    }

    result.scale = baseScale * userScale;
    const double sw = w * result.scale;
    const double sh = h * result.scale;

    const double targetMinX = (availWidth - sw) * 0.5 + opts.offsetX;
    double targetMinY = 0.0;
    switch (effectiveMode) {
    case GlyphPlacementMode::BaselineCentered:
        targetMinY = 0.0;
        break;
    case GlyphPlacementMode::CenterInAscent:
        targetMinY = (effAscent - sh) * 0.5;
        break;
    case GlyphPlacementMode::CenterInEm:
        targetMinY = -effDescent + (emSize - sh) * 0.5;
        break;
    default:
        targetMinY = 0.0;
        break;
    }
    targetMinY += opts.offsetY;

    result.tx = targetMinX - minX * result.scale;
    result.ty = targetMinY - minY * result.scale;
    return result;
}

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

struct NewFontOptions {
    QString fontName = QStringLiteral("Untitled");
    QString familyName = QStringLiteral("Untitled");
    QString fullName = QStringLiteral("Untitled");
    QString weight = QStringLiteral("Regular");
    QString version = QStringLiteral("1.0");
    QString copyright;
    int ascent = 800;
    int descent = 200;
    int emSize = 1000;
};

struct MoveGlyphsRequest {
    QList<int> gids;
    int32_t targetStartUnicode = -1;
    bool overwriteOccupied = false;
};

struct MoveGlyphsResult {
    int movedCount = 0;
    QVector<GlyphSummary> updatedGlyphs;
    FontDocumentSnapshot snapshot;
};

struct BatchImportItem {
    QString filePath;
    int32_t unicode = -1;
    QString glyphName;
    SvgImportOptions customOptions;
    bool hasCustomOptions = false;
};

struct BatchImportJob {
    QVector<BatchImportItem> items;
    SvgImportOptions defaultOptions;
    bool overwriteExisting = false;
};

struct BatchImportResult {
    int importedCount = 0;
    int skippedCount = 0;
    QStringList errors;
    FontSummary updatedFontSummary;
    QVector<GlyphSummary> updatedGlyphs;
    FontDocumentSnapshot snapshot;
};

} // namespace ffgui
