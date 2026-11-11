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
    void updateGlyphMetadata(const GlyphMetadataUpdate& update);
    void updateFontMetadata(const FontMetadataUpdate& update);
    void validateFont(bool force);
    void closeFont();

signals:
    void fontLoaded(const ffgui::FontDocumentSnapshot& snapshot);
    void fontLoadFailed(const QString& error);
    void fontExported(const QString& outputPath);
    void fontExportFailed(const QString& error);
    void glyphMetadataUpdated(const ffgui::GlyphSummary& glyph);
    void glyphMetadataUpdateFailed(const QString& error);
    void fontMetadataUpdated(const ffgui::FontSummary& summary);
    void fontMetadataUpdateFailed(const QString& error);
    void fontValidated(const ffgui::FontValidationReport& report);
    void fontValidationFailed(const QString& error);
    void fontClosed();
    void logMessage(const QString& level, const QString& message);
    void progressUpdated(int current, int total, const QString& status);

private:
    std::unique_ptr<FontForgeBridge> m_bridge;
};

} // namespace ffgui
