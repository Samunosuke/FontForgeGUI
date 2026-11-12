#pragma once

#include "backend/FontTypes.hpp"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QPainterPath>
#include <QString>
#include <QVector>

namespace ffgui {

namespace ClipboardMimeTypes {
    inline const QString GlyphJson = QStringLiteral("application/x-fontforgegui-glyphs+json");
    inline const QString SvgXml = QStringLiteral("image/svg+xml");
    inline const QString TextPlain = QStringLiteral("text/plain");
} // namespace ClipboardMimeTypes

inline QString painterPathToSvgD(const QPainterPath& path, double scaleY = -1.0, double offsetY = 0.0) {
    QString d;
    for (int i = 0; i < path.elementCount(); ++i) {
        const auto el = path.elementAt(i);
        const double x = el.x;
        const double y = el.y * scaleY + offsetY;
        if (el.isMoveTo()) {
            d += QStringLiteral("M %1 %2 ").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);
        } else if (el.isLineTo()) {
            d += QStringLiteral("L %1 %2 ").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2);
        } else if (el.isCurveTo()) {
            if (i + 2 < path.elementCount()) {
                const auto p2 = path.elementAt(++i);
                const auto p3 = path.elementAt(++i);
                d += QStringLiteral("C %1 %2, %3 %4, %5 %6 ")
                         .arg(x, 0, 'f', 2).arg(y, 0, 'f', 2)
                         .arg(p2.x, 0, 'f', 2).arg(p2.y * scaleY + offsetY, 0, 'f', 2)
                         .arg(p3.x, 0, 'f', 2).arg(p3.y * scaleY + offsetY, 0, 'f', 2);
            }
        }
    }
    return d.trimmed();
}

inline QByteArray glyphToSvg(const GlyphSummary& glyph, int ascent = 800, int descent = 200) {
    const int emSize = (ascent + descent) > 0 ? (ascent + descent) : 1000;
    const int width = glyph.width > 0 ? glyph.width : emSize;
    const QString d = painterPathToSvgD(glyph.outline, -1.0, static_cast<double>(ascent));
    const QString svg = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %1 %2\" width=\"%1\" height=\"%2\">\n"
        "  <path fill=\"black\" fill-rule=\"evenodd\" d=\"%3\" />\n"
        "</svg>\n"
    ).arg(width).arg(emSize).arg(d);
    return svg.toUtf8();
}

inline QMimeData* serializeGlyphsToMimeData(const QVector<GlyphSummary>& glyphs,
                                            int ascent = 800, int descent = 200) {
    auto* mime = new QMimeData();
    if (glyphs.isEmpty()) {
        return mime;
    }

    QJsonArray arr;
    QString plainText;
    QByteArray firstSvg;

    for (const GlyphSummary& g : glyphs) {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), g.name);
        obj.insert(QStringLiteral("unicode"), g.unicode);
        obj.insert(QStringLiteral("width"), g.width);
        obj.insert(QStringLiteral("vwidth"), g.vwidth);

        QByteArray svgBytes = g.svgData;
        if (svgBytes.isEmpty()) {
            svgBytes = glyphToSvg(g, ascent, descent);
        }
        obj.insert(QStringLiteral("svgData"), QString::fromUtf8(svgBytes));
        arr.append(obj);

        if (firstSvg.isEmpty()) {
            firstSvg = svgBytes;
        }

        if (g.unicode >= 0) {
            plainText += QString::fromUcs4(reinterpret_cast<const char32_t*>(&g.unicode), 1);
        } else if (!g.name.isEmpty()) {
            plainText += QStringLiteral("/%1 ").arg(g.name);
        }
    }

    mime->setData(ClipboardMimeTypes::GlyphJson,
                  QJsonDocument(arr).toJson(QJsonDocument::Compact));

    if (!firstSvg.isEmpty()) {
        mime->setData(ClipboardMimeTypes::SvgXml, firstSvg);
    }
    if (!plainText.isEmpty()) {
        mime->setText(plainText);
    }

    return mime;
}

inline bool canPasteGlyphs(const QMimeData* mime) {
    if (!mime) {
        return false;
    }
    if (mime->hasFormat(ClipboardMimeTypes::GlyphJson) || mime->hasFormat(ClipboardMimeTypes::SvgXml)) {
        return true;
    }
    if (mime->hasText()) {
        const QString t = mime->text().trimmed();
        if (t.startsWith(QStringLiteral("<svg"), Qt::CaseInsensitive) ||
            t.contains(QStringLiteral("<path"), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

inline QVector<GlyphSummary> deserializeGlyphsFromMimeData(const QMimeData* mime) {
    QVector<GlyphSummary> result;
    if (!mime) {
        return result;
    }

    if (mime->hasFormat(ClipboardMimeTypes::GlyphJson)) {
        const QByteArray bytes = mime->data(ClipboardMimeTypes::GlyphJson);
        const QJsonDocument doc = QJsonDocument::fromJson(bytes);
        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            for (const QJsonValue& val : arr) {
                if (!val.isObject()) continue;
                const QJsonObject obj = val.toObject();
                GlyphSummary g;
                g.name = obj.value(QStringLiteral("name")).toString();
                g.unicode = obj.value(QStringLiteral("unicode")).toInt(-1);
                g.width = obj.value(QStringLiteral("width")).toInt(1000);
                g.vwidth = obj.value(QStringLiteral("vwidth")).toInt(1000);
                g.svgData = obj.value(QStringLiteral("svgData")).toString().toUtf8();
                result.append(g);
            }
            return result;
        }
    }

    // Fallback: single SVG payload from clipboard
    QByteArray svgBytes;
    if (mime->hasFormat(ClipboardMimeTypes::SvgXml)) {
        svgBytes = mime->data(ClipboardMimeTypes::SvgXml);
    } else if (mime->hasText()) {
        const QString t = mime->text().trimmed();
        if (t.startsWith(QStringLiteral("<svg"), Qt::CaseInsensitive)) {
            svgBytes = t.toUtf8();
        }
    }

    if (!svgBytes.isEmpty()) {
        GlyphSummary g;
        g.name = QStringLiteral("pasted_glyph");
        g.unicode = -1;
        g.width = 1000;
        g.svgData = svgBytes;
        result.append(g);
    }

    return result;
}

inline QByteArray getSvgFromMimeForGlyph(const QMimeData* mime, int index = 0) {
    if (!mime) return {};

    if (mime->hasFormat(ClipboardMimeTypes::GlyphJson)) {
        const QByteArray bytes = mime->data(ClipboardMimeTypes::GlyphJson);
        const QJsonDocument doc = QJsonDocument::fromJson(bytes);
        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            if (index >= 0 && index < arr.size() && arr[index].isObject()) {
                return arr[index].toObject().value(QStringLiteral("svgData")).toString().toUtf8();
            }
        }
    }

    if (mime->hasFormat(ClipboardMimeTypes::SvgXml)) {
        return mime->data(ClipboardMimeTypes::SvgXml);
    }
    if (mime->hasText() && mime->text().trimmed().startsWith(QStringLiteral("<svg"), Qt::CaseInsensitive)) {
        return mime->text().trimmed().toUtf8();
    }
    return {};
}

} // namespace ffgui
