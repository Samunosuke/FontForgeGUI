#include "model/UnicodeRangeFilter.hpp"

#include <QRegularExpression>
#include <QStringList>

namespace ffgui {

namespace {

bool parseCodePoint(QString token, int32_t* outValue) {
    token = token.trimmed();
    if (token.startsWith(QStringLiteral("U+"), Qt::CaseInsensitive) ||
        token.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        token = token.mid(2);
    }
    if (token.isEmpty() || token.size() > 6) {
        return false;
    }
    bool ok = false;
    const uint32_t val = token.toUInt(&ok, 16);
    if (!ok || val > 0x10FFFFu) {
        return false;
    }
    *outValue = static_cast<int32_t>(val);
    return true;
}

} // namespace

UnicodeRangeFilter UnicodeRangeFilter::parse(const QString& spec, bool includeUnencoded,
                                             QString* outError) {
    UnicodeRangeFilter filter;
    filter.m_spec = spec.trimmed();
    filter.m_includeUnencoded = includeUnencoded;

    if (filter.m_spec.isEmpty() || filter.m_spec == QStringLiteral("*")) {
        filter.m_matchAll = true;
        filter.m_valid = true;
        return filter;
    }

    filter.m_matchAll = false;
    const QString normalized = QString(filter.m_spec)
                                   .replace(QStringLiteral(".."), QStringLiteral("-"))
                                   .replace(QChar(0x2013), QChar('-')) // en-dash
                                   .replace(QChar(0x2014), QChar('-'));
    const QStringList tokens =
        normalized.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);

    if (tokens.isEmpty()) {
        filter.m_matchAll = true;
        filter.m_valid = true;
        return filter;
    }

    for (const QString& token : tokens) {
        const qsizetype dashIdx = token.indexOf(QChar('-'));
        if (dashIdx < 0) {
            int32_t cp = -1;
            if (!parseCodePoint(token, &cp)) {
                filter.m_valid = false;
                if (outError) {
                    *outError = QStringLiteral("Invalid code point: '%1'").arg(token);
                }
                return filter;
            }
            filter.m_ranges.append({cp, cp});
        } else {
            const QString left = token.left(dashIdx);
            const QString right = token.mid(dashIdx + 1);
            int32_t start = -1;
            int32_t end = -1;
            if (!parseCodePoint(left, &start) || !parseCodePoint(right, &end)) {
                filter.m_valid = false;
                if (outError) {
                    *outError = QStringLiteral("Invalid range: '%1'").arg(token);
                }
                return filter;
            }
            if (start > end) {
                filter.m_valid = false;
                if (outError) {
                    *outError = QStringLiteral("Range start > end: '%1'").arg(token);
                }
                return filter;
            }
            filter.m_ranges.append({start, end});
        }
    }

    filter.m_valid = true;
    return filter;
}

QVector<UnicodeRangePreset> UnicodeRangeFilter::defaultPresets() {
    return {
        {QStringLiteral("All Glyphs"), QStringLiteral("*"), true},
        {QStringLiteral("Basic Latin (U+0020–007E)"), QStringLiteral("U+0020-007E"), false},
        {QStringLiteral("Latin-1 Supplement (U+00A0–00FF)"), QStringLiteral("U+00A0-00FF"), false},
        {QStringLiteral("Latin Extended-A (U+0100–017F)"), QStringLiteral("U+0100-017F"), false},
        {QStringLiteral("Icons / PUA (U+E000–F8FF)"), QStringLiteral("U+E000-F8FF"), false},
        {QStringLiteral("Private Use Plane 15 (U+F0000–FFFFD)"), QStringLiteral("U+F0000-FFFFD"),
         false},
    };
}

bool UnicodeRangeFilter::matches(const GlyphSummary& glyph) const {
    if (glyph.unicode < 0 && glyph.altUnicodes.isEmpty()) {
        return m_includeUnencoded;
    }
    if (m_matchAll) {
        return true;
    }
    for (const UnicodeRange& range : m_ranges) {
        if (glyph.unicode >= 0 && range.contains(glyph.unicode)) {
            return true;
        }
        for (int32_t alt : glyph.altUnicodes) {
            if (range.contains(alt)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace ffgui

