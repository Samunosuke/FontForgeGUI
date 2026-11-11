#pragma once

#include "backend/FontTypes.hpp"

#include <QObject>
#include <QString>
#include <memory>

namespace ffgui {

class FontForgeBridge;

/**
 * QObject executing all FontForge operations sequentially on its owning QThread.
 */
class FontWorker : public QObject {
    Q_OBJECT

public:
    explicit FontWorker(QObject* parent = nullptr);
    ~FontWorker() override;

public slots:
    void initialize();
    void openFont(const QString& path);
    void exportFont(const ExportOptions& options);
    void closeFont();

signals:
    void fontLoaded(const ffgui::FontDocumentSnapshot& snapshot);
    void fontLoadFailed(const QString& error);
    void fontExported(const QString& outputPath);
    void fontExportFailed(const QString& error);
    void fontClosed();
    void logMessage(const QString& level, const QString& message);
    void progressUpdated(int current, int total, const QString& status);

private:
    std::unique_ptr<FontForgeBridge> m_bridge;
};

} // namespace ffgui

