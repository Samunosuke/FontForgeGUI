#include "model/GlyphMetadataUtils.hpp"

#include <QCoreApplication>
#include <QRegularExpression>

namespace ffgui {

QString formatCodePoint(int32_t cp) {
    if (cp < 0) {
        return {};
    }
    return QStringLiteral("U+%1").arg(cp, 4, 16, QLatin1Char('0')).toUpper();
}

bool parseSingleCodePoint(QString text, int32_t* outCp) {
    text = text.trimmed();
    if (text.isEmpty() || text == QStringLiteral("-1")
        || text.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("unencoded"), Qt::CaseInsensitive) == 0
        || text == QStringLiteral("—")) {
        *outCp = -1;
        return true;
    }
    if (text.startsWith(QStringLiteral("U+"), Qt::CaseInsensitive)
        || text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        text = text.mid(2);
    }
    if (text.isEmpty() || text.size() > 6) {
        return false;
    }
    bool ok = false;
    const uint32_t val = text.toUInt(&ok, 16);
    if (!ok || val > 0x10FFFFu) {
        return false;
    }
    *outCp = static_cast<int32_t>(val);
    return true;
}

bool parseAltUnicodesList(const QString& text, QList<int32_t>* outAlts, QString* outError) {
    QList<int32_t> alts;
    const QString trimmed = text.trimmed();
    if (!trimmed.isEmpty()) {
        static const QRegularExpression kSepRe(QStringLiteral("[,;\\s]+"));
        const QStringList tokens = trimmed.split(kSepRe, Qt::SkipEmptyParts);
        for (const QString& tok : tokens) {
            int32_t altCp = -1;
            if (!parseSingleCodePoint(tok, &altCp) || altCp < 0) {
                if (outError) {
                    *outError = QCoreApplication::translate("GlyphMetadataUtils",
                                                            "Invalid alternate Unicode: '%1'")
                                    .arg(tok);
                }
                return false;
            }
            if (!alts.contains(altCp)) {
                alts.append(altCp);
            }
        }
    }
    if (outAlts) {
        *outAlts = std::move(alts);
    }
    return true;
}

bool validateGlyphName(const QString& name, QString* outError, bool allowEmpty) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        if (allowEmpty) {
            return true;
        }
        if (outError) {
            *outError = QCoreApplication::translate("GlyphMetadataUtils",
                                                    "Glyph name cannot be empty.");
        }
        return false;
    }
    for (QChar c : trimmed) {
        if (c.isSpace()) {
            if (outError) {
                *outError = QCoreApplication::translate("GlyphMetadataUtils",
                                                        "Glyph name cannot contain spaces.");
            }
            return false;
        }
    }
    return true;
}

QString suggestedGlyphName(int32_t unicode) {
    if (unicode < 0) {
        return {};
    }
    if (unicode <= 0xFFFF) {
        return QStringLiteral("uni") +
               QString::number(unicode, 16).rightJustified(4, QLatin1Char('0')).toUpper();
    }
    return QStringLiteral("u") +
           QString::number(unicode, 16).rightJustified(5, QLatin1Char('0')).toUpper();
}

QString formatUnicodePreviewBadge(const QString& unicodeText) {
    int32_t previewCp = -1;
    if (!parseSingleCodePoint(unicodeText, &previewCp)) {
        return QStringLiteral("?");
    }
    if (previewCp >= 0x20 && previewCp <= 0x10FFFF) {
        return QStringLiteral("'%1'").arg(
            QString::fromUcs4(reinterpret_cast<const char32_t*>(&previewCp), 1));
    }
    return QStringLiteral("—");
}

} // namespace ffgui
