#pragma once

#include "backend/FontTypes.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QString>

namespace ffgui {

inline QString loadLastFontDirectory() {
    QSettings settings(QStringLiteral("FontForgeGUI"), QStringLiteral("FontForgeGUI"));
    const QString dir = settings.value(QStringLiteral("lastFontDir")).toString();
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
        QSettings settings(QStringLiteral("FontForgeGUI"), QStringLiteral("FontForgeGUI"));
        settings.setValue(QStringLiteral("lastFontDir"), dir);
    }
}

inline SvgImportOptions loadDefaultImportOptions() {
    SvgImportOptions opts;
    QSettings settings(QStringLiteral("FontForgeGUI"), QStringLiteral("FontForgeGUI"));
    const QString jsonStr = settings.value(QStringLiteral("defaultImportOptions")).toString();
    if (jsonStr.isEmpty()) {
        return opts;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isObject()) {
        return opts;
    }
    const QJsonObject obj = doc.object();
    if (obj.contains(QStringLiteral("placementMode"))) {
        opts.placementMode = static_cast<GlyphPlacementMode>(obj.value(QStringLiteral("placementMode")).toInt(static_cast<int>(opts.placementMode)));
    }
    if (obj.contains(QStringLiteral("scaleToEm"))) {
        opts.scaleToEm = obj.value(QStringLiteral("scaleToEm")).toBool(opts.scaleToEm);
    }
    if (obj.contains(QStringLiteral("scaleFactor"))) {
        opts.scaleFactor = obj.value(QStringLiteral("scaleFactor")).toDouble(opts.scaleFactor);
    }
    if (obj.contains(QStringLiteral("simplify"))) {
        opts.simplify = obj.value(QStringLiteral("simplify")).toBool(opts.simplify);
    }
    if (obj.contains(QStringLiteral("correctDirection"))) {
        opts.correctDirection = obj.value(QStringLiteral("correctDirection")).toBool(opts.correctDirection);
    }
    if (obj.contains(QStringLiteral("removeOverlap"))) {
        opts.removeOverlap = obj.value(QStringLiteral("removeOverlap")).toBool(opts.removeOverlap);
    }
    if (obj.contains(QStringLiteral("roundToInt"))) {
        opts.roundToInt = obj.value(QStringLiteral("roundToInt")).toBool(opts.roundToInt);
    }
    if (obj.contains(QStringLiteral("marginRatio"))) {
        opts.marginRatio = obj.value(QStringLiteral("marginRatio")).toDouble(opts.marginRatio);
    }
    if (obj.contains(QStringLiteral("offsetX"))) {
        opts.offsetX = obj.value(QStringLiteral("offsetX")).toDouble(opts.offsetX);
    }
    if (obj.contains(QStringLiteral("offsetY"))) {
        opts.offsetY = obj.value(QStringLiteral("offsetY")).toDouble(opts.offsetY);
    }
    if (obj.contains(QStringLiteral("normalizeToBlackFill"))) {
        opts.normalizeToBlackFill = obj.value(QStringLiteral("normalizeToBlackFill")).toBool(opts.normalizeToBlackFill);
    }
    if (obj.contains(QStringLiteral("whiteAsCutout"))) {
        opts.whiteAsCutout = obj.value(QStringLiteral("whiteAsCutout")).toBool(opts.whiteAsCutout);
    }
    if (obj.contains(QStringLiteral("keepStrokes"))) {
        opts.keepStrokes = obj.value(QStringLiteral("keepStrokes")).toBool(opts.keepStrokes);
    }
    if (obj.contains(QStringLiteral("stripTinyPaths"))) {
        opts.stripTinyPaths = obj.value(QStringLiteral("stripTinyPaths")).toBool(opts.stripTinyPaths);
    }
    if (obj.contains(QStringLiteral("minGeometryExtent"))) {
        opts.minGeometryExtent = obj.value(QStringLiteral("minGeometryExtent")).toDouble(opts.minGeometryExtent);
    }
    if (obj.contains(QStringLiteral("useAlpha"))) {
        opts.useAlpha = obj.value(QStringLiteral("useAlpha")).toBool(opts.useAlpha);
    }
    if (obj.contains(QStringLiteral("bitmapThreshold"))) {
        opts.bitmapThreshold = obj.value(QStringLiteral("bitmapThreshold")).toInt(opts.bitmapThreshold);
    }
    if (obj.contains(QStringLiteral("invertBitmap"))) {
        opts.invertBitmap = obj.value(QStringLiteral("invertBitmap")).toBool(opts.invertBitmap);
    }
    if (obj.contains(QStringLiteral("minSpeckleArea"))) {
        opts.minSpeckleArea = obj.value(QStringLiteral("minSpeckleArea")).toInt(opts.minSpeckleArea);
    }
    if (obj.contains(QStringLiteral("curveSmoothing"))) {
        opts.curveSmoothing = obj.value(QStringLiteral("curveSmoothing")).toDouble(opts.curveSmoothing);
    }
    return opts;
}

inline void saveDefaultImportOptions(const SvgImportOptions& opts) {
    QJsonObject obj;
    obj.insert(QStringLiteral("placementMode"), static_cast<int>(opts.placementMode));
    obj.insert(QStringLiteral("scaleToEm"), opts.scaleToEm);
    obj.insert(QStringLiteral("scaleFactor"), opts.scaleFactor);
    obj.insert(QStringLiteral("simplify"), opts.simplify);
    obj.insert(QStringLiteral("correctDirection"), opts.correctDirection);
    obj.insert(QStringLiteral("removeOverlap"), opts.removeOverlap);
    obj.insert(QStringLiteral("roundToInt"), opts.roundToInt);
    obj.insert(QStringLiteral("marginRatio"), opts.marginRatio);
    obj.insert(QStringLiteral("offsetX"), opts.offsetX);
    obj.insert(QStringLiteral("offsetY"), opts.offsetY);
    obj.insert(QStringLiteral("normalizeToBlackFill"), opts.normalizeToBlackFill);
    obj.insert(QStringLiteral("whiteAsCutout"), opts.whiteAsCutout);
    obj.insert(QStringLiteral("keepStrokes"), opts.keepStrokes);
    obj.insert(QStringLiteral("stripTinyPaths"), opts.stripTinyPaths);
    obj.insert(QStringLiteral("minGeometryExtent"), opts.minGeometryExtent);
    obj.insert(QStringLiteral("useAlpha"), opts.useAlpha);
    obj.insert(QStringLiteral("bitmapThreshold"), opts.bitmapThreshold);
    obj.insert(QStringLiteral("invertBitmap"), opts.invertBitmap);
    obj.insert(QStringLiteral("minSpeckleArea"), opts.minSpeckleArea);
    obj.insert(QStringLiteral("curveSmoothing"), opts.curveSmoothing);

    QSettings settings(QStringLiteral("FontForgeGUI"), QStringLiteral("FontForgeGUI"));
    settings.setValue(QStringLiteral("defaultImportOptions"),
                      QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

inline void resetDefaultImportOptions() {
    QSettings settings(QStringLiteral("FontForgeGUI"), QStringLiteral("FontForgeGUI"));
    settings.remove(QStringLiteral("defaultImportOptions"));
}

} // namespace ffgui

