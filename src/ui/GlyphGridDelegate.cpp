#include "ui/GlyphGridDelegate.hpp"

#include "model/GlyphListModel.hpp"

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <algorithm>

namespace ffgui {

GlyphGridDelegate::GlyphGridDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void GlyphGridDelegate::setCellSize(int size) {
    m_cellSize = std::clamp(size, 64, 200);
}

QSize GlyphGridDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                                  const QModelIndex& /*index*/) const {
    return QSize(m_cellSize, m_cellSize + 24);
}

void GlyphGridDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QPalette& pal = option.palette;
    const bool selected = (option.state & QStyle::State_Selected) != 0;
    const bool hovered = (option.state & QStyle::State_MouseOver) != 0;

    const QRect cardRect = option.rect.adjusted(3, 3, -3, -3);

    QColor bgColor = pal.base().color();
    QColor borderColor = pal.mid().color();
    borderColor.setAlpha(90);

    if (selected) {
        bgColor = pal.highlight().color();
        bgColor.setAlpha(36);
        borderColor = pal.highlight().color();
    } else if (hovered) {
        bgColor = pal.alternateBase().color();
        borderColor = pal.highlight().color();
        borderColor.setAlpha(140);
    }

    painter->setPen(QPen(borderColor, selected ? 1.8 : 1.0));
    painter->setBrush(bgColor);
    painter->drawRoundedRect(cardRect, 6.0, 6.0);

    // Header: Unicode (left) and GID (right)
    const int unicode = index.data(GlyphListModel::UnicodeRole).toInt();
    const int gid = index.data(GlyphListModel::GidRole).toInt();
    const QString name = index.data(GlyphListModel::NameRole).toString();
    const int advanceWidth = index.data(GlyphListModel::WidthRole).toInt();
    const int ascent = index.data(GlyphListModel::AscentRole).toInt();
    const int descent = index.data(GlyphListModel::DescentRole).toInt();
    const int emSize = std::max(1, index.data(GlyphListModel::EmSizeRole).toInt());
    const bool hasOutline = index.data(GlyphListModel::HasOutlineRole).toBool();
    const QPainterPath outline = index.data(GlyphListModel::OutlineRole).value<QPainterPath>();

    QFont metaFont = option.font;
    metaFont.setPointSizeF(std::max(7.5, metaFont.pointSizeF() * 0.78));
    painter->setFont(metaFont);

    QColor mutedText = pal.text().color();
    mutedText.setAlpha(155);
    painter->setPen(mutedText);

    const QRect headerRect = cardRect.adjusted(6, 4, -6, 0);
    const QString uniText =
        unicode >= 0 ? QStringLiteral("U+%1").arg(unicode, 4, 16, QLatin1Char('0')).toUpper()
                     : QStringLiteral("—");
    painter->drawText(headerRect, Qt::AlignLeft | Qt::AlignTop, uniText);
    painter->drawText(headerRect, Qt::AlignRight | Qt::AlignTop,
                      QStringLiteral("#%1").arg(gid));

    // Footer: Glyph name
    const int headerH = 18;
    const int footerH = 20;
    const QRect footerRect = cardRect.adjusted(5, 0, -5, -4);
    painter->setPen(pal.text().color());
    QFont nameFont = option.font;
    nameFont.setPointSizeF(std::max(8.0, nameFont.pointSizeF() * 0.85));
    painter->setFont(nameFont);
    const QFontMetrics fm(nameFont);
    const QString elidedName = fm.elidedText(name, Qt::ElideRight, footerRect.width());
    painter->drawText(footerRect, Qt::AlignHCenter | Qt::AlignBottom, elidedName);

    // Glyph preview area
    const QRectF previewRect(cardRect.left() + 8, cardRect.top() + headerH,
                             cardRect.width() - 16, cardRect.height() - headerH - footerH);

    // Baseline position based on ascent / emSize
    const double scale = std::min(previewRect.width() / static_cast<double>(emSize),
                                  previewRect.height() / static_cast<double>(emSize)) *
                         0.85;
    const double baselineY =
        previewRect.top() + (previewRect.height() - emSize * scale) * 0.5 + ascent * scale;

    // Subtle baseline guide
    QColor guideColor = pal.mid().color();
    guideColor.setAlpha(70);
    painter->setPen(QPen(guideColor, 1.0, Qt::DotLine));
    painter->drawLine(QPointF(previewRect.left(), baselineY),
                      QPointF(previewRect.right(), baselineY));

    if (hasOutline) {
        const QRectF bounds = outline.boundingRect();
        double refWidth = advanceWidth > 0 ? static_cast<double>(advanceWidth) : bounds.width();
        double refCenterX = advanceWidth > 0 ? refWidth * 0.5 : bounds.center().x();

        // If outline overflows far beyond advance width, fit within previewRect cleanly
        double drawScale = scale;
        const double scaledW = bounds.width() * drawScale;
        const double scaledH = bounds.height() * drawScale;
        if (scaledW > previewRect.width() * 0.95 || scaledH > previewRect.height() * 0.95) {
            const double fitX =
                bounds.width() > 0 ? (previewRect.width() * 0.9) / bounds.width() : drawScale;
            const double fitY =
                bounds.height() > 0 ? (previewRect.height() * 0.9) / bounds.height() : drawScale;
            drawScale = std::min(drawScale, std::min(fitX, fitY));
            refCenterX = bounds.center().x();
        }

        const double targetCenterX = previewRect.center().x();
        const double targetBaselineY =
            previewRect.top() + (previewRect.height() - emSize * drawScale) * 0.5 +
            ascent * drawScale;

        painter->save();
        painter->setClipRect(previewRect);
        painter->translate(targetCenterX - refCenterX * drawScale, targetBaselineY);
        painter->scale(drawScale, -drawScale);
        painter->setPen(Qt::NoPen);
        painter->setBrush(pal.text().color());
        painter->drawPath(outline);
        painter->restore();
    } else {
        QColor emptyColor = pal.text().color();
        emptyColor.setAlpha(60);
        painter->setPen(emptyColor);
        painter->drawText(previewRect, Qt::AlignCenter,
                          advanceWidth > 0 ? QStringLiteral("space") : QStringLiteral("empty"));
    }

    Q_UNUSED(descent);
    painter->restore();
}

} // namespace ffgui

