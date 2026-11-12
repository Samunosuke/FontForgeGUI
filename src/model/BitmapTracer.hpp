#pragma once

#include <QByteArray>
#include <QImage>
#include <QPainterPath>
#include <QRectF>
#include <QSize>
#include <QString>

namespace ffgui {

struct BitmapTraceOptions {
    bool useAlpha{true};          // Prefer alpha channel when transparency is present
    int threshold{128};           // Alpha / luminance threshold (1..254)
    bool invert{false};           // Invert foreground and background
    int minSpeckleArea{4};        // Drop isolated pixel islands/holes smaller than N px
    double simplifyTolerance{1.0}; // Polygon simplification tolerance in pixels
    double curveSmoothing{0.75};  // 0.0 = polygonal lines, 0.1..1.0 = smooth cubic Beziers
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
};

class BitmapTracer {
public:
    static BitmapTraceResult trace(const QImage& image,
                                   const BitmapTraceOptions& options = {});
    static BitmapTraceResult traceFile(const QString& filePath,
                                       const BitmapTraceOptions& options = {});

    // Renders an SVG or image to a probe buffer and returns the tight ink bounding box
    // in viewBox coordinates.
    static QRectF computeSvgInkBounds(const QByteArray& svgData, const QRectF& viewBox);
};

} // namespace ffgui

