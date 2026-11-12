#include "model/BitmapTracer.hpp"

#include <QColor>
#include <QPainter>
#include <QPointF>
#include <QSvgRenderer>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <vector>

namespace ffgui {

namespace {

struct DirectedEdge {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    int dir = 0; // 0 = Right (+x), 1 = Down (+y), 2 = Left (-x), 3 = Up (-y)
    bool used = false;
};

void despeckleBinaryGrid(std::vector<uint8_t>& grid, int w, int h, int minArea, uint8_t targetVal) {
    if (minArea <= 1 || w <= 0 || h <= 0) {
        return;
    }

    std::vector<uint8_t> visited(static_cast<size_t>(w * h), 0);
    std::vector<int> component;
    component.reserve(static_cast<size_t>(minArea * 2));
    std::queue<int> q;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int startIdx = y * w + x;
            if (visited[startIdx] || grid[startIdx] != targetVal) {
                continue;
            }

            component.clear();
            bool touchesBorder = false;
            visited[startIdx] = 1;
            q.push(startIdx);

            while (!q.empty()) {
                const int cur = q.front();
                q.pop();
                component.push_back(cur);

                const int cy = cur / w;
                const int cx = cur % w;
                if (targetVal == 0 && (cx == 0 || cy == 0 || cx == w - 1 || cy == h - 1)) {
                    touchesBorder = true;
                }

                const int dx[4] = {1, -1, 0, 0};
                const int dy[4] = {0, 0, 1, -1};
                for (int d = 0; d < 4; ++d) {
                    const int nx = cx + dx[d];
                    const int ny = cy + dy[d];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) {
                        continue;
                    }
                    const int nIdx = ny * w + nx;
                    if (!visited[nIdx] && grid[nIdx] == targetVal) {
                        visited[nIdx] = 1;
                        q.push(nIdx);
                    }
                }
            }

            if (targetVal == 0 && touchesBorder) {
                continue; // Never fill exterior background
            }

            if (static_cast<int>(component.size()) < minArea) {
                const uint8_t replacement = (targetVal == 1) ? 0 : 1;
                for (int idx : component) {
                    grid[idx] = replacement;
                }
            }
        }
    }
}

double signedPolygonArea(const QVector<QPointF>& pts) {
    if (pts.size() < 3) {
        return 0.0;
    }
    double area2 = 0.0;
    const int n = pts.size();
    for (int i = 0; i < n; ++i) {
        const QPointF& a = pts[i];
        const QPointF& b = pts[(i + 1) % n];
        area2 += (a.x() * b.y() - b.x() * a.y());
    }
    return area2 * 0.5;
}

double pointToSegmentDistanceSq(const QPointF& p, const QPointF& a, const QPointF& b) {
    const double vx = b.x() - a.x();
    const double vy = b.y() - a.y();
    const double wx = p.x() - a.x();
    const double wy = p.y() - a.y();
    const double c1 = wx * vx + wy * vy;
    if (c1 <= 0.0) {
        return wx * wx + wy * wy;
    }
    const double c2 = vx * vx + vy * vy;
    if (c2 <= c1) {
        const double dx = p.x() - b.x();
        const double dy = p.y() - b.y();
        return dx * dx + dy * dy;
    }
    const double t = c1 / c2;
    const double px = a.x() + t * vx;
    const double py = a.y() + t * vy;
    const double dx = p.x() - px;
    const double dy = p.y() - py;
    return dx * dx + dy * dy;
}

void rdpSimplifyRange(const QVector<QPointF>& pts, int first, int last, double epsSq,
                      std::vector<uint8_t>& keep) {
    if (last <= first + 1) {
        return;
    }
    double maxDistSq = -1.0;
    int bestIdx = -1;
    const QPointF& a = pts[first];
    const QPointF& b = pts[last];
    for (int i = first + 1; i < last; ++i) {
        const double dSq = pointToSegmentDistanceSq(pts[i], a, b);
        if (dSq > maxDistSq) {
            maxDistSq = dSq;
            bestIdx = i;
        }
    }
    if (maxDistSq > epsSq && bestIdx != -1) {
        keep[bestIdx] = 1;
        rdpSimplifyRange(pts, first, bestIdx, epsSq, keep);
        rdpSimplifyRange(pts, bestIdx, last, epsSq, keep);
    }
}

