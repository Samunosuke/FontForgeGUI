#pragma once

#include <QSize>
#include <QStyledItemDelegate>

namespace ffgui {

class GlyphGridDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit GlyphGridDelegate(QObject* parent = nullptr);

    void setCellSize(int size);
    int cellSize() const { return m_cellSize; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    int m_cellSize = 104;
};

} // namespace ffgui

