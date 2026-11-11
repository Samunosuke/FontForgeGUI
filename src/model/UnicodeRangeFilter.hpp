#pragma once

#include "backend/FontTypes.hpp"

#include <QPair>
#include <QString>
#include <QVector>
#include <cstdint>

namespace ffgui {

struct UnicodeRange {
    int32_t start = 0;
    int32_t end = 0;

    bool contains(int32_t cp) const { return cp >= start && cp <= end; }
};

struct UnicodeRangePreset {
    QString name;
    QString rangeSpec;
    bool includeUnencoded = false;
};

/**
 * Parses and evaluates Unicode code point range expressions such as:
 *   "U+0020-007E, U+00A0-00FF, E000-E0FF, 20AC"
 * An empty specification (or "*") matches all encoded glyphs.
 */
class UnicodeRangeFilter {
public:
    static UnicodeRangeFilter parse(const QString& spec, bool includeUnencoded = true,
                                    QString* outError = nullptr);

    static QVector<UnicodeRangePreset> defaultPresets();

    bool isValid() const { return m_valid; }
    bool isMatchAll() const { return m_matchAll; }
    bool includeUnencoded() const { return m_includeUnencoded; }
    const QString& spec() const { return m_spec; }
    const QVector<UnicodeRange>& ranges() const { return m_ranges; }

    bool matches(const GlyphSummary& glyph) const;

private:
    QString m_spec;
    QVector<UnicodeRange> m_ranges;
    bool m_matchAll = true;
    bool m_includeUnencoded = true;
    bool m_valid = true;
};

} // namespace ffgui