// Removes 1-pixel staircase jaggies while preserving long axis-aligned straight segments.
QVector<QPointF> smoothPixelStaircases(const QVector<QPointF>& rawLoop) {
    const int n = rawLoop.size();
    if (n < 4) {
        return rawLoop;
    }

    // First, collapse strictly collinear consecutive grid vertices into run endpoints,
    // inserting anchor points near the ends of long straight runs (>= 4 px) so corners
    // of rectangles stay crisp.
    QVector<QPointF> anchors;
    anchors.reserve(n);

    int i = 0;
    while (i < n) {
        const QPointF p0 = rawLoop[i];
        int j = i + 1;
        const QPointF step = rawLoop[j % n] - p0;
        while (j < i + n) {
            const QPointF nextStep = rawLoop[(j + 1) % n] - rawLoop[j % n];
            if (nextStep != step) {
                break;
            }
            ++j;
        }
        const int runLen = j - i;
        const QPointF pEnd = rawLoop[j % n];
        if (runLen >= 4) {
            anchors.append(p0);
            anchors.append(p0 + step);
            anchors.append(pEnd - step);
        } else {
            anchors.append(p0);
            anchors.append((p0 + pEnd) * 0.5);
        }
        i = j;
    }

    return anchors;
}

QVector<QPointF> simplifyClosedPolygon(const QVector<QPointF>& loop, double tolerance) {
    const int n = loop.size();
    if (n <= 4 || tolerance <= 0.01) {
        return loop;
    }

    // Pick two diametrically opposite vertices as initial split anchors for closed RDP
    int anchorA = 0;
    int anchorB = n / 2;
    double maxPairSq = -1.0;
    for (int i = 0; i < n; i += std::max(1, n / 16)) {
        for (int j = i + 1; j < n; ++j) {
            const double dx = loop[j].x() - loop[i].x();
            const double dy = loop[j].y() - loop[i].y();
            const double dSq = dx * dx + dy * dy;
            if (dSq > maxPairSq) {
                maxPairSq = dSq;
                anchorA = i;
                anchorB = j;
            }
        }
    }

    QVector<QPointF> rotated;
    rotated.reserve(n + 1);
    for (int k = 0; k <= n; ++k) {
        rotated.append(loop[(anchorA + k) % n]);
    }
    const int splitIdx = (anchorB - anchorA + n) % n;

    std::vector<uint8_t> keep(static_cast<size_t>(n + 1), 0);
    keep[0] = 1;
    keep[splitIdx] = 1;
    keep[n] = 1;

    const double epsSq = tolerance * tolerance;
    rdpSimplifyRange(rotated, 0, splitIdx, epsSq, keep);
    rdpSimplifyRange(rotated, splitIdx, n, epsSq, keep);

    QVector<QPointF> out;
    for (int k = 0; k < n; ++k) {
        if (keep[k]) {
            out.append(rotated[k]);
        }
    }
    return out;
}

QString formatNum(double v, double scale) {
    return QString::number(v * scale, 'f', 2);
}

