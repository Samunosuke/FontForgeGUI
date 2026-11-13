#pragma once

#include "backend/FontTypes.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QSettings>
#include <QString>

namespace ffgui {

namespace SettingsKeys {
    inline constexpr QLatin1StringView Organization{"FontForgeGUI"};
    inline constexpr QLatin1StringView Application{"FontForgeGUI"};
    inline constexpr QLatin1StringView LastFontDir{"lastFontDir"};
    inline constexpr QLatin1StringView DefaultImportOptions{"defaultImportOptions"};
    inline constexpr QLatin1StringView ToolbarDisplayMode{"toolbarDisplayMode"};
} // namespace SettingsKeys

enum class ToolbarDisplayMode {
    IconAndTextBeside = 0,
    IconAndTextUnder = 1,
    IconOnly = 2,
    TextOnly = 3,
};

namespace ImportOptionKeys {
    inline constexpr QLatin1StringView PlacementMode{"placementMode"};
    inline constexpr QLatin1StringView ScaleToEm{"scaleToEm"};
    inline constexpr QLatin1StringView ScaleFactor{"scaleFactor"};
    inline constexpr QLatin1StringView Simplify{"simplify"};
    inline constexpr QLatin1StringView CorrectDirection{"correctDirection"};
    inline constexpr QLatin1StringView RemoveOverlap{"removeOverlap"};
    inline constexpr QLatin1StringView RoundToInt{"roundToInt"};
    inline constexpr QLatin1StringView MarginRatio{"marginRatio"};
    inline constexpr QLatin1StringView OffsetX{"offsetX"};
    inline constexpr QLatin1StringView OffsetY{"offsetY"};
    inline constexpr QLatin1StringView NormalizeToBlackFill{"normalizeToBlackFill"};
    inline constexpr QLatin1StringView WhiteAsCutout{"whiteAsCutout"};
    inline constexpr QLatin1StringView KeepStrokes{"keepStrokes"};
    inline constexpr QLatin1StringView StripTinyPaths{"stripTinyPaths"};
    inline constexpr QLatin1StringView MinGeometryExtent{"minGeometryExtent"};
    inline constexpr QLatin1StringView UseAlpha{"useAlpha"};
    inline constexpr QLatin1StringView BitmapThreshold{"bitmapThreshold"};
    inline constexpr QLatin1StringView InvertBitmap{"invertBitmap"};
    inline constexpr QLatin1StringView MinSpeckleArea{"minSpeckleArea"};
    inline constexpr QLatin1StringView CurveSmoothing{"curveSmoothing"};
    inline constexpr QLatin1StringView BinarizationMode{"binarizationMode"};
    inline constexpr QLatin1StringView AutoDetectBgColor{"autoDetectBgColor"};
    inline constexpr QLatin1StringView CustomBgColor{"customBgColor"};
    inline constexpr QLatin1StringView ColorTolerance{"colorTolerance"};
} // namespace ImportOptionKeys

inline BitmapBinarizationMode parseBinarizationMode(const QString& str) {
    const QString s = str.toLower();
    if (s == QStringLiteral("colordistance") || s == QStringLiteral("distance") || s == QStringLiteral("chromakey")) {
        return BitmapBinarizationMode::ColorDistance;
    }
    if (s == QStringLiteral("colorpalette") || s == QStringLiteral("palette")) {
        return BitmapBinarizationMode::ColorPalette;
    }
    if (s == QStringLiteral("edgegradient") || s == QStringLiteral("edge") || s == QStringLiteral("sobel")) {
        return BitmapBinarizationMode::EdgeGradient;
    }
    return BitmapBinarizationMode::AlphaOrLuminance;
}

inline QString binarizationModeToString(BitmapBinarizationMode mode) {
    switch (mode) {
    case BitmapBinarizationMode::ColorDistance:
        return QStringLiteral("ColorDistance");
    case BitmapBinarizationMode::ColorPalette:
        return QStringLiteral("ColorPalette");
    case BitmapBinarizationMode::EdgeGradient:
        return QStringLiteral("EdgeGradient");
    default:
        return QStringLiteral("AlphaOrLuminance");
    }
}

inline GlyphPlacementMode parsePlacementMode(const QString& str) {
    const QString s = str.toLower();
    if (s == QStringLiteral("baseline") || s == QStringLiteral("baselinecentered")) {
        return GlyphPlacementMode::BaselineCentered;
    }
    if (s == QStringLiteral("centerinascent") || s == QStringLiteral("ascent") || s == QStringLiteral("center-ascent")) {
        return GlyphPlacementMode::CenterInAscent;
    }
    if (s == QStringLiteral("centerinem") || s == QStringLiteral("em") || s == QStringLiteral("center-em")) {
        return GlyphPlacementMode::CenterInEm;
    }
    if (s == QStringLiteral("originalsvgcoords") || s == QStringLiteral("raw") || s == QStringLiteral("original")) {
        return GlyphPlacementMode::OriginalSvgCoords;
    }
    return GlyphPlacementMode::AutoAspectRatio;
}

inline QString placementModeToString(GlyphPlacementMode mode) {
    switch (mode) {
    case GlyphPlacementMode::BaselineCentered:
        return QStringLiteral("BaselineCentered");
    case GlyphPlacementMode::CenterInAscent:
        return QStringLiteral("CenterInAscent");
    case GlyphPlacementMode::CenterInEm:
        return QStringLiteral("CenterInEm");
    case GlyphPlacementMode::OriginalSvgCoords:
        return QStringLiteral("OriginalSvgCoords");
    default:
        return QStringLiteral("AutoAspectRatio");
    }
}

inline SvgImportOptions importOptionsFromJson(const QJsonObject& obj,
                                              const SvgImportOptions& fallback = SvgImportOptions{}) {
    SvgImportOptions opts = fallback;
    if (obj.contains(ImportOptionKeys::PlacementMode)) {
        const QJsonValue val = obj.value(ImportOptionKeys::PlacementMode);
        if (val.isString()) {
            opts.placementMode = parsePlacementMode(val.toString());
        } else if (val.isDouble()) {
            opts.placementMode = static_cast<GlyphPlacementMode>(val.toInt(static_cast<int>(opts.placementMode)));
        }
    }
    if (obj.contains(ImportOptionKeys::ScaleFactor)) {
        opts.scaleFactor = obj.value(ImportOptionKeys::ScaleFactor).toDouble(opts.scaleFactor);
    }
    if (obj.contains(ImportOptionKeys::ScaleToEm)) {
        opts.scaleToEm = obj.value(ImportOptionKeys::ScaleToEm).toBool(opts.scaleToEm);
    }
    if (obj.contains(ImportOptionKeys::Simplify)) {
        opts.simplify = obj.value(ImportOptionKeys::Simplify).toBool(opts.simplify);
    }
    if (obj.contains(ImportOptionKeys::CorrectDirection)) {
        opts.correctDirection = obj.value(ImportOptionKeys::CorrectDirection).toBool(opts.correctDirection);
    }
    if (obj.contains(ImportOptionKeys::RemoveOverlap)) {
        opts.removeOverlap = obj.value(ImportOptionKeys::RemoveOverlap).toBool(opts.removeOverlap);
    }
    if (obj.contains(ImportOptionKeys::RoundToInt)) {
        opts.roundToInt = obj.value(ImportOptionKeys::RoundToInt).toBool(opts.roundToInt);
    }
    if (obj.contains(ImportOptionKeys::MarginRatio)) {
        opts.marginRatio = obj.value(ImportOptionKeys::MarginRatio).toDouble(opts.marginRatio);
    }
    if (obj.contains(ImportOptionKeys::OffsetX)) {
        opts.offsetX = obj.value(ImportOptionKeys::OffsetX).toDouble(opts.offsetX);
    }
    if (obj.contains(ImportOptionKeys::OffsetY)) {
        opts.offsetY = obj.value(ImportOptionKeys::OffsetY).toDouble(opts.offsetY);
    }
    if (obj.contains(ImportOptionKeys::NormalizeToBlackFill)) {
        opts.normalizeToBlackFill = obj.value(ImportOptionKeys::NormalizeToBlackFill).toBool(opts.normalizeToBlackFill);
    }
    if (obj.contains(ImportOptionKeys::WhiteAsCutout)) {
        opts.whiteAsCutout = obj.value(ImportOptionKeys::WhiteAsCutout).toBool(opts.whiteAsCutout);
    }
    if (obj.contains(ImportOptionKeys::KeepStrokes)) {
        opts.keepStrokes = obj.value(ImportOptionKeys::KeepStrokes).toBool(opts.keepStrokes);
    }
    if (obj.contains(ImportOptionKeys::StripTinyPaths)) {
        opts.stripTinyPaths = obj.value(ImportOptionKeys::StripTinyPaths).toBool(opts.stripTinyPaths);
    }
    if (obj.contains(ImportOptionKeys::MinGeometryExtent)) {
        opts.minGeometryExtent = obj.value(ImportOptionKeys::MinGeometryExtent).toDouble(opts.minGeometryExtent);
    }
    if (obj.contains(ImportOptionKeys::UseAlpha)) {
        opts.useAlpha = obj.value(ImportOptionKeys::UseAlpha).toBool(opts.useAlpha);
    }
    if (obj.contains(ImportOptionKeys::BitmapThreshold)) {
        opts.bitmapThreshold = obj.value(ImportOptionKeys::BitmapThreshold).toInt(opts.bitmapThreshold);
    }
    if (obj.contains(ImportOptionKeys::InvertBitmap)) {
        opts.invertBitmap = obj.value(ImportOptionKeys::InvertBitmap).toBool(opts.invertBitmap);
    }
    if (obj.contains(ImportOptionKeys::MinSpeckleArea)) {
        opts.minSpeckleArea = obj.value(ImportOptionKeys::MinSpeckleArea).toInt(opts.minSpeckleArea);
    }
    if (obj.contains(ImportOptionKeys::CurveSmoothing)) {
        opts.curveSmoothing = obj.value(ImportOptionKeys::CurveSmoothing).toDouble(opts.curveSmoothing);
    }
    if (obj.contains(ImportOptionKeys::BinarizationMode)) {
        const QJsonValue val = obj.value(ImportOptionKeys::BinarizationMode);
        if (val.isString()) {
            opts.binarizationMode = parseBinarizationMode(val.toString());
        } else if (val.isDouble()) {
            opts.binarizationMode = static_cast<BitmapBinarizationMode>(val.toInt(static_cast<int>(opts.binarizationMode)));
        }
    }
    if (obj.contains(ImportOptionKeys::AutoDetectBgColor)) {
        opts.autoDetectBgColor = obj.value(ImportOptionKeys::AutoDetectBgColor).toBool(opts.autoDetectBgColor);
    }
    if (obj.contains(ImportOptionKeys::CustomBgColor)) {
        opts.customBgColor = obj.value(ImportOptionKeys::CustomBgColor).toString(opts.customBgColor);
    }
    if (obj.contains(ImportOptionKeys::ColorTolerance)) {
        opts.colorTolerance = obj.value(ImportOptionKeys::ColorTolerance).toDouble(opts.colorTolerance);
    }
    return opts;
}

inline QJsonObject importOptionsToJson(const SvgImportOptions& opts) {
    QJsonObject obj;
    obj.insert(ImportOptionKeys::PlacementMode, placementModeToString(opts.placementMode));
    obj.insert(ImportOptionKeys::ScaleFactor, opts.scaleFactor);
    obj.insert(ImportOptionKeys::ScaleToEm, opts.scaleToEm);
    obj.insert(ImportOptionKeys::Simplify, opts.simplify);
    obj.insert(ImportOptionKeys::CorrectDirection, opts.correctDirection);
    obj.insert(ImportOptionKeys::RemoveOverlap, opts.removeOverlap);
    obj.insert(ImportOptionKeys::RoundToInt, opts.roundToInt);
    obj.insert(ImportOptionKeys::MarginRatio, opts.marginRatio);
    obj.insert(ImportOptionKeys::OffsetX, opts.offsetX);
    obj.insert(ImportOptionKeys::OffsetY, opts.offsetY);
    obj.insert(ImportOptionKeys::NormalizeToBlackFill, opts.normalizeToBlackFill);
    obj.insert(ImportOptionKeys::WhiteAsCutout, opts.whiteAsCutout);
    obj.insert(ImportOptionKeys::KeepStrokes, opts.keepStrokes);
    obj.insert(ImportOptionKeys::StripTinyPaths, opts.stripTinyPaths);
    obj.insert(ImportOptionKeys::MinGeometryExtent, opts.minGeometryExtent);
    obj.insert(ImportOptionKeys::UseAlpha, opts.useAlpha);
    obj.insert(ImportOptionKeys::BitmapThreshold, opts.bitmapThreshold);
    obj.insert(ImportOptionKeys::InvertBitmap, opts.invertBitmap);
    obj.insert(ImportOptionKeys::MinSpeckleArea, opts.minSpeckleArea);
    obj.insert(ImportOptionKeys::CurveSmoothing, opts.curveSmoothing);
    obj.insert(ImportOptionKeys::BinarizationMode, binarizationModeToString(opts.binarizationMode));
    obj.insert(ImportOptionKeys::AutoDetectBgColor, opts.autoDetectBgColor);
    obj.insert(ImportOptionKeys::CustomBgColor, opts.customBgColor);
    obj.insert(ImportOptionKeys::ColorTolerance, opts.colorTolerance);
    return obj;
}

inline QString loadLastFontDirectory() {
    QSettings settings(SettingsKeys::Organization, SettingsKeys::Application);
    const QString dir = settings.value(SettingsKeys::LastFontDir).toString();
    if (!dir.isEmpty() && QDir(dir).exists()) {
        return dir;
    }
    return {};
}

inline void saveLastFontDirectory(const QString& filePathOrDir) {
    if (filePathOrDir.isEmpty()) {
        return;
    }
    const QFileInfo fi(filePathOrDir);
    const QString dir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    if (!dir.isEmpty() && QDir(dir).exists()) {
        QSettings settings(SettingsKeys::Organization, SettingsKeys::Application);
        settings.setValue(SettingsKeys::LastFontDir, dir);
    }
}

inline SvgImportOptions loadDefaultImportOptions() {
    QSettings settings(SettingsKeys::Organization, SettingsKeys::Application);
    const QString jsonStr = settings.value(SettingsKeys::DefaultImportOptions).toString();
    if (jsonStr.isEmpty()) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isObject()) {
        return {};
    }
    return importOptionsFromJson(doc.object());
}

