#pragma once

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QString>

namespace ffgui {

inline QString loadLastFontDirectory() {
    QSettings settings;
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
        QSettings settings;
        settings.setValue(QStringLiteral("lastFontDir"), dir);
    }
}

} // namespace ffgui