void appendContourToSvgAndPath(const QVector<QPointF>& pts, double coordScale,
                               double curveSmoothing, QString* outSvgPathData,
                               QPainterPath* outPainterPath) {
    const int n = pts.size();
    if (n < 3) {
        return;
    }

    const double alpha = std::clamp(curveSmoothing, 0.0, 1.0);
    if (alpha <= 0.05) {
        // Pure polygonal contour
        outSvgPathData->append(
            QStringLiteral("M %1 %2 ")
                .arg(formatNum(pts[0].x(), coordScale), formatNum(pts[0].y(), coordScale)));
        QPainterPath sub;
        sub.moveTo(pts[0] * coordScale);
        for (int i = 1; i < n; ++i) {
            outSvgPathData->append(
                QStringLiteral("L %1 %2 ")
                    .arg(formatNum(pts[i].x(), coordScale), formatNum(pts[i].y(), coordScale)));
            sub.lineTo(pts[i] * coordScale);
        }
        outSvgPathData->append(QStringLiteral("Z "));
        sub.closeSubpath();
        outPainterPath->addPath(sub);
        return;
    }

    // Potrace-inspired corner-preserving cubic Bezier curve fitting:
    // Midpoints m_i = 0.5 * (pts[i] + pts[i+1]) form the curve endpoints;
    // sharp corners (turning angle > ~75 deg) stay sharp with L segments, while
    // smooth vertices become cubic Beziers (C).
    std::vector<bool> isCorner(static_cast<size_t>(n), false);
    for (int i = 0; i < n; ++i) {
        const QPointF& prev = pts[(i + n - 1) % n];
        const QPointF& curr = pts[i];
        const QPointF& next = pts[(i + 1) % n];
        const QPointF v1 = curr - prev;
        const QPointF v2 = next - curr;
        const double len1 = std::hypot(v1.x(), v1.y());
        const double len2 = std::hypot(v2.x(), v2.y());
        if (len1 < 1e-4 || len2 < 1e-4) {
            isCorner[i] = true;
            continue;
        }
        const double cosAngle = (v1.x() * v2.x() + v1.y() * v2.y()) / (len1 * len2);
        // If the direction changes by more than ~65 degrees OR one of the adjacent segments
        // is a long straight line (>= 6 px) meeting an angle, treat as a crisp corner.
        if (cosAngle < 0.42 || ((len1 >= 6.0 || len2 >= 6.0) && cosAngle < 0.82)) {
            isCorner[i] = true;
        }
    }

    auto midPoint = [&](int idx) -> QPointF {
        return (pts[idx % n] + pts[(idx + 1) % n]) * 0.5;
    };

    const QPointF startMid = midPoint(n - 1);
    outSvgPathData->append(
        QStringLiteral("M %1 %2 ")
            .arg(formatNum(startMid.x(), coordScale), formatNum(startMid.y(), coordScale)));

    QPainterPath sub;
    sub.moveTo(startMid * coordScale);

    const double k = 0.55 * alpha + 0.25; // Control point weight toward vertex (0.25..0.80)
    for (int i = 0; i < n; ++i) {
        const QPointF mPrev = midPoint((i + n - 1) % n);
        const QPointF v = pts[i];
        const QPointF mNext = midPoint(i);
        if (isCorner[i]) {
            outSvgPathData->append(
                QStringLiteral("L %1 %2 L %3 %4 ")
                    .arg(formatNum(v.x(), coordScale), formatNum(v.y(), coordScale),
                         formatNum(mNext.x(), coordScale), formatNum(mNext.y(), coordScale)));
            sub.lineTo(v * coordScale);
            sub.lineTo(mNext * coordScale);
        } else {
            const QPointF c1 = mPrev + (v - mPrev) * k;
            const QPointF c2 = mNext + (v - mNext) * k;
            outSvgPathData->append(
                QStringLiteral("C %1 %2 %3 %4 %5 %6 ")
                    .arg(formatNum(c1.x(), coordScale), formatNum(c1.y(), coordScale),
                         formatNum(c2.x(), coordScale), formatNum(c2.y(), coordScale),
                         formatNum(mNext.x(), coordScale), formatNum(mNext.y(), coordScale)));
            sub.cubicTo(c1 * coordScale, c2 * coordScale, mNext * coordScale);
        }
    }
    outSvgPathData->append(QStringLiteral("Z "));
    sub.closeSubpath();
    outPainterPath->addPath(sub);
}

QByteArray wrapSvgPathDocument(int width, int height, const QString& pathData, bool evenOdd) {
    if (pathData.trimmed().isEmpty()) {
        return {};
    }
    const QString fillRule = evenOdd ? QStringLiteral(" fill-rule=\"evenodd\"") : QString();
    const QString xml =
        QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%2\" "
                       "viewBox=\"0 0 %1 %2\"><path fill=\"#000000\"%3 d=\"%4\"/></svg>")
            .arg(width)
            .arg(height)
            .arg(fillRule, pathData.trimmed());
    return xml.toUtf8();
}

} // namespace

double BitmapTracer::perceptualColorDistance(QRgb c1, QRgb c2) {
    const double dr = static_cast<double>(qRed(c1) - qRed(c2));
    const double dg = static_cast<double>(qGreen(c1) - qGreen(c2));
    const double db = static_cast<double>(qBlue(c1) - qBlue(c2));
    const double distSq = 2.0 * dr * dr + 4.0 * dg * dg + 3.0 * db * db;
    // Maximum possible distance: 2*255^2 + 4*255^2 + 3*255^2 = 585225.0
    return std::sqrt(distSq / 585225.0);
}

