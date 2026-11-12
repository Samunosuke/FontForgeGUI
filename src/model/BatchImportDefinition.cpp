#include "model/BatchImportDefinition.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace ffgui {

namespace {

int32_t parseUnicodeValue(const QJsonValue& val) {
    if (val.isDouble()) {
        return static_cast<int32_t>(val.toInt());
    }
    if (val.isString()) {
        QString s = val.toString().trimmed();
        if (s.startsWith(QStringLiteral("U+"), Qt::CaseInsensitive) ||
            s.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
            s = s.mid(2);
        }
        bool ok = false;
        uint32_t num = s.toUInt(&ok, 16);
        if (ok && num <= 0x10FFFFu) {
            return static_cast<int32_t>(num);
        }
    }
    return -1;
}

GlyphPlacementMode parsePlacementMode(const QString& str) {
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

QString placementModeToString(GlyphPlacementMode mode) {
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

SvgImportOptions parseOptions(const QJsonObject& obj, const SvgImportOptions& fallback) {
    SvgImportOptions opts = fallback;
    if (obj.contains(QStringLiteral("placementMode"))) {
        opts.placementMode = parsePlacementMode(obj.value(QStringLiteral("placementMode")).toString());
    }
    if (obj.contains(QStringLiteral("scaleFactor"))) {
        opts.scaleFactor = obj.value(QStringLiteral("scaleFactor")).toDouble(opts.scaleFactor);
    }
    if (obj.contains(QStringLiteral("scaleToEm"))) {
        opts.scaleToEm = obj.value(QStringLiteral("scaleToEm")).toBool(opts.scaleToEm);
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
    return opts;
}

QJsonObject serializeOptions(const SvgImportOptions& opts) {
    QJsonObject obj;
    obj.insert(QStringLiteral("placementMode"), placementModeToString(opts.placementMode));
    obj.insert(QStringLiteral("scaleFactor"), opts.scaleFactor);
    obj.insert(QStringLiteral("scaleToEm"), opts.scaleToEm);
    obj.insert(QStringLiteral("simplify"), opts.simplify);
    obj.insert(QStringLiteral("correctDirection"), opts.correctDirection);
    obj.insert(QStringLiteral("removeOverlap"), opts.removeOverlap);
    obj.insert(QStringLiteral("roundToInt"), opts.roundToInt);
    obj.insert(QStringLiteral("marginRatio"), opts.marginRatio);
    obj.insert(QStringLiteral("offsetX"), opts.offsetX);
    obj.insert(QStringLiteral("offsetY"), opts.offsetY);
    return obj;
}

} // namespace

bool BatchImportDefinition::loadFromFile(const QString& filePath, BatchImportJob* outJob,
                                         QString* outError) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (outError) {
            *outError = QStringLiteral("Cannot open file %1: %2").arg(filePath, file.errorString());
        }
        return false;
    }
    const QString baseDir = QFileInfo(filePath).absolutePath();
    return fromJson(file.readAll(), baseDir, outJob, outError);
}

bool BatchImportDefinition::saveToFile(const QString& filePath, const BatchImportJob& job,
                                       QString* outError) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (outError) {
            *outError = QStringLiteral("Cannot write to file %1: %2").arg(filePath, file.errorString());
        }
        return false;
    }
    const QString baseDir = QFileInfo(filePath).absolutePath();
    const QByteArray data = toJson(job, baseDir);
    file.write(data);
    return true;
}

bool BatchImportDefinition::fromJson(const QByteArray& jsonData, const QString& baseDir,
                                     BatchImportJob* outJob, QString* outError) {
    if (!outJob) {
        return false;
    }

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (outError) {
            *outError = QStringLiteral("Invalid JSON definition: %1").arg(parseErr.errorString());
        }
        return false;
    }

    const QJsonObject root = doc.object();
    BatchImportJob job;
    job.overwriteExisting = root.value(QStringLiteral("overwriteExisting")).toBool(false);

    if (root.contains(QStringLiteral("defaults")) && root.value(QStringLiteral("defaults")).isObject()) {
        job.defaultOptions = parseOptions(root.value(QStringLiteral("defaults")).toObject(), job.defaultOptions);
    }

    const QJsonArray glyphsArr = root.value(QStringLiteral("glyphs")).toArray();
    for (const QJsonValue& gVal : glyphsArr) {
        if (!gVal.isObject()) {
            continue;
        }
        const QJsonObject gObj = gVal.toObject();
        QString relPath = gObj.value(QStringLiteral("file")).toString().trimmed();
        if (relPath.isEmpty()) {
            relPath = gObj.value(QStringLiteral("path")).toString().trimmed();
        }
        if (relPath.isEmpty()) {
            continue;
        }

        QString fullPath = relPath;
        if (!baseDir.isEmpty() && !QDir::isAbsolutePath(fullPath)) {
            fullPath = QDir(baseDir).filePath(relPath);
        }

        BatchImportItem item;
        item.filePath = fullPath;
        item.unicode = parseUnicodeValue(gObj.value(QStringLiteral("unicode")));
        item.glyphName = gObj.value(QStringLiteral("name")).toString().trimmed();

        if (gObj.contains(QStringLiteral("options")) && gObj.value(QStringLiteral("options")).isObject()) {
            item.customOptions = parseOptions(gObj.value(QStringLiteral("options")).toObject(), job.defaultOptions);
            item.hasCustomOptions = true;
        }

        job.items.append(std::move(item));
    }

    *outJob = std::move(job);
    return true;
}

QByteArray BatchImportDefinition::toJson(const BatchImportJob& job, const QString& baseDir) {
    QJsonObject root;
    root.insert(QStringLiteral("overwriteExisting"), job.overwriteExisting);
    root.insert(QStringLiteral("defaults"), serializeOptions(job.defaultOptions));

    QJsonArray glyphsArr;
    for (const BatchImportItem& item : job.items) {
        QJsonObject gObj;
        QString displayPath = item.filePath;
        if (!baseDir.isEmpty()) {
            displayPath = QDir(baseDir).relativeFilePath(item.filePath);
        }
        gObj.insert(QStringLiteral("file"), displayPath);
        if (item.unicode >= 0) {
            gObj.insert(QStringLiteral("unicode"),
                        QStringLiteral("U+%1").arg(item.unicode, 4, 16, QLatin1Char('0')).toUpper());
        }
        if (!item.glyphName.isEmpty()) {
            gObj.insert(QStringLiteral("name"), item.glyphName);
        }
        if (item.hasCustomOptions) {
            gObj.insert(QStringLiteral("options"), serializeOptions(item.customOptions));
        }
        glyphsArr.append(gObj);
    }
    root.insert(QStringLiteral("glyphs"), glyphsArr);

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace ffgui

