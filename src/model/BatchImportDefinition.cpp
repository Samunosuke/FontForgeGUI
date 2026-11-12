#include "model/BatchImportDefinition.hpp"
#include "model/FontSettingsUtils.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QRegularExpression>

namespace ffgui {

namespace DefinitionKeys {
    inline constexpr QLatin1StringView OverwriteExisting{"overwriteExisting"};
    inline constexpr QLatin1StringView Defaults{"defaults"};
    inline constexpr QLatin1StringView Glyphs{"glyphs"};
    inline constexpr QLatin1StringView File{"file"};
    inline constexpr QLatin1StringView Path{"path"};
    inline constexpr QLatin1StringView Unicode{"unicode"};
    inline constexpr QLatin1StringView Name{"name"};
    inline constexpr QLatin1StringView Options{"options"};
} // namespace DefinitionKeys

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
    job.overwriteExisting = root.value(DefinitionKeys::OverwriteExisting).toBool(false);

    if (root.contains(DefinitionKeys::Defaults) && root.value(DefinitionKeys::Defaults).isObject()) {
        job.defaultOptions = importOptionsFromJson(root.value(DefinitionKeys::Defaults).toObject(), job.defaultOptions);
    }

    const QJsonArray glyphsArr = root.value(DefinitionKeys::Glyphs).toArray();
    for (const QJsonValue& gVal : glyphsArr) {
        if (!gVal.isObject()) {
            continue;
        }
        const QJsonObject gObj = gVal.toObject();
        QString relPath = gObj.value(DefinitionKeys::File).toString().trimmed();
        if (relPath.isEmpty()) {
            relPath = gObj.value(DefinitionKeys::Path).toString().trimmed();
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
        item.unicode = parseUnicodeValue(gObj.value(DefinitionKeys::Unicode));
        item.glyphName = gObj.value(DefinitionKeys::Name).toString().trimmed();

        if (gObj.contains(DefinitionKeys::Options) && gObj.value(DefinitionKeys::Options).isObject()) {
            item.customOptions = importOptionsFromJson(gObj.value(DefinitionKeys::Options).toObject(), job.defaultOptions);
            item.hasCustomOptions = true;
        }

        job.items.append(std::move(item));
    }

    *outJob = std::move(job);
    return true;
}

QByteArray BatchImportDefinition::toJson(const BatchImportJob& job, const QString& baseDir) {
    QJsonObject root;
    root.insert(DefinitionKeys::OverwriteExisting, job.overwriteExisting);
    root.insert(DefinitionKeys::Defaults, importOptionsToJson(job.defaultOptions));

    QJsonArray glyphsArr;
    for (const BatchImportItem& item : job.items) {
        QJsonObject gObj;
        QString displayPath = item.filePath;
        if (!baseDir.isEmpty()) {
            displayPath = QDir(baseDir).relativeFilePath(item.filePath);
        }
        gObj.insert(DefinitionKeys::File, displayPath);
        if (item.unicode >= 0) {
            gObj.insert(DefinitionKeys::Unicode,
                        QStringLiteral("U+%1").arg(item.unicode, 4, 16, QLatin1Char('0')).toUpper());
        }
        if (!item.glyphName.isEmpty()) {
            gObj.insert(DefinitionKeys::Name, item.glyphName);
        }
        if (item.hasCustomOptions) {
            gObj.insert(DefinitionKeys::Options, importOptionsToJson(item.customOptions));
        }
        glyphsArr.append(gObj);
    }
    root.insert(DefinitionKeys::Glyphs, glyphsArr);

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace ffgui