QRgb BitmapTracer::detectBackgroundColor(const QImage& image) {
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return 0xFFFFFFFF;
    }

    const int w = image.width();
    const int h = image.height();

    std::unordered_map<uint16_t, int> freq;
    std::unordered_map<uint16_t, QRgb> representative;

    auto sample = [&](int x, int y) {
        const QRgb px = image.pixel(x, y);
        if (qAlpha(px) < 16) {
            return;
        }
        const uint16_t key = static_cast<uint16_t>(
            ((qRed(px) >> 3) << 10) | ((qGreen(px) >> 3) << 5) | (qBlue(px) >> 3));
        freq[key]++;
        if (!representative.count(key)) {
            representative[key] = px;
        }
    };

    for (int x = 0; x < w; ++x) {
        sample(x, 0);
        sample(x, h - 1);
    }
    for (int y = 1; y < h - 1; ++y) {
        sample(0, y);
        sample(w - 1, y);
    }

    if (freq.empty()) {
        return 0xFFFFFFFF;
    }

    uint16_t bestKey = 0;
    int maxCount = -1;
    for (const auto& [k, count] : freq) {
        if (count > maxCount) {
            maxCount = count;
            bestKey = k;
        }
    }
    return representative[bestKey];
}

QVector<QRgb> BitmapTracer::extractPalette(const QImage& image, int maxColors) {
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        return {};
    }

    constexpr int kThumbDim = 128;
    QImage thumb = image;
    if (thumb.width() > kThumbDim || thumb.height() > kThumbDim) {
        thumb = thumb.scaled(kThumbDim, kThumbDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QImage indexed = thumb.convertToFormat(QImage::Format_Indexed8, Qt::AvoidDither);
    const QList<QRgb> table = indexed.colorTable();
    if (table.isEmpty()) {
        return {};
    }

    std::vector<int> counts(table.size(), 0);
    const int w = indexed.width();
    const int h = indexed.height();
    for (int y = 0; y < h; ++y) {
        const uchar* scan = indexed.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const int idx = scan[x];
            if (idx >= 0 && idx < static_cast<int>(counts.size())) {
                counts[idx]++;
            }
        }
    }

    std::vector<int> indices(table.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](int a, int b) {
        return counts[a] > counts[b];
    });

    QVector<QRgb> palette;
    palette.reserve(maxColors);
    for (int idx : indices) {
        if (counts[idx] <= 0) {
            continue;
        }
        const QRgb color = table[idx];
        if (qAlpha(color) < 16) {
            continue;
        }

        bool duplicate = false;
        for (QRgb existing : palette) {
            if (perceptualColorDistance(color, existing) < 0.08) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            palette.append(color);
            if (palette.size() >= maxColors) {
                break;
            }
        }
    }
    return palette;
}