inline void saveDefaultImportOptions(const SvgImportOptions& opts) {
    const QJsonObject obj = importOptionsToJson(opts);
    QSettings settings(SettingsKeys::Organization, SettingsKeys::Application);
    settings.setValue(SettingsKeys::DefaultImportOptions,
                      QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

inline void resetDefaultImportOptions() {
    QSettings settings(SettingsKeys::Organization, SettingsKeys::Application);
    settings.remove(SettingsKeys::DefaultImportOptions);
}

inline Qt::ToolButtonStyle toQtToolButtonStyle(ToolbarDisplayMode mode) {
    switch (mode) {
    case ToolbarDisplayMode::IconAndTextBeside:
        return Qt::ToolButtonTextBesideIcon;
    case ToolbarDisplayMode::IconAndTextUnder:
        return Qt::ToolButtonTextUnderIcon;
    case ToolbarDisplayMode::IconOnly:
        return Qt::ToolButtonIconOnly;
    case ToolbarDisplayMode::TextOnly:
        return Qt::ToolButtonTextOnly;
    }
    return Qt::ToolButtonTextBesideIcon;
}

inline ToolbarDisplayMode loadToolbarDisplayMode() {
    QSettings settings(SettingsKeys::Organization, SettingsKeys::Application);
    const int raw =
        settings.value(SettingsKeys::ToolbarDisplayMode,
                       static_cast<int>(ToolbarDisplayMode::IconAndTextBeside))
            .toInt();
    if (raw < 0 || raw > static_cast<int>(ToolbarDisplayMode::TextOnly)) {
        return ToolbarDisplayMode::IconAndTextBeside;
    }
    return static_cast<ToolbarDisplayMode>(raw);
}

inline void saveToolbarDisplayMode(ToolbarDisplayMode mode) {
    QSettings settings(SettingsKeys::Organization, SettingsKeys::Application);
    settings.setValue(SettingsKeys::ToolbarDisplayMode, static_cast<int>(mode));
}

} // namespace ffgui
