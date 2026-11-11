#pragma once

#include "backend/FontTypes.hpp"

#include <QString>
#include <functional>
#include <memory>

namespace ffgui {

/**
 * Thread-unsafe direct adapter over headless libfontforge.
 * Must be constructed and invoked exclusively from a single dedicated worker thread.
 */
class FontForgeBridge {
public:
    using LogCallback = std::function<void(const QString& level, const QString& message)>;
    using ProgressCallback = std::function<void(int current, int total, const QString& status)>;

    FontForgeBridge();
    ~FontForgeBridge();

    FontForgeBridge(const FontForgeBridge&) = delete;
    FontForgeBridge& operator=(const FontForgeBridge&) = delete;

    void setLogCallback(LogCallback cb);
    void setProgressCallback(ProgressCallback cb);

    void initialize();

    bool openFont(const QString& path, FontDocumentSnapshot* outSnapshot, QString* outError);
    bool exportFont(const ExportOptions& options, QString* outError);
    bool updateGlyphMetadata(const GlyphMetadataUpdate& update, GlyphSummary* outGlyph,
                             QString* outError);
    bool updateFontMetadata(const FontMetadataUpdate& update, FontSummary* outSummary,
                            QString* outError);
    bool validateFont(bool force, FontValidationReport* outReport, QString* outError);
    void closeFont();

    bool hasOpenFont() const;

    static QStringList availableSfntStringIds();
    static QVector<QPair<int, QString>> commonSfntLanguages();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace ffgui