BitmapTraceResult BitmapTracer::trace(const QImage& image, const BitmapTraceOptions& options) {
    BitmapTraceResult result;
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        result.errorMessage = QStringLiteral("PNG / bitmap image is empty or invalid.");
        return result;
    }

    result.imageSize = image.size();
    result.viewBox = QRectF(0.0, 0.0, image.width(), image.height());

    // Cap working raster size to 512px along the longest edge so tracing stays interactive (< 15ms)
    constexpr int kMaxTraceDim = 512;
    QImage workImg = image.convertToFormat(QImage::Format_ARGB32);
    double coordScale = 1.0;
    if (workImg.width() > kMaxTraceDim || workImg.height() > kMaxTraceDim) {
        const QSize scaledSize =
            workImg.size().scaled(kMaxTraceDim, kMaxTraceDim, Qt::KeepAspectRatio);
        coordScale = static_cast<double>(image.width()) / std::max(1, scaledSize.width());
        workImg = workImg.scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    const int w = workImg.width();
    const int h = workImg.height();

    // Detect background color and dominant color palette
    result.detectedBgColor = options.autoDetectBgColor
                                 ? detectBackgroundColor(workImg)
                                 : options.customBgColor;
    result.extractedPalette = extractPalette(workImg, 8);

    // Detect whether the image actually uses alpha transparency
    int minAlpha = 255;
    int maxAlpha = 0;
    for (int y = 0; y < h; ++y) {
        const QRgb* scan = reinterpret_cast<const QRgb*>(workImg.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            const int a = qAlpha(scan[x]);
            minAlpha = std::min(minAlpha, a);
            maxAlpha = std::max(maxAlpha, a);
        }
    }

    const bool hasMeaningfulAlpha = (minAlpha < 245 && maxAlpha > 10);
    const bool useAlpha = options.useAlpha && hasMeaningfulAlpha;
    result.usedAlphaChannel = useAlpha;

    const int thresh = std::clamp(options.threshold, 1, 254);
    std::vector<uint8_t> grid(static_cast<size_t>(w * h), 0);

    const BitmapBinarizationMode mode = options.binarizationMode;
    const QRgb bgCol = result.detectedBgColor;
    const double colTol = std::clamp(options.colorTolerance, 0.01, 1.0);

    if (mode == BitmapBinarizationMode::EdgeGradient) {
        // Multi-channel Sobel Edge Detection
        const double edgeThresh = static_cast<double>(thresh);
        for (int y = 1; y < h - 1; ++y) {
            const QRgb* prevRow = reinterpret_cast<const QRgb*>(workImg.constScanLine(y - 1));
            const QRgb* currRow = reinterpret_cast<const QRgb*>(workImg.constScanLine(y));
            const QRgb* nextRow = reinterpret_cast<const QRgb*>(workImg.constScanLine(y + 1));
            for (int x = 1; x < w - 1; ++x) {
                const int rx = (qRed(prevRow[x + 1]) + 2 * qRed(currRow[x + 1]) + qRed(nextRow[x + 1])) -
                               (qRed(prevRow[x - 1]) + 2 * qRed(currRow[x - 1]) + qRed(nextRow[x - 1]));
                const int ry = (qRed(nextRow[x - 1]) + 2 * qRed(nextRow[x]) + qRed(nextRow[x + 1])) -
                               (qRed(prevRow[x - 1]) + 2 * qRed(prevRow[x]) + qRed(prevRow[x + 1]));

                const int gx = (qGreen(prevRow[x + 1]) + 2 * qGreen(currRow[x + 1]) + qGreen(nextRow[x + 1])) -
                               (qGreen(prevRow[x - 1]) + 2 * qGreen(currRow[x - 1]) + qGreen(nextRow[x - 1]));
                const int gy = (qGreen(nextRow[x - 1]) + 2 * qGreen(nextRow[x]) + qGreen(nextRow[x + 1])) -
                               (qGreen(prevRow[x - 1]) + 2 * qGreen(prevRow[x]) + qGreen(prevRow[x + 1]));

                const int bx = (qBlue(prevRow[x + 1]) + 2 * qBlue(currRow[x + 1]) + qBlue(nextRow[x + 1])) -
                               (qBlue(prevRow[x - 1]) + 2 * qBlue(currRow[x - 1]) + qBlue(nextRow[x - 1]));
                const int by = (qBlue(nextRow[x - 1]) + 2 * qBlue(nextRow[x]) + qBlue(nextRow[x + 1])) -
                               (qBlue(prevRow[x - 1]) + 2 * qBlue(prevRow[x]) + qBlue(prevRow[x + 1]));

                const double gMag = std::sqrt(static_cast<double>(rx * rx + ry * ry + gx * gx + gy * gy + bx * bx + by * by) / 3.0) / 4.0;
                bool fg = (gMag >= edgeThresh * 0.5);
                if (options.invert) {
                    fg = !fg;
                }
                grid[y * w + x] = fg ? 1 : 0;
            }
        }
    } else if (mode == BitmapBinarizationMode::ColorDistance) {
        // Perceptual distance to background color
        for (int y = 0; y < h; ++y) {
            const QRgb* scan = reinterpret_cast<const QRgb*>(workImg.constScanLine(y));
            for (int x = 0; x < w; ++x) {
                const QRgb px = scan[x];
                const int a = qAlpha(px);
                if (a < 10) {
                    grid[y * w + x] = options.invert ? 1 : 0;
                    continue;
                }
                const int r = (qRed(px) * a + qRed(bgCol) * (255 - a)) / 255;
                const int g = (qGreen(px) * a + qGreen(bgCol) * (255 - a)) / 255;
                const int b = (qBlue(px) * a + qBlue(bgCol) * (255 - a)) / 255;
                const QRgb compPx = qRgb(r, g, b);

                const double dist = perceptualColorDistance(compPx, bgCol);
                bool fg = (dist >= colTol);
                if (options.invert) {
                    fg = !fg;
                }
                grid[y * w + x] = fg ? 1 : 0;
            }
        }
    } else if (mode == BitmapBinarizationMode::ColorPalette) {
        // Segment by dominant colors
        QVector<QRgb> targetColors = options.selectedPaletteColors;
        if (targetColors.isEmpty()) {
            for (QRgb c : result.extractedPalette) {
                if (perceptualColorDistance(c, bgCol) >= colTol) {
                    targetColors.append(c);
                }
            }
        }

        for (int y = 0; y < h; ++y) {
            const QRgb* scan = reinterpret_cast<const QRgb*>(workImg.constScanLine(y));
            for (int x = 0; x < w; ++x) {
                const QRgb px = scan[x];
                const int a = qAlpha(px);
                if (a < 10) {
                    grid[y * w + x] = options.invert ? 1 : 0;
                    continue;
                }
                const int r = (qRed(px) * a + qRed(bgCol) * (255 - a)) / 255;
                const int g = (qGreen(px) * a + qGreen(bgCol) * (255 - a)) / 255;
                const int b = (qBlue(px) * a + qBlue(bgCol) * (255 - a)) / 255;
                const QRgb compPx = qRgb(r, g, b);

                bool fg = false;
                for (QRgb target : targetColors) {
                    if (perceptualColorDistance(compPx, target) <= colTol) {
                        fg = true;
                        break;
                    }
                }
                if (options.invert) {
                    fg = !fg;
                }
                grid[y * w + x] = fg ? 1 : 0;
            }
        }
    } else {
        // Standard Alpha / Luminance mode
        for (int y = 0; y < h; ++y) {
            const QRgb* scan = reinterpret_cast<const QRgb*>(workImg.constScanLine(y));
            for (int x = 0; x < w; ++x) {
                const QRgb px = scan[x];
                const int a = qAlpha(px);
                bool fg = false;
                if (useAlpha) {
                    fg = (a >= thresh);
                } else {
                    const int r = (qRed(px) * a + 255 * (255 - a)) / 255;
                    const int g = (qGreen(px) * a + 255 * (255 - a)) / 255;
                    const int b = (qBlue(px) * a + 255 * (255 - a)) / 255;
                    const int luma = (77 * r + 150 * g + 29 * b) >> 8;
                    fg = (luma < thresh);
                }
                if (options.invert) {
                    fg = !fg;
                }
                grid[y * w + x] = fg ? 1 : 0;
            }
        }
    }

    if (options.minSpeckleArea > 1) {
        despeckleBinaryGrid(grid, w, h, options.minSpeckleArea, 1); // Remove tiny foreground specks
        despeckleBinaryGrid(grid, w, h, options.minSpeckleArea, 0); // Fill tiny interior holes
    }

    // Extract directed pixel boundary edges keeping foreground (1) on the right
    const int stride = w + 1;
    std::vector<DirectedEdge> edges;
    edges.reserve(static_cast<size_t>(w * h));
    std::vector<std::vector<int>> outgoing(static_cast<size_t>((w + 1) * (h + 1)));

    auto addEdge = [&](int x0, int y0, int x1, int y1, int dir) {
        const int idx = static_cast<int>(edges.size());
        edges.push_back({x0, y0, x1, y1, dir, false});
        outgoing[y0 * stride + x0].push_back(idx);
    };

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (grid[y * w + x] == 0) {
                continue;
            }
            if (y == 0 || grid[(y - 1) * w + x] == 0) {
                addEdge(x, y, x + 1, y, 0); // Top edge -> Right
            }
            if (x + 1 == w || grid[y * w + (x + 1)] == 0) {
                addEdge(x + 1, y, x + 1, y + 1, 1); // Right edge -> Down
            }
            if (y + 1 == h || grid[(y + 1) * w + x] == 0) {
                addEdge(x + 1, y + 1, x, y + 1, 2); // Bottom edge -> Left
            }
            if (x == 0 || grid[y * w + (x - 1)] == 0) {
                addEdge(x, y + 1, x, y, 3); // Left edge -> Up
            }
        }
    }

    if (edges.empty()) {
        result.valid = true;
        return result;
    }

    QString allPathsD;
    QString solidPathsD;
    QString cutoutPathsD;
    QPainterPath fullPath;
    fullPath.setFillRule(Qt::OddEvenFill);

    for (size_t eIdx = 0; eIdx < edges.size(); ++eIdx) {
        if (edges[eIdx].used) {
            continue;
        }

        QVector<QPointF> rawLoop;
        int curEdge = static_cast<int>(eIdx);
        while (curEdge != -1 && !edges[curEdge].used) {
            edges[curEdge].used = true;
            const DirectedEdge& e = edges[curEdge];
            rawLoop.append(QPointF(e.x0, e.y0));

            const int nextVertex = e.y1 * stride + e.x1;
            const auto& candList = outgoing[nextVertex];
            int chosen = -1;
            int bestTurn = 99;
            // Prefer right turn ((dir + 1) % 4), then straight (dir), then left turn ((dir + 3) % 4)
            for (int cIdx : candList) {
                if (edges[cIdx].used) {
                    continue;
                }
                const int turn = (edges[cIdx].dir - e.dir + 4) % 4;
                const int priority = (turn == 1) ? 0 : (turn == 0 ? 1 : (turn == 3 ? 2 : 3));
                if (priority < bestTurn) {
                    bestTurn = priority;
                    chosen = cIdx;
                }
            }
            curEdge = chosen;
        }

        if (rawLoop.size() < 4) {
            continue;
        }

        const double area = signedPolygonArea(rawLoop);
        if (std::abs(area) < static_cast<double>(std::max(1, options.minSpeckleArea))) {
            continue;
        }

        const QVector<QPointF> deStaircased = smoothPixelStaircases(rawLoop);
        const QVector<QPointF> simplified =
            simplifyClosedPolygon(deStaircased, std::max(0.2, options.simplifyTolerance * 0.65));
        if (simplified.size() < 3) {
            continue;
        }

        ++result.totalContours;
        result.totalNodes += simplified.size();

        QString singleD;
        appendContourToSvgAndPath(simplified, coordScale, options.curveSmoothing, &singleD,
                                  &fullPath);
        allPathsD.append(singleD);

        // In Y-down image coordinates with foreground on the right:
        // area > 0 is an outer clockwise loop; area < 0 is an inner counter-clockwise hole.
        if (area < 0.0) {
            ++result.holeContours;
            cutoutPathsD.append(singleD);
        } else {
            solidPathsD.append(singleD);
        }
    }

    result.path = fullPath;
    result.inkBounds = fullPath.boundingRect();
    result.cleanedSvg =
        wrapSvgPathDocument(image.width(), image.height(), allPathsD, true);
    result.solidSvg =
        wrapSvgPathDocument(image.width(), image.height(), solidPathsD, false);
    result.cutoutSvg =
        wrapSvgPathDocument(image.width(), image.height(), cutoutPathsD, false);
    result.valid = true;
    return result;
}

