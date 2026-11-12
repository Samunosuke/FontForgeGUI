#pragma once

#include <QListView>
#include <QString>

namespace ffgui {

/**
 * Virtualized glyph grid view that accepts font file Drag & Drop
 * when no font is currently loaded.
 */
class GlyphGridView : public QListView {
    Q_OBJECT

public:
    explicit GlyphGridView(QWidget* parent = nullptr);

    void setEmptyDropEnabled(bool enabled);
    bool isEmptyDropEnabled() const { return m_emptyDropEnabled; }

signals:
    void fontFileDropped(const QString& path);
    void zoomStepsRequested(int steps);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QString extractSupportedFontPath(const QMimeData* mimeData) const;

    bool m_emptyDropEnabled = true;
    bool m_dragHover = false;
};

} // namespace ffgui

