#pragma once

#include <QByteArray>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace ffgui {

struct SvgFilterOptions {
    bool stripEditorMetadata{true};
    bool removeHiddenElements{true};
    bool removeBackgroundRects{true};
    bool stripNoFillElements{true};
    bool keepStrokes{false};
    bool normalizeToBlackFill{true};
    bool whiteAsCutout{true};
    bool stripTinyPaths{true};
    double minGeometryExtent{1.0};
    QStringList excludedColors;
    QStringList includedColors;
};

struct SvgSanitizeResult {
    bool valid{false};
    QString errorMessage;
    QByteArray cleanedSvg;
    QByteArray solidSvg;
    QByteArray strokeSvg;
    QByteArray cutoutSvg;
    QRectF viewBox;
    double width{0.0};
    double height{0.0};
    QStringList detectedColors;
    int totalElementsBefore{0};
    int keptGeometryCount{0};
    int cutoutGeometryCount{0};
    int removedMetadataCount{0};
    int removedHiddenCount{0};
    int removedColorFilteredCount{0};
    int removedTinyOrEmptyCount{0};
};

class SvgSanitizer {
public:
    static SvgSanitizeResult sanitize(const QByteArray& rawSvg,
                                      const SvgFilterOptions& options = {});
    static SvgSanitizeResult sanitizeFile(const QString& filePath,
                                          const SvgFilterOptions& options = {});
    static QString normalizeColor(const QString& rawColor);
};

} // namespace ffgui

