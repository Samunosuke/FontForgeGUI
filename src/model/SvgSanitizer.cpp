#include "model/SvgSanitizer.hpp"

#include <QColor>
#include <QDomDocument>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QSizeF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace ffgui {
namespace {

using StyleMap = QHash<QString, QString>;
using ClassStyleTable = QHash<QString, StyleMap>;

const QSet<QString>& editorMetadataTags() {
    static const QSet<QString> kTags = {
        QStringLiteral("metadata"),
        QStringLiteral("title"),
        QStringLiteral("desc"),
        QStringLiteral("script"),
        QStringLiteral("style"),
        QStringLiteral("namedview"),
    };
    return kTags;
}

const QSet<QString>& geometryTags() {
    static const QSet<QString> kTags = {
        QStringLiteral("path"),
        QStringLiteral("rect"),
        QStringLiteral("circle"),
        QStringLiteral("ellipse"),
        QStringLiteral("polygon"),
        QStringLiteral("polyline"),
        QStringLiteral("line"),
        QStringLiteral("use"),
    };
    return kTags;
}

QString localTagName(const QDomElement& el) {
    const QString tag = el.tagName();
    const int colon = tag.indexOf(QLatin1Char(':'));
    return (colon >= 0 ? tag.mid(colon + 1) : tag).toLower();
}

bool isEditorPrefixedName(const QString& name) {
    const QString lower = name.toLower();
    return lower.startsWith(QStringLiteral("sodipodi:"))
        || lower.startsWith(QStringLiteral("inkscape:"))
        || lower.startsWith(QStringLiteral("xmlns:sodipodi"))
        || lower.startsWith(QStringLiteral("xmlns:inkscape"))
        || lower.startsWith(QStringLiteral("xmlns:rdf"))
        || lower.startsWith(QStringLiteral("xmlns:cc"))
        || lower.startsWith(QStringLiteral("xmlns:dc"));
}

double parseSvgNumber(const QString& raw, double fallback = 0.0, bool* okOut = nullptr) {
    if (okOut) *okOut = false;
    QString s = raw.trimmed();
    if (s.isEmpty()) return fallback;

    static const QRegularExpression kUnitRe(
        QStringLiteral(R"(^([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)\s*(px|pt|pc|mm|cm|in|%)?$)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = kUnitRe.match(s);
    if (!m.hasMatch()) return fallback;

    bool ok = false;
    double val = m.captured(1).toDouble(&ok);
    if (!ok) return fallback;

    const QString unit = m.captured(2).toLower();
    if (unit == QStringLiteral("pt")) val *= 96.0 / 72.0;
    else if (unit == QStringLiteral("pc")) val *= 16.0;
    else if (unit == QStringLiteral("mm")) val *= 96.0 / 25.4;
    else if (unit == QStringLiteral("cm")) val *= 96.0 / 2.54;
    else if (unit == QStringLiteral("in")) val *= 96.0;

    if (okOut) *okOut = true;
    return val;
}

QRectF parseViewBox(const QString& raw) {
    if (raw.trimmed().isEmpty()) return {};
    static const QRegularExpression kSepRe(QStringLiteral(R"([\s,]+)"));
    const QStringList parts = raw.trimmed().split(kSepRe, Qt::SkipEmptyParts);
    if (parts.size() != 4) return {};
    bool ok[4]{};
    const double x = parts[0].toDouble(&ok[0]);
    const double y = parts[1].toDouble(&ok[1]);
    const double w = parts[2].toDouble(&ok[2]);
    const double h = parts[3].toDouble(&ok[3]);
    if (!ok[0] || !ok[1] || !ok[2] || !ok[3] || w <= 0.0 || h <= 0.0) return {};
    return QRectF(x, y, w, h);
}

StyleMap parseStyleDeclarations(const QString& styleText) {
    StyleMap result;
    const QStringList decls = styleText.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& decl : decls) {
        const int colon = decl.indexOf(QLatin1Char(':'));
        if (colon <= 0) continue;
        const QString key = decl.left(colon).trimmed().toLower();
        const QString val = decl.mid(colon + 1).trimmed();
        if (!key.isEmpty()) result.insert(key, val);
    }
    return result;
}

QString serializeStyleDeclarations(const StyleMap& map) {
    QStringList keys = map.keys();
    std::sort(keys.begin(), keys.end());
    QStringList parts;
    parts.reserve(keys.size());
    for (const QString& k : keys) {
        parts.append(k + QLatin1Char(':') + map.value(k));
    }
    return parts.join(QLatin1Char(';'));
}

ClassStyleTable extractCssClassRules(const QDomElement& root) {
    ClassStyleTable table;
    const QDomNodeList styles = root.elementsByTagName(QStringLiteral("style"));
    static const QRegularExpression kRuleRe(QStringLiteral(R"(\.([A-Za-z0-9_-]+)\s*\{([^}]*)\})"));
    for (int i = 0; i < styles.count(); ++i) {
        const QString css = styles.at(i).toElement().text();
        auto it = kRuleRe.globalMatch(css);
        while (it.hasNext()) {
            const auto m = it.next();
            table.insert(m.captured(1), parseStyleDeclarations(m.captured(2)));
        }
    }
    return table;
}

struct PresentationState {
    QString fill{QStringLiteral("black")};
    QString stroke{QStringLiteral("none")};
    double opacity{1.0};
    double fillOpacity{1.0};
    double strokeOpacity{1.0};
    double strokeWidth{1.0};
    QString fillRule;
    bool displayNone{false};
    bool visibilityHidden{false};
};

QString readEffectiveProp(const QDomElement& el, const StyleMap& inlineStyle,
                          const StyleMap& classStyle, const QString& prop) {
    if (inlineStyle.contains(prop)) return inlineStyle.value(prop);
    if (el.hasAttribute(prop)) return el.attribute(prop).trimmed();
    if (classStyle.contains(prop)) return classStyle.value(prop);
    return {};
}

PresentationState resolvePresentation(const QDomElement& el, const PresentationState& parent,
                                      const ClassStyleTable& classStyles) {
    PresentationState state = parent;
    StyleMap classStyle;
    const QString clsAttr = el.attribute(QStringLiteral("class")).trimmed();
    if (!clsAttr.isEmpty()) {
        static const QRegularExpression kWsRe(QStringLiteral(R"(\s+)"));
        for (const QString& cls : clsAttr.split(kWsRe, Qt::SkipEmptyParts)) {
            if (classStyles.contains(cls)) {
                const StyleMap& cs = classStyles.value(cls);
                for (auto it = cs.cbegin(); it != cs.cend(); ++it) {
                    classStyle.insert(it.key(), it.value());
                }
            }
        }
    }
    const StyleMap inlineStyle = parseStyleDeclarations(el.attribute(QStringLiteral("style")));

    auto prop = [&](const QString& name) {
        return readEffectiveProp(el, inlineStyle, classStyle, name);
    };

    if (const QString v = prop(QStringLiteral("display")); !v.isEmpty()) {
        state.displayNone = (v.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0);
    }
    if (const QString v = prop(QStringLiteral("visibility")); !v.isEmpty()) {
        const QString low = v.toLower();
        state.visibilityHidden = (low == QStringLiteral("hidden") || low == QStringLiteral("collapse"));
    }
    if (const QString v = prop(QStringLiteral("opacity")); !v.isEmpty()) {
        bool ok = false;
        const double d = v.toDouble(&ok);
        if (ok) state.opacity *= std::clamp(d, 0.0, 1.0);
    }
    if (const QString v = prop(QStringLiteral("fill")); !v.isEmpty()) state.fill = v;
    if (const QString v = prop(QStringLiteral("stroke")); !v.isEmpty()) state.stroke = v;
    if (const QString v = prop(QStringLiteral("fill-opacity")); !v.isEmpty()) {
        bool ok = false;
        const double d = v.toDouble(&ok);
        if (ok) state.fillOpacity = std::clamp(d, 0.0, 1.0);
    }
    if (const QString v = prop(QStringLiteral("stroke-opacity")); !v.isEmpty()) {
        bool ok = false;
        const double d = v.toDouble(&ok);
        if (ok) state.strokeOpacity = std::clamp(d, 0.0, 1.0);
    }
    if (const QString v = prop(QStringLiteral("stroke-width")); !v.isEmpty()) {
        bool ok = false;
        const double d = parseSvgNumber(v, state.strokeWidth, &ok);
        if (ok) state.strokeWidth = d;
    }
    if (const QString v = prop(QStringLiteral("fill-rule")); !v.isEmpty()) {
        state.fillRule = v.toLower();
    }
    return state;
}

bool isWhiteOrNearWhite(const QString& normalizedHex) {
    const QColor c(normalizedHex);
    if (!c.isValid()) return false;
    return c.red() >= 250 && c.green() >= 250 && c.blue() >= 250;
}

bool hasVisibleFill(const PresentationState& st) {
    const QString norm = SvgSanitizer::normalizeColor(st.fill);
    return !norm.isEmpty() && st.fillOpacity > 0.0 && st.opacity > 0.0;
}

bool hasVisibleStroke(const PresentationState& st) {
    const QString norm = SvgSanitizer::normalizeColor(st.stroke);
    return !norm.isEmpty() && st.strokeOpacity > 0.0 && st.opacity > 0.0 && st.strokeWidth > 0.0;
}

QSizeF estimatePointsExtent(const QString& rawPoints) {
    static const QRegularExpression kNumRe(QStringLiteral(R"([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)"));
    auto it = kNumRe.globalMatch(rawPoints);
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = minX;
    double maxY = maxX;
    int idx = 0;
    double pendingX = 0.0;
    while (it.hasNext()) {
        const double val = it.next().captured(0).toDouble();
        if ((idx % 2) == 0) {
            pendingX = val;
        } else {
            minX = std::min(minX, pendingX);
            maxX = std::max(maxX, pendingX);
            minY = std::min(minY, val);
            maxY = std::max(maxY, val);
        }
        ++idx;
    }
    if (idx < 2) return {0.0, 0.0};
    return {maxX - minX, maxY - minY};
}

QSizeF estimatePathExtent(const QString& d) {
    static const QRegularExpression kTokRe(
        QStringLiteral(R"(([A-Za-z])|([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?))"));
    auto it = kTokRe.globalMatch(d);

    double curX = 0.0, curY = 0.0, startX = 0.0, startY = 0.0;
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = minX;
    double maxY = maxX;
    bool hasPoint = false;

    auto record = [&](double x, double y) {
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
        hasPoint = true;
    };

    QChar cmd;
    QVector<double> args;
    while (it.hasNext()) {
        const auto m = it.next();
        if (!m.captured(1).isEmpty()) {
            cmd = m.captured(1).at(0);
            args.clear();
            if (cmd == QLatin1Char('Z') || cmd == QLatin1Char('z')) {
                curX = startX;
                curY = startY;
            }
            continue;
        }
        args.append(m.captured(2).toDouble());
        const QChar upper = cmd.toUpper();
        const bool rel = cmd.isLower();

        if (upper == QLatin1Char('M') || upper == QLatin1Char('L') || upper == QLatin1Char('T')) {
            if (args.size() == 2) {
                curX = (rel ? curX : 0.0) + args[0];
                curY = (rel ? curY : 0.0) + args[1];
                record(curX, curY);
                if (upper == QLatin1Char('M')) {
                    startX = curX;
                    startY = curY;
                    cmd = rel ? QLatin1Char('l') : QLatin1Char('L');
                }
                args.clear();
            }
        } else if (upper == QLatin1Char('H')) {
            curX = (rel ? curX : 0.0) + args[0];
            record(curX, curY);
            args.clear();
        } else if (upper == QLatin1Char('V')) {
            curY = (rel ? curY : 0.0) + args[0];
            record(curX, curY);
            args.clear();
        } else if (upper == QLatin1Char('C')) {
            if (args.size() == 6) {
                const double bx = rel ? curX : 0.0;
                const double by = rel ? curY : 0.0;
                record(bx + args[0], by + args[1]);
                record(bx + args[2], by + args[3]);
                curX = bx + args[4];
                curY = by + args[5];
                record(curX, curY);
                args.clear();
            }
        } else if (upper == QLatin1Char('S') || upper == QLatin1Char('Q')) {
            if (args.size() == 4) {
                const double bx = rel ? curX : 0.0;
                const double by = rel ? curY : 0.0;
                record(bx + args[0], by + args[1]);
                curX = bx + args[2];
                curY = by + args[3];
                record(curX, curY);
                args.clear();
            }
        } else if (upper == QLatin1Char('A')) {
            if (args.size() == 7) {
                curX = (rel ? curX : 0.0) + args[5];
                curY = (rel ? curY : 0.0) + args[6];
                record(curX, curY);
                args.clear();
            }
        }
    }
    if (!hasPoint) return {0.0, 0.0};
    return {maxX - minX, maxY - minY};
}

QSizeF estimateShapeExtent(const QDomElement& el, const QString& tag) {
    if (tag == QStringLiteral("path")) {
        return estimatePathExtent(el.attribute(QStringLiteral("d")));
    }
    if (tag == QStringLiteral("rect")) {
        return {parseSvgNumber(el.attribute(QStringLiteral("width"))),
                parseSvgNumber(el.attribute(QStringLiteral("height")))};
    }
    if (tag == QStringLiteral("circle")) {
        const double d = 2.0 * parseSvgNumber(el.attribute(QStringLiteral("r")));
        return {d, d};
    }
    if (tag == QStringLiteral("ellipse")) {
        return {2.0 * parseSvgNumber(el.attribute(QStringLiteral("rx"))),
                2.0 * parseSvgNumber(el.attribute(QStringLiteral("ry")))};
    }
    if (tag == QStringLiteral("line")) {
        const double x1 = parseSvgNumber(el.attribute(QStringLiteral("x1")));
        const double y1 = parseSvgNumber(el.attribute(QStringLiteral("y1")));
        const double x2 = parseSvgNumber(el.attribute(QStringLiteral("x2")));
        const double y2 = parseSvgNumber(el.attribute(QStringLiteral("y2")));
        return {std::abs(x2 - x1), std::abs(y2 - y1)};
    }
    if (tag == QStringLiteral("polygon") || tag == QStringLiteral("polyline")) {
        return estimatePointsExtent(el.attribute(QStringLiteral("points")));
    }
    return {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
}

bool isBackgroundRect(const QDomElement& el, const PresentationState& st, const QRectF& viewBox) {
    if (hasVisibleStroke(st)) return false;
    const double w = parseSvgNumber(el.attribute(QStringLiteral("width")));
    const double h = parseSvgNumber(el.attribute(QStringLiteral("height")));
    const QString fillColor = SvgSanitizer::normalizeColor(st.fill);
    const bool whiteOrNoFill = fillColor.isEmpty() || isWhiteOrNearWhite(fillColor);
    if (!whiteOrNoFill) return false;

    if (viewBox.isValid()) {
        return w >= viewBox.width() * 0.90 && h >= viewBox.height() * 0.90;
    }
    const double rx = parseSvgNumber(el.attribute(QStringLiteral("rx")));
    const double ry = parseSvgNumber(el.attribute(QStringLiteral("ry")));
    return fillColor.isEmpty() || (rx <= 0.0 && ry <= 0.0 && w >= 100.0 && h >= 100.0);
}

void stripEditorAttributes(QDomElement& el) {
    const QDomNamedNodeMap attrs = el.attributes();
    QStringList toRemove;
    for (int i = 0; i < attrs.count(); ++i) {
        const QString name = attrs.item(i).nodeName();
        if (isEditorPrefixedName(name)) toRemove.append(name);
    }
    for (const QString& name : toRemove) el.removeAttribute(name);
}

void cleanPresentationOnGroup(QDomElement& el) {
    el.removeAttribute(QStringLiteral("fill"));
    el.removeAttribute(QStringLiteral("stroke"));
    el.removeAttribute(QStringLiteral("fill-opacity"));
    el.removeAttribute(QStringLiteral("stroke-opacity"));
    el.removeAttribute(QStringLiteral("opacity"));
    el.removeAttribute(QStringLiteral("class"));
    if (el.hasAttribute(QStringLiteral("style"))) {
        StyleMap s = parseStyleDeclarations(el.attribute(QStringLiteral("style")));
        for (const QString& k : {QStringLiteral("fill"), QStringLiteral("stroke"),
                                 QStringLiteral("fill-opacity"), QStringLiteral("stroke-opacity"),
                                 QStringLiteral("opacity")}) {
            s.remove(k);
        }
        if (s.isEmpty()) el.removeAttribute(QStringLiteral("style"));
        else el.setAttribute(QStringLiteral("style"), serializeStyleDeclarations(s));
    }
}

void normalizeGeometryAttributes(QDomElement& el, const PresentationState& st,
                                 const SvgFilterOptions& options, bool isCutout,
                                 bool visFill, bool visStroke) {
    if (!options.normalizeToBlackFill && options.keepStrokes) return;

    const QString targetColor = (isCutout && options.whiteAsCutout)
        ? QStringLiteral("#ffffff")
        : QStringLiteral("#000000");

    el.removeAttribute(QStringLiteral("class"));
    el.removeAttribute(QStringLiteral("opacity"));
    el.removeAttribute(QStringLiteral("fill-opacity"));
    el.removeAttribute(QStringLiteral("stroke-opacity"));

    if (options.normalizeToBlackFill) {
        if (visFill) {
            el.setAttribute(QStringLiteral("fill"), targetColor);
        } else {
            el.setAttribute(QStringLiteral("fill"), QStringLiteral("none"));
        }
        if (!st.fillRule.isEmpty()) {
            el.setAttribute(QStringLiteral("fill-rule"), st.fillRule);
        }
        if (visStroke) {
            el.setAttribute(QStringLiteral("stroke"), targetColor);
        }
    }
    if (!options.keepStrokes) {
        el.setAttribute(QStringLiteral("stroke"), QStringLiteral("none"));
        el.removeAttribute(QStringLiteral("stroke-width"));
        el.removeAttribute(QStringLiteral("stroke-linecap"));
        el.removeAttribute(QStringLiteral("stroke-linejoin"));
        el.removeAttribute(QStringLiteral("stroke-dasharray"));
    }

    if (el.hasAttribute(QStringLiteral("style"))) {
        // Inline style overrides presentation attributes in SVG; remove cleaned keys.
        StyleMap s = parseStyleDeclarations(el.attribute(QStringLiteral("style")));
        for (const QString& k : {QStringLiteral("fill"), QStringLiteral("fill-opacity"),
                                 QStringLiteral("opacity"), QStringLiteral("stroke-opacity"),
                                 QStringLiteral("filter")}) {
            if (options.normalizeToBlackFill || k == QStringLiteral("opacity")) s.remove(k);
        }
        if (!options.keepStrokes) {
            for (const QString& k : {QStringLiteral("stroke"), QStringLiteral("stroke-width"),
                                     QStringLiteral("stroke-linecap"), QStringLiteral("stroke-linejoin"),
                                     QStringLiteral("stroke-dasharray")}) {
                s.remove(k);
            }
        } else if (options.normalizeToBlackFill) {
            s.remove(QStringLiteral("stroke"));
        }
        if (s.isEmpty()) el.removeAttribute(QStringLiteral("style"));
        else el.setAttribute(QStringLiteral("style"), serializeStyleDeclarations(s));
    }
}

enum class VariantKind { FillOnly, StrokeOnly, CutoutAll };

void removeInlineStyleKeys(QDomElement& el, const QStringList& keys) {
    if (!el.hasAttribute(QStringLiteral("style"))) return;
    StyleMap s = parseStyleDeclarations(el.attribute(QStringLiteral("style")));
    for (const QString& k : keys) s.remove(k);
    if (s.isEmpty()) el.removeAttribute(QStringLiteral("style"));
    else el.setAttribute(QStringLiteral("style"), serializeStyleDeclarations(s));
}

bool pruneVariantTree(QDomElement el, VariantKind kind, bool recolorToBlack, bool insideDefs) {
    const QString tag = localTagName(el);
    const QString cutoutMarker = QStringLiteral("data-ffgui-cutout");
    const QString fillMarker = QStringLiteral("data-ffgui-fill");
    const QString strokeMarker = QStringLiteral("data-ffgui-stroke");
    const bool nextDefs = insideDefs || (tag == QStringLiteral("defs"));

    if (!insideDefs && geometryTags().contains(tag)) {
        const bool isCutout = el.hasAttribute(cutoutMarker);
        const bool hasFill = el.hasAttribute(fillMarker);
        const bool hasStroke = el.hasAttribute(strokeMarker);
        el.removeAttribute(cutoutMarker);
        el.removeAttribute(fillMarker);
        el.removeAttribute(strokeMarker);

        const bool keep =
            (kind == VariantKind::CutoutAll) ? isCutout
            : (kind == VariantKind::FillOnly) ? (!isCutout && hasFill)
                                              : (!isCutout && hasStroke);
        if (!keep) return false;

        if (kind == VariantKind::FillOnly) {
            el.setAttribute(QStringLiteral("stroke"), QStringLiteral("none"));
            el.removeAttribute(QStringLiteral("stroke-width"));
            removeInlineStyleKeys(el, {QStringLiteral("stroke"), QStringLiteral("stroke-width")});
        } else if (kind == VariantKind::StrokeOnly) {
            el.setAttribute(QStringLiteral("fill"), QStringLiteral("none"));
            removeInlineStyleKeys(el, {QStringLiteral("fill")});
        }
        if (recolorToBlack) {
            if (hasFill || el.attribute(QStringLiteral("fill")).compare(QStringLiteral("#ffffff"), Qt::CaseInsensitive) == 0) {
                if (el.attribute(QStringLiteral("fill")) != QStringLiteral("none")) {
                    el.setAttribute(QStringLiteral("fill"), QStringLiteral("#000000"));
                }
            }
            if (el.attribute(QStringLiteral("stroke")).compare(QStringLiteral("#ffffff"), Qt::CaseInsensitive) == 0) {
                el.setAttribute(QStringLiteral("stroke"), QStringLiteral("#000000"));
            }
        }
        return true;
    }

    QList<QDomNode> removals;
    for (QDomNode child = el.firstChild(); !child.isNull(); child = child.nextSibling()) {
        if (!child.isElement()) continue;
        if (!pruneVariantTree(child.toElement(), kind, recolorToBlack, nextDefs)) {
            removals.append(child);
        }
    }
    for (const QDomNode& rem : removals) el.removeChild(rem);

    if (tag == QStringLiteral("g")) {
        bool hasChildEl = false;
        for (QDomNode child = el.firstChild(); !child.isNull(); child = child.nextSibling()) {
            if (child.isElement()) {
                hasChildEl = true;
                break;
            }
        }
        return hasChildEl;
    }
    return true;
}

void stripInternalMarkers(QDomElement el) {
    el.removeAttribute(QStringLiteral("data-ffgui-cutout"));
    el.removeAttribute(QStringLiteral("data-ffgui-fill"));
    el.removeAttribute(QStringLiteral("data-ffgui-stroke"));
    for (QDomNode child = el.firstChild(); !child.isNull(); child = child.nextSibling()) {
        if (child.isElement()) stripInternalMarkers(child.toElement());
    }
}

int countElements(const QDomElement& root) {
    int count = 0;
    for (QDomNode n = root.firstChild(); !n.isNull(); n = n.nextSibling()) {
        if (n.isElement()) count += 1 + countElements(n.toElement());
    }
    return count;
}

} // namespace

QString SvgSanitizer::normalizeColor(const QString& rawColor) {
    const QString s = rawColor.trimmed();
    if (s.isEmpty()) return {};
    const QString low = s.toLower();
    if (low == QStringLiteral("none") || low == QStringLiteral("transparent")
        || low.startsWith(QStringLiteral("url(")) || low == QStringLiteral("inherit")) {
        return {};
    }
    if (low == QStringLiteral("currentcolor")) return QStringLiteral("#000000");

    static const QRegularExpression kRgbRe(
        QStringLiteral(R"(^rgba?\(\s*([0-9.]+%?)\s*[,\s]\s*([0-9.]+%?)\s*[,\s]\s*([0-9.]+%?)(?:\s*[,/]\s*([0-9.]+%?))?\s*\)$)"),
        QRegularExpression::CaseInsensitiveOption);
    if (const auto m = kRgbRe.match(s); m.hasMatch()) {
        auto parseChan = [](const QString& tok) -> int {
            if (tok.endsWith(QLatin1Char('%'))) {
                const double pct = tok.left(tok.size() - 1).toDouble();
                return std::clamp(static_cast<int>(std::lround(pct * 2.55)), 0, 255);
            }
            return std::clamp(static_cast<int>(std::lround(tok.toDouble())), 0, 255);
        };
        if (!m.captured(4).isEmpty()) {
            const QString aTok = m.captured(4);
            const double alpha = aTok.endsWith(QLatin1Char('%'))
                ? aTok.left(aTok.size() - 1).toDouble() / 100.0
                : aTok.toDouble();
            if (alpha <= 0.0) return {};
        }
        return QColor(parseChan(m.captured(1)), parseChan(m.captured(2)), parseChan(m.captured(3)))
            .name(QColor::HexRgb);
    }

    if (low.startsWith(QLatin1Char('#')) && low.size() == 9) {
        // CSS #RRGGBBAA
        const int alpha = low.mid(7, 2).toInt(nullptr, 16);
        if (alpha <= 0) return {};
        const QColor c(low.left(7));
        return c.isValid() ? c.name(QColor::HexRgb) : QString{};
    }

    const QColor c = QColor::fromString(low);
    if (!c.isValid() || c.alpha() == 0) return {};
    return c.name(QColor::HexRgb);
}

SvgSanitizeResult SvgSanitizer::sanitize(const QByteArray& rawSvg, const SvgFilterOptions& options) {
    SvgSanitizeResult result;
    if (rawSvg.trimmed().isEmpty()) {
        result.errorMessage = QStringLiteral("Empty SVG input.");
        return result;
    }

    QDomDocument doc;
    const auto parseRes = doc.setContent(rawSvg, QDomDocument::ParseOption::Default);
    if (!parseRes) {
        result.errorMessage = QStringLiteral("Invalid SVG XML at line %1, column %2: %3")
                                  .arg(parseRes.errorLine)
                                  .arg(parseRes.errorColumn)
                                  .arg(parseRes.errorMessage);
        return result;
    }

    QDomElement root = doc.documentElement();
    if (root.isNull() || localTagName(root) != QStringLiteral("svg")) {
        result.errorMessage = QStringLiteral("Root element is not <svg>.");
        return result;
    }

    result.totalElementsBefore = countElements(root);
    result.viewBox = parseViewBox(root.attribute(QStringLiteral("viewBox")));
    result.width = parseSvgNumber(root.attribute(QStringLiteral("width")), result.viewBox.width());
    result.height = parseSvgNumber(root.attribute(QStringLiteral("height")), result.viewBox.height());
    if (!result.viewBox.isValid() && result.width > 0.0 && result.height > 0.0) {
        result.viewBox = QRectF(0.0, 0.0, result.width, result.height);
    }

    const ClassStyleTable classStyles = extractCssClassRules(root);

    QSet<QString> excludedSet;
    for (const QString& c : options.excludedColors) {
        if (const QString n = normalizeColor(c); !n.isEmpty()) excludedSet.insert(n);
    }
    QSet<QString> includedSet;
    for (const QString& c : options.includedColors) {
        if (const QString n = normalizeColor(c); !n.isEmpty()) includedSet.insert(n);
    }

    QSet<QString> detectedColorSet;
    int solidFillCount = 0;
    int solidStrokeCount = 0;
    int cutoutCount = 0;

    auto processElement = [&](auto& self, QDomElement el, const PresentationState& parentState,
                              bool insideDefs) -> bool {
        const QString tag = localTagName(el);
        const bool editorNode = isEditorPrefixedName(el.tagName()) || editorMetadataTags().contains(tag);

        if (options.stripEditorMetadata && editorNode) {
            ++result.removedMetadataCount;
            return false;
        }
        if (options.stripEditorMetadata) stripEditorAttributes(el);

        if (insideDefs && options.normalizeToBlackFill
            && (tag == QStringLiteral("lineargradient") || tag == QStringLiteral("radialgradient")
                || tag == QStringLiteral("filter"))) {
            ++result.removedMetadataCount;
            return false;
        }

        const PresentationState state = resolvePresentation(el, parentState, classStyles);
        if (options.removeHiddenElements && (state.displayNone || state.visibilityHidden || state.opacity <= 0.0)) {
            ++result.removedHiddenCount;
            return false;
        }

        const bool nextDefs = insideDefs || (tag == QStringLiteral("defs"));
        if (!insideDefs && geometryTags().contains(tag)) {
            const bool visFill = hasVisibleFill(state);
            const bool visStroke = options.keepStrokes && hasVisibleStroke(state);

            if (options.removeBackgroundRects && tag == QStringLiteral("rect")
                && isBackgroundRect(el, state, result.viewBox)) {
                ++result.removedHiddenCount;
                return false;
            }

            if (options.stripNoFillElements && !visFill && !visStroke) {
                ++result.removedHiddenCount;
                return false;
            }

            const QString effectiveColor = visFill
                ? normalizeColor(state.fill)
                : normalizeColor(state.stroke);
            if (!effectiveColor.isEmpty()) detectedColorSet.insert(effectiveColor);

            if (!excludedSet.isEmpty() && !effectiveColor.isEmpty() && excludedSet.contains(effectiveColor)) {
                ++result.removedColorFilteredCount;
                return false;
            }
            if (!includedSet.isEmpty() && (effectiveColor.isEmpty() || !includedSet.contains(effectiveColor))) {
                ++result.removedColorFilteredCount;
                return false;
            }

            if (options.stripTinyPaths && tag != QStringLiteral("use")) {
                const QSizeF ext = estimateShapeExtent(el, tag);
                const double strokePad = visStroke ? state.strokeWidth : 0.0;
                if (std::max(ext.width() + strokePad, ext.height() + strokePad) <
                    options.minGeometryExtent) {
                    ++result.removedTinyOrEmptyCount;
                    return false;
                }
            }

            const bool isCutout = options.whiteAsCutout && !effectiveColor.isEmpty()
                                  && isWhiteOrNearWhite(effectiveColor);
            normalizeGeometryAttributes(el, state, options, isCutout, visFill, visStroke);
            if (visFill) el.setAttribute(QStringLiteral("data-ffgui-fill"), QStringLiteral("1"));
            if (visStroke) el.setAttribute(QStringLiteral("data-ffgui-stroke"), QStringLiteral("1"));
            if (isCutout) {
                el.setAttribute(QStringLiteral("data-ffgui-cutout"), QStringLiteral("1"));
                ++cutoutCount;
            } else {
                if (visFill) ++solidFillCount;
                if (visStroke) ++solidStrokeCount;
            }
            ++result.keptGeometryCount;
            return true;
        }

        QList<QDomNode> toRemove;
        for (QDomNode child = el.firstChild(); !child.isNull(); child = child.nextSibling()) {
            if (child.isComment()) {
                if (options.stripEditorMetadata) toRemove.append(child);
                continue;
            }
            if (!child.isElement()) continue;
            if (!self(self, child.toElement(), state, nextDefs)) {
                toRemove.append(child);
            }
        }
        for (const QDomNode& rem : toRemove) el.removeChild(rem);

        if ((tag == QStringLiteral("g") || tag == QStringLiteral("defs"))
            && options.removeHiddenElements) {
            bool hasChildElement = false;
            for (QDomNode child = el.firstChild(); !child.isNull(); child = child.nextSibling()) {
                if (child.isElement()) {
                    hasChildElement = true;
                    break;
                }
            }
            if (!hasChildElement) {
                ++result.removedTinyOrEmptyCount;
                return false;
            }
        }

        if (tag == QStringLiteral("g") && options.normalizeToBlackFill) {
            cleanPresentationOnGroup(el);
        }
        return true;
    };

    if (options.stripEditorMetadata) stripEditorAttributes(root);
    const PresentationState rootState = resolvePresentation(root, PresentationState{}, classStyles);

    QList<QDomNode> rootRemovals;
    for (QDomNode child = root.firstChild(); !child.isNull(); child = child.nextSibling()) {
        if (child.isComment()) {
            if (options.stripEditorMetadata) rootRemovals.append(child);
            continue;
        }
        if (!child.isElement()) continue;
        if (!processElement(processElement, child.toElement(), rootState, false)) {
            rootRemovals.append(child);
        }
    }
    for (const QDomNode& rem : rootRemovals) root.removeChild(rem);

    if (options.normalizeToBlackFill) cleanPresentationOnGroup(root);

    const int solidCount = solidFillCount + solidStrokeCount;
    if (cutoutCount > 0 && solidCount == 0) {
        // Pure-white icon on transparent canvas: treat kept white shapes as positive geometry.
        pruneVariantTree(root, VariantKind::CutoutAll, options.normalizeToBlackFill, false);
        result.cleanedSvg = doc.toByteArray(2);
        result.solidSvg = result.cleanedSvg;
        result.strokeSvg.clear();
        result.cutoutSvg.clear();
        result.cutoutGeometryCount = 0;
    } else {
        if (solidFillCount > 0) {
            QDomDocument solidDoc = doc.cloneNode(true).toDocument();
            pruneVariantTree(solidDoc.documentElement(), VariantKind::FillOnly, false, false);
            result.solidSvg = solidDoc.toByteArray(2);
        }
        if (solidStrokeCount > 0) {
            QDomDocument strokeDoc = doc.cloneNode(true).toDocument();
            pruneVariantTree(strokeDoc.documentElement(), VariantKind::StrokeOnly, false, false);
            result.strokeSvg = strokeDoc.toByteArray(2);
        }
        if (cutoutCount > 0) {
            QDomDocument cutoutDoc = doc.cloneNode(true).toDocument();
            pruneVariantTree(cutoutDoc.documentElement(), VariantKind::CutoutAll, true, false);
            result.cutoutSvg = cutoutDoc.toByteArray(2);
        }
        stripInternalMarkers(root);
        result.cleanedSvg = doc.toByteArray(2);
        if (result.solidSvg.isEmpty() && result.strokeSvg.isEmpty()) {
            result.solidSvg = result.cleanedSvg;
        }
        result.cutoutGeometryCount = cutoutCount;
    }

    result.detectedColors = detectedColorSet.values();
    std::sort(result.detectedColors.begin(), result.detectedColors.end());
    result.valid = true;
    return result;
}

SvgSanitizeResult SvgSanitizer::sanitizeFile(const QString& filePath, const SvgFilterOptions& options) {
    SvgSanitizeResult result;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errorMessage = QStringLiteral("Failed to open SVG file: %1").arg(file.errorString());
        return result;
    }
    return sanitize(file.readAll(), options);
}

} // namespace ffgui

