#pragma once

#include "backend/FontTypes.hpp"

#include <QJsonObject>
#include <QString>

namespace ffgui {

class BatchImportDefinition {
public:
    static bool loadFromFile(const QString& filePath, BatchImportJob* outJob,
                             QString* outError = nullptr);
    static bool saveToFile(const QString& filePath, const BatchImportJob& job,
                           QString* outError = nullptr);

    static bool fromJson(const QByteArray& jsonData, const QString& baseDir,
                         BatchImportJob* outJob, QString* outError = nullptr);
    static QByteArray toJson(const BatchImportJob& job, const QString& baseDir = QString());
};

} // namespace ffgui

