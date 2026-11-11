#include "ui/GlyphGridView.hpp"

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QPainter>
#include <QPalette>
#include <QSet>
#include <QUrl>

namespace ffgui {

namespace {

bool isSupportedFontFile(const QString& localPath) {
    if (localPath.isEmpty()) {
        return false;
    }
    const QFileInfo fi(localPath);
    if (!fi.exists()) {
        return false;
    }
    static const QSet<QString> kSupportedExts = {
        QStringLiteral("ttf"),   QStringLiteral("otf"),   QStringLiteral("ttc"),
        QStringLiteral("woff"),  QStringLiteral("woff2"), QStringLiteral("sfd"),
        QStringLiteral("sfdir"), QStringLiteral("pfb"),   QStringLiteral("pfa"),
        QStringLiteral("ufo"),   QStringLiteral("svg"),   QStringLiteral("bdf"),
        QStringLiteral("pcf"),   QStringLiteral("dfont"), QStringLiteral("otb"),
        QStringLiteral("cid"),   QStringLiteral("ps"),
    };
    const QString ext = fi.suffix().toLower();
    if (fi.isDir()) {
        return ext == QStringLiteral("ufo") || ext == QStringLiteral("sfdir");
    }
    return kSupportedExts.contains(ext);
}

} // namespace

GlyphGridView::GlyphGridView(QWidget* parent) : QListView(parent) {
    setDragDropMode(QAbstractItemView::DropOnly);
    setEmptyDropEnabled(true);
}

void GlyphGridView::setEmptyDropEnabled(bool enabled) {
    m_emptyDropEnabled = enabled;
    m_dragHover = false;
    setAcceptDrops(enabled);
    if (viewport()) {
        viewport()->setAcceptDrops(enabled);
        viewport()->update();
    }
}

QString GlyphGridView::extractSupportedFontPath(const QMimeData* mimeData) const {
    if (!mimeData || !mimeData->hasUrls()) {
        return {};
    }
    for (const QUrl& url : mimeData->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString localPath = url.toLocalFile();
        if (isSupportedFontFile(localPath)) {
            return localPath;
        }
    }
    return {};
}

void GlyphGridView::dragEnterEvent(QDragEnterEvent* event) {
    if (!m_emptyDropEnabled) {
        event->ignore();
        return;
    }
    if (!extractSupportedFontPath(event->mimeData()).isEmpty()) {
        m_dragHover = true;
        viewport()->update();
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void GlyphGridView::dragMoveEvent(QDragMoveEvent* event) {
    if (!m_emptyDropEnabled) {
        event->ignore();
        return;
    }
    if (!extractSupportedFontPath(event->mimeData()).isEmpty()) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void GlyphGridView::dragLeaveEvent(QDragLeaveEvent* event) {
    if (m_dragHover) {
        m_dragHover = false;
        viewport()->update();
    }
    QListView::dragLeaveEvent(event);
}

void GlyphGridView::dropEvent(QDropEvent* event) {
    const bool wasHover = m_dragHover;
    m_dragHover = false;
    if (wasHover && viewport()) {
        viewport()->update();
    }

    if (!m_emptyDropEnabled) {
        event->ignore();
        return;
    }

    const QString path = extractSupportedFontPath(event->mimeData());
    if (path.isEmpty()) {
        event->ignore();
        return;
    }

    event->acceptProposedAction();
    emit fontFileDropped(path);
}

void GlyphGridView::paintEvent(QPaintEvent* event) {
    QListView::paintEvent(event);

    if (!m_emptyDropEnabled) {
        return;
    }

    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPalette& pal = palette();
    const QRectF box = viewport()->rect().adjusted(24, 24, -24, -24);

    QColor borderColor = m_dragHover ? pal.highlight().color() : pal.mid().color();
    borderColor.setAlpha(m_dragHover ? 220 : 120);
    QPen pen(borderColor, m_dragHover ? 2.5 : 1.5, Qt::DashLine);
    p.setPen(pen);

    if (m_dragHover) {
        QColor fill = pal.highlight().color();
        fill.setAlpha(24);
        p.setBrush(fill);
    } else {
        p.setBrush(Qt::NoBrush);
    }
    p.drawRoundedRect(box, 12.0, 12.0);

    QColor textColor = pal.text().color();
    textColor.setAlpha(m_dragHover ? 230 : 140);
    p.setPen(textColor);

    QFont titleFont = font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.15);
    titleFont.setBold(true);
    p.setFont(titleFont);

    const QString title =
        m_dragHover ? tr("Release to open font") : tr("Drop a font file here to open");
    const QString subtitle =
        tr("Supported: .ttf, .otf, .woff, .woff2, .sfd, .ufo, .pfb, .svg\n"
           "or use File → Open Font… (Ctrl+O)");

    const QRectF titleRect(box.left() + 16, box.center().y() - 36, box.width() - 32, 30);
    const QRectF subRect(box.left() + 16, box.center().y(), box.width() - 32, 48);

    p.drawText(titleRect, Qt::AlignCenter, title);

    QFont subFont = font();
    p.setFont(subFont);
    QColor subColor = pal.text().color();
    subColor.setAlpha(115);
    p.setPen(subColor);
    p.drawText(subRect, Qt::AlignHCenter | Qt::AlignTop, subtitle);
}

} // namespace ffgui

