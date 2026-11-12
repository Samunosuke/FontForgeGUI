#pragma once

#include "backend/FontTypes.hpp"

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QPainterPath>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVector>

namespace ffgui {

struct BitmapTraceOptions {
    BitmapBinarizationMode binarizationMode{BitmapBinarizationMode::AlphaOrLuminance};
    bool useAlpha{true};          // Prefer alpha channel when transparency is present
    int threshold{128};           // Alpha / luminance / edge threshold (1..254)
    bool invert{false};           // Invert foreground and background
    int minSpeckleArea{4};        // Drop isolated pixel islands/holes smaller than N px
    double simplifyTolerance{1.0}; // Polygon simplification tolerance in pixels
    double curveSmoothing{0.75};  // 0.0 = polygonal lines, 0.1..1.0 = smooth cubic Beziers

    // Multicolored PNG additions (v0.5.2)
    bool autoDetectBgColor{true};
    QRgb customBgColor{0xFFFFFFFF}; // Background color (used if autoDetectBgColor is false or as fallback)
    double colorTolerance{0.15};    // Color distance tolerance (0.01 .. 1.00, default 0.15 = 15%)
    QVector<QRgb> selectedPaletteColors; // For ColorPalette mode: which colors are treated as foreground
};

struct BitmapTraceResult {
    bool valid{false};
    QString errorMessage;
    QByteArray cleanedSvg; // Full SVG (with evenodd fill rule for preview)
    QByteArray solidSvg;   // Outer contours SVG
    QByteArray cutoutSvg;  // Inner hole contours SVG
    QPainterPath path;     // Traced path in SVG pixel coordinates (Y-down)
    QRectF viewBox;
    QRectF inkBounds;      // Tight bounding box of traced foreground contours
    QSize imageSize;
    int totalContours{0};
    int holeContours{0};
    int totalNodes{0};
    bool usedAlphaChannel{false};

    // Multicolored PNG results (v0.5.2)
    QRgb detectedBgColor{0xFFFFFFFF}; // The background color used (detected or custom)
    QVector<QRgb> extractedPalette;   // Dominant colors detected in the image
};

class BitmapTracer {
public:
    static BitmapTraceResult trace(const QImage& image,
                                   const BitmapTraceOptions& options = {});
    static BitmapTraceResult traceFile(const QString& filePath,
                                       const BitmapTraceOptions& options = {});

    // Utility methods for color detection and segmentation
    static QRgb detectBackgroundColor(const QImage& image);
    static QVector<QRgb> extractPalette(const QImage& image, int maxColors = 8);
    static double perceptualColorDistance(QRgb c1, QRgb c2);

    // Renders an SVG or image to a probe buffer and returns the tight ink bounding box
    // in viewBox coordinates.
    static QRectF computeSvgInkBounds(const QByteArray& svgData, const QRectF& viewBox);
};

} // namespace ffgui
