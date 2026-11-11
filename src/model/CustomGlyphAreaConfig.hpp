#pragma once

#include "model/UnicodeRangeFilter.hpp"

#include <QString>
#include <QVector>

namespace ffgui {

struct CustomGlyphArea {
    QString name;
    QString rangeSpec;
    bool includeUnencoded = false;
};

/**
 * Persists dynamic Custom Glyph Areas in a sidecar file (<font>.ffgui.json)
 * when the font format itself does not store GUI metadata.
 */
struct CustomGlyphAreaConfig {
    int activeIndex = 0;
    QVector<CustomGlyphArea> areas;

    static QString sidecarPathForFont(const QString& fontPath);
    static CustomGlyphAreaConfig createDefault();

    static bool loadForFont(const QString& fontPath, CustomGlyphAreaConfig* outConfig,
                            QString* outError = nullptr);
    bool saveForFont(const QString& fontPath, QString* outError = nullptr) const;
};

} // namespace ffgui