BitmapTraceResult BitmapTracer::traceFile(const QString& filePath,
                                          const BitmapTraceOptions& options) {
    QImage img(filePath);
    if (img.isNull()) {
        BitmapTraceResult res;
        res.errorMessage = QStringLiteral("Failed to load image file: %1").arg(filePath);
        return res;
    }
    return trace(img, options);
}

QRectF BitmapTracer::computeSvgInkBounds(const QByteArray& svgData, const QRectF& viewBox) {
    if (svgData.isEmpty()) {
        return viewBox;
    }
    QSvgRenderer renderer(svgData);
    if (!renderer.isValid()) {
        return viewBox;
    }

    QRectF vb = viewBox;
    if (!vb.isValid() || vb.width() <= 0.0 || vb.height() <= 0.0) {
        vb = renderer.viewBoxF();
    }
    if (!vb.isValid() || vb.width() <= 0.0 || vb.height() <= 0.0) {
        const QSize def = renderer.defaultSize();
        vb = QRectF(0.0, 0.0, def.width() > 0 ? def.width() : 100.0,
                    def.height() > 0 ? def.height() : 100.0);
    }

    constexpr int kProbeDim = 256;
    const QSize probeSize =
        QSizeF(vb.width(), vb.height()).scaled(kProbeDim, kProbeDim, Qt::KeepAspectRatio).toSize();
    const int pw = std::max(16, probeSize.width());
    const int ph = std::max(16, probeSize.height());

    QImage img(pw, ph, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        renderer.render(&p, QRectF(0.0, 0.0, pw, ph));
    }

    int minX = pw;
    int minY = ph;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < ph; ++y) {
        const QRgb* scan = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < pw; ++x) {
            if (qAlpha(scan[x]) > 8) {
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        return vb;
    }

    const double sx = vb.width() / static_cast<double>(pw);
    const double sy = vb.height() / static_cast<double>(ph);
    return QRectF(vb.left() + minX * sx, vb.top() + minY * sy,
                  (maxX - minX + 1) * sx, (maxY - minY + 1) * sy);
}

} // namespace ffgui

