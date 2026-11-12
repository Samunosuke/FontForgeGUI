#include "ui/ImportSvgDialog.hpp"
#include "model/FontSettingsUtils.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QSvgRenderer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>

namespace ffgui {

namespace {

constexpr auto kSettingsOrg = "FontForgeGUI";
constexpr auto kSettingsApp = "FontForgeGUI";
constexpr auto kLastSvgDirKey = "importSvg/lastDirectory";

bool isSupportedImportFile(const QFileInfo& fi) {
    if (!fi.exists() || !fi.isFile()) {
        return false;
    }
    const QString ext = fi.suffix().toLower();
    return ext == QStringLiteral("svg") || ext == QStringLiteral("png");
}

QString extractDroppedImportPath(const QMimeData* mimeData) {
    if (!mimeData || !mimeData->hasUrls()) {
        return {};
    }
    for (const QUrl& url : mimeData->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString localPath = url.toLocalFile();
        const QFileInfo fi(localPath);
        if (isSupportedImportFile(fi)) {
            return localPath;
        }
    }
    return {};
}

QString loadLastSvgDirectory() {
    QSettings settings(QString::fromLatin1(kSettingsOrg), QString::fromLatin1(kSettingsApp));
    const QString dir = settings.value(QString::fromLatin1(kLastSvgDirKey)).toString();
    if (!dir.isEmpty() && QDir(dir).exists()) {
        return dir;
    }
    return {};
}

void saveLastSvgDirectory(const QString& filePath) {
    const QString dir = QFileInfo(filePath).absolutePath();
    if (dir.isEmpty()) {
        return;
    }
    QSettings settings(QString::fromLatin1(kSettingsOrg), QString::fromLatin1(kSettingsApp));
    settings.setValue(QString::fromLatin1(kLastSvgDirKey), dir);
}

void drawCheckerboard(QPainter& p, const QRect& canvasRect) {
    constexpr int kTile = 10;
    for (int y = canvasRect.top(); y <= canvasRect.bottom(); y += kTile) {
        for (int x = canvasRect.left(); x <= canvasRect.right(); x += kTile) {
            const bool dark =
                (((x - canvasRect.left()) / kTile) + ((y - canvasRect.top()) / kTile)) % 2 != 0;
            p.fillRect(QRect(x, y, kTile, kTile),
                       dark ? QColor(232, 232, 236) : QColor(246, 246, 249));
        }
    }
}

} // namespace

class SvgPreviewWidget : public QWidget {
public:
    explicit SvgPreviewWidget(const QString& title, QWidget* parent = nullptr)
        : QWidget(parent), m_title(title) {
        setMinimumSize(220, 200);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setTitle(const QString& title) {
        m_title = title;
        update();
    }

    void setDropHandler(std::function<void(const QString&)> handler,
                        const QString& emptyPlaceholder = QString()) {
        m_dropHandler = std::move(handler);
        m_emptyPlaceholder = emptyPlaceholder;
        setAcceptDrops(static_cast<bool>(m_dropHandler));
        update();
    }

    void setSvgData(const QByteArray& svgBytes, const QString& placeholderText = QString()) {
        m_rasterImage = QImage();
        m_placeholder = placeholderText;
        m_valid = !svgBytes.isEmpty() && m_renderer.load(svgBytes);
        if (!m_valid && m_placeholder.isEmpty() && !svgBytes.isEmpty()) {
            m_placeholder = tr("Cannot render SVG");
        }
        update();
    }

    void setRasterImage(const QImage& image, const QString& placeholderText = QString()) {
        m_rasterImage = image;
        m_placeholder = placeholderText;
        m_valid = !m_rasterImage.isNull();
        update();
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        if (!m_dropHandler || extractDroppedImportPath(event->mimeData()).isEmpty()) {
            event->ignore();
            return;
        }
        m_dragHover = true;
        update();
        event->acceptProposedAction();
    }

    void dragMoveEvent(QDragMoveEvent* event) override {
        if (!m_dropHandler || extractDroppedImportPath(event->mimeData()).isEmpty()) {
            event->ignore();
            return;
        }
        event->acceptProposedAction();
    }

    void dragLeaveEvent(QDragLeaveEvent* event) override {
        m_dragHover = false;
        update();
        event->accept();
    }

    void dropEvent(QDropEvent* event) override {
        m_dragHover = false;
        update();
        if (!m_dropHandler) {
            event->ignore();
            return;
        }
        const QString droppedPath = extractDroppedImportPath(event->mimeData());
        if (droppedPath.isEmpty()) {
            event->ignore();
            return;
        }
        event->acceptProposedAction();
        m_dropHandler(droppedPath);
    }

    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QRect box = rect().adjusted(2, 2, -2, -2);
        QColor borderColor = palette().mid().color();
        Qt::PenStyle borderStyle = Qt::SolidLine;
        qreal borderWidth = 1.0;
        if (m_dragHover) {
            borderColor = palette().highlight().color();
            borderStyle = Qt::DashLine;
            borderWidth = 2.0;
        }
        p.setPen(QPen(borderColor, borderWidth, borderStyle));
        p.setBrush(palette().base());
        p.drawRoundedRect(box, 6, 6);

        const QRect headerRect(box.left() + 8, box.top() + 4, box.width() - 16, 18);
        p.setPen(palette().placeholderText().color());
        QFont f = font();
        f.setPointSizeF(std::max(8.0, f.pointSizeF() - 1.0));
        f.setBold(true);
        p.setFont(f);
        p.drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter, m_title);

        const QRect canvasRect = box.adjusted(10, 26, -10, -10);
        if (!m_valid) {
            p.setPen(m_dragHover ? palette().highlight().color()
                                 : palette().placeholderText().color());
            const QString fallbackText =
                !m_emptyPlaceholder.isEmpty() ? m_emptyPlaceholder : tr("No file loaded");
            p.drawText(canvasRect, Qt::AlignCenter | Qt::TextWordWrap,
                       m_placeholder.isEmpty() ? fallbackText : m_placeholder);
            return;
        }

        p.save();
        p.setClipRect(canvasRect);
        drawCheckerboard(p, canvasRect);

        if (!m_rasterImage.isNull()) {
            const QSizeF imgSize(m_rasterImage.width(), m_rasterImage.height());
            const QSizeF scaled = imgSize.scaled(canvasRect.size() * 0.9, Qt::KeepAspectRatio);
            const QRectF target(canvasRect.center().x() - scaled.width() * 0.5,
                                canvasRect.center().y() - scaled.height() * 0.5,
                                scaled.width(), scaled.height());
            p.drawImage(target, m_rasterImage);
        } else {
            QSizeF defSize = m_renderer.defaultSize();
            if (!defSize.isValid() || defSize.width() <= 0.0 || defSize.height() <= 0.0) {
                defSize = QSizeF(100.0, 100.0);
            }
            const QSizeF scaled = defSize.scaled(canvasRect.size() * 0.9, Qt::KeepAspectRatio);
            const QRectF target(canvasRect.center().x() - scaled.width() * 0.5,
                                canvasRect.center().y() - scaled.height() * 0.5,
                                scaled.width(), scaled.height());
            m_renderer.render(&p, target);
        }
        p.restore();
    }

private:
    QString m_title;
    QString m_placeholder;
    QString m_emptyPlaceholder;
    QSvgRenderer m_renderer;
    QImage m_rasterImage;
    std::function<void(const QString&)> m_dropHandler;
    bool m_valid = false;
    bool m_dragHover = false;
};

/**
 * Interactive Glyph Metric & Placement Preview Widget.
 * Renders the filtered SVG / traced PNG inside the target glyph's Ascent/Baseline/Descent/Width
 * coordinate space and lets the user drag to adjust (offsetX, offsetY) and wheel-scroll to scale.
 */
class GlyphPlacementPreviewWidget : public QWidget {
public:
    using OffsetChangedCallback = std::function<void(double newOffsetX, double newOffsetY)>;
    using ScaleChangedCallback = std::function<void(double newScalePercent)>;
    using ResetRequestedCallback = std::function<void()>;

    explicit GlyphPlacementPreviewWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(250, 210);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMouseTracking(true);
        setToolTip(
            tr("Drag with left mouse button to reposition the icon.\n"
               "Scroll mouse wheel to scale.\n"
               "Double-click to reset position and scale."));
    }

    void setCallbacks(OffsetChangedCallback onOffset, ScaleChangedCallback onScale,
                      ResetRequestedCallback onReset) {
        m_onOffsetChanged = std::move(onOffset);
        m_onScaleChanged = std::move(onScale);
        m_onResetRequested = std::move(onReset);
    }

    void setGlyphContext(int ascent, int descent, double advanceWidth,
                         const QPainterPath& existingOutline, bool showExistingOutline) {
        m_ascent = ascent > 0 ? ascent : 800;
        m_descent = descent >= 0 ? descent : 200;
        m_advanceWidth = advanceWidth > 0.0 ? advanceWidth : static_cast<double>(m_ascent + m_descent);
        m_existingOutline = existingOutline;
        m_showExistingOutline = showExistingOutline;
        update();
    }

    void setContent(const QByteArray& svgBytes, const QRectF& viewBox, const QRectF& inkBounds,
                    const SvgImportOptions& opts, const QString& placeholderText = QString()) {
        m_placeholder = placeholderText;
        m_valid = !svgBytes.isEmpty() && m_renderer.load(svgBytes);
        if (!m_valid && m_placeholder.isEmpty() && !svgBytes.isEmpty()) {
            m_placeholder = tr("Cannot render preview");
        }
        m_viewBox = viewBox;
        if (!m_viewBox.isValid() || m_viewBox.width() <= 0.0 || m_viewBox.height() <= 0.0) {
            m_viewBox = m_renderer.viewBoxF();
        }
        if (!m_viewBox.isValid() || m_viewBox.width() <= 0.0 || m_viewBox.height() <= 0.0) {
            const QSize s = m_renderer.defaultSize();
            m_viewBox = QRectF(0.0, 0.0, s.width() > 0 ? s.width() : 100.0,
                               s.height() > 0 ? s.height() : 100.0);
        }
        m_inkBounds = (inkBounds.isValid() && inkBounds.width() > 0.0 && inkBounds.height() > 0.0)
                          ? inkBounds
                          : m_viewBox;
        m_opts = opts;
        update();
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (m_valid && event->button() == Qt::LeftButton) {
            m_dragging = true;
            m_dragStartPos = event->pos();
            m_dragStartOffsetX = m_opts.offsetX;
            m_dragStartOffsetY = m_opts.offsetY;
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (m_dragging && m_pxPerUnit > 1e-6) {
            const QPoint delta = event->pos() - m_dragStartPos;
            const double dFontX = static_cast<double>(delta.x()) / m_pxPerUnit;
            const double dFontY = static_cast<double>(-delta.y()) / m_pxPerUnit;
            const double newX = std::round(m_dragStartOffsetX + dFontX);
            const double newY = std::round(m_dragStartOffsetY + dFontY);
            if (m_onOffsetChanged) {
                m_onOffsetChanged(newX, newY);
            }
            event->accept();
            return;
        }
        if (m_valid) {
            setCursor(Qt::OpenHandCursor);
        } else {
            unsetCursor();
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (m_dragging && event->button() == Qt::LeftButton) {
            m_dragging = false;
            setCursor(m_valid ? Qt::OpenHandCursor : Qt::ArrowCursor);
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (m_valid && event->button() == Qt::LeftButton && m_onResetRequested) {
            m_onResetRequested();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override {
        if (!m_valid || !m_onScaleChanged) {
            QWidget::wheelEvent(event);
            return;
        }
        const int deltaY = event->angleDelta().y();
        if (deltaY != 0) {
            const double step = (deltaY > 0) ? 5.0 : -5.0;
            const double curPct = m_opts.scaleFactor * 100.0;
            const double nextPct = std::clamp(curPct + step, 10.0, 500.0);
            m_onScaleChanged(nextPct);
        }
        event->accept();
    }

    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRect box = rect().adjusted(2, 2, -2, -2);
        p.setPen(QPen(palette().mid().color(), 1.0));
        p.setBrush(palette().base());
        p.drawRoundedRect(box, 6, 6);

        const QRect headerRect(box.left() + 8, box.top() + 4, box.width() - 16, 18);
        p.setPen(palette().placeholderText().color());
        QFont f = font();
        f.setPointSizeF(std::max(8.0, f.pointSizeF() - 1.0));
        f.setBold(true);
        p.setFont(f);
        p.drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter,
                   tr("Glyph Placement Preview (Drag to move · Wheel to scale)"));

        const QRect canvasRect = box.adjusted(10, 26, -10, -10);
        if (!m_valid) {
            p.setPen(palette().placeholderText().color());
            p.drawText(canvasRect, Qt::AlignCenter | Qt::TextWordWrap,
                       m_placeholder.isEmpty() ? tr("No vector geometry to preview") : m_placeholder);
            return;
        }

        p.save();
        p.setClipRect(canvasRect);
        drawCheckerboard(p, canvasRect);

        const double emSize = static_cast<double>(std::max(1, m_ascent + m_descent));
        const double totalFontW = std::max(m_advanceWidth, emSize) * 1.30;
        const double totalFontH = emSize * 1.32;

        m_pxPerUnit = std::min(static_cast<double>(canvasRect.width()) / totalFontW,
                               static_cast<double>(canvasRect.height()) / totalFontH);
        const double glyphPixelW = m_advanceWidth * m_pxPerUnit;
        const double emPixelH = emSize * m_pxPerUnit;

        const double originX = canvasRect.center().x() - glyphPixelW * 0.5;
        const double topEmY = canvasRect.center().y() - emPixelH * 0.5;
        const double baselineY = topEmY + m_ascent * m_pxPerUnit;
        const double descentY = baselineY + m_descent * m_pxPerUnit;
        const double midAscentY = baselineY - (m_ascent * 0.5) * m_pxPerUnit;
        const double rightAdvanceX = originX + glyphPixelW;

        auto toScreenX = [&](double fx) { return originX + fx * m_pxPerUnit; };
        auto toScreenY = [&](double fy) { return baselineY - fy * m_pxPerUnit; };

        // Highlight active glyph advance box (-descent .. +ascent, 0 .. width)
        const QRectF emBoxRect(originX, topEmY, glyphPixelW, emPixelH);
        p.fillRect(emBoxRect, QColor(255, 255, 255, 165));
        p.setPen(QPen(QColor(160, 170, 190, 180), 1.0));
        p.drawRect(emBoxRect);

        // Draw metric guide lines (Ascent, Midline, Baseline y=0, Descent, x=0, x=width)
        p.setPen(QPen(QColor(90, 140, 210, 150), 1.0, Qt::DashLine));
        p.drawLine(QPointF(canvasRect.left(), topEmY), QPointF(canvasRect.right(), topEmY));
        p.drawLine(QPointF(canvasRect.left(), descentY), QPointF(canvasRect.right(), descentY));

        p.setPen(QPen(QColor(140, 160, 190, 120), 1.0, Qt::DotLine));
        p.drawLine(QPointF(canvasRect.left(), midAscentY), QPointF(canvasRect.right(), midAscentY));

        // Baseline (y = 0) — strong reference line
        p.setPen(QPen(QColor(215, 70, 65, 210), 1.5, Qt::SolidLine));
        p.drawLine(QPointF(canvasRect.left(), baselineY), QPointF(canvasRect.right(), baselineY));

        // Left origin (x = 0) & Advance width (x = width)
        p.setPen(QPen(QColor(100, 115, 140, 170), 1.0, Qt::DashLine));
        p.drawLine(QPointF(originX, canvasRect.top()), QPointF(originX, canvasRect.bottom()));
        p.drawLine(QPointF(rightAdvanceX, canvasRect.top()),
                   QPointF(rightAdvanceX, canvasRect.bottom()));

        // Optional ghost outline of existing glyph contours
        if (m_showExistingOutline && !m_existingOutline.isEmpty()) {
            p.save();
            p.translate(originX, baselineY);
            p.scale(m_pxPerUnit, -m_pxPerUnit);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(120, 140, 180, 65));
            p.drawPath(m_existingOutline);
            p.restore();
        }

        // Compute exact FontForge initial SVG interpretation coordinates and placement transform
        const double ffInitScale =
            (m_opts.scaleToEm && m_viewBox.height() > 0.0) ? (emSize / m_viewBox.height()) : 1.0;

        const double rawMinX = (m_inkBounds.left() - m_viewBox.left()) * ffInitScale;
        const double rawMaxX = (m_inkBounds.right() - m_viewBox.left()) * ffInitScale;
        const double rawMaxY =
            static_cast<double>(m_ascent) - (m_inkBounds.top() - m_viewBox.top()) * ffInitScale;
        const double rawMinY =
            static_cast<double>(m_ascent) - (m_inkBounds.bottom() - m_viewBox.top()) * ffInitScale;

        const IconPlacementTransform pt =
            computeIconPlacement(rawMinX, rawMaxX, rawMinY, rawMaxY, m_advanceWidth, m_ascent,
                                 m_descent, m_opts);

        // Map full SVG viewBox to font coordinates and then to screen coordinates
        const double vbFontLeft = pt.tx;
        const double vbFontTopY = static_cast<double>(m_ascent) * pt.scale + pt.ty;
        const double vbFontW = m_viewBox.width() * ffInitScale * pt.scale;
        const double vbFontH = m_viewBox.height() * ffInitScale * pt.scale;

        const QRectF svgScreenRect(toScreenX(vbFontLeft), toScreenY(vbFontTopY),
                                   vbFontW * m_pxPerUnit, vbFontH * m_pxPerUnit);
        m_renderer.render(&p, svgScreenRect);

        // Draw tight placed ink bounding box frame
        const double placedMinX = rawMinX * pt.scale + pt.tx;
        const double placedMaxX = rawMaxX * pt.scale + pt.tx;
        const double placedMinY = rawMinY * pt.scale + pt.ty;
        const double placedMaxY = rawMaxY * pt.scale + pt.ty;

        const QRectF inkScreenRect(
            QPointF(toScreenX(placedMinX), toScreenY(placedMaxY)),
            QPointF(toScreenX(placedMaxX), toScreenY(placedMinY)));
        p.setPen(QPen(QColor(35, 135, 230, 190), 1.0, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(inkScreenRect);

        // Draw metric labels
        QFont smallFont = font();
        smallFont.setPointSizeF(std::max(7.0, smallFont.pointSizeF() - 2.0));
        p.setFont(smallFont);
        p.setPen(QColor(90, 110, 145, 210));
        p.drawText(QPointF(canvasRect.left() + 4, topEmY - 3),
                   QStringLiteral("Ascent (%1)").arg(m_ascent));
        p.drawText(QPointF(canvasRect.left() + 4, descentY + 11),
                   QStringLiteral("Descent (-%1)").arg(m_descent));
        p.setPen(QColor(195, 55, 50, 230));
        p.drawText(QPointF(canvasRect.left() + 4, baselineY - 3), QStringLiteral("Baseline (0)"));

        // Bottom info badge showing aspect ratio & active placement rule
        const QString ratioModeStr =
            tr("W/H: %1 (%2) · Bounds: [%3..%4, %5..%6]")
                .arg(pt.aspectRatio, 0, 'f', 2)
                .arg(pt.isWideAspect ? tr("wide → centered") : tr("upright → baseline"))
                .arg( std::lround(placedMinX))
                .arg( std::lround(placedMaxX))
                .arg( std::lround(placedMinY))
                .arg( std::lround(placedMaxY));

        const QRect badgeRect(canvasRect.left() + 4, canvasRect.bottom() - 20,
                              canvasRect.width() - 8, 18);
        p.fillRect(badgeRect, QColor(25, 30, 40, 165));
        p.setPen(QColor(240, 244, 250));
        p.drawText(badgeRect.adjusted(6, 0, -6, 0), Qt::AlignLeft | Qt::AlignVCenter, ratioModeStr);

        p.restore();
    }

private:
    QSvgRenderer m_renderer;
    QRectF m_viewBox;
    QRectF m_inkBounds;
    SvgImportOptions m_opts;
    QPainterPath m_existingOutline;
    QString m_placeholder;
    int m_ascent = 800;
    int m_descent = 200;
    double m_advanceWidth = 1000.0;
    double m_pxPerUnit = 0.15;
    bool m_showExistingOutline = false;
    bool m_valid = false;

    bool m_dragging = false;
    QPoint m_dragStartPos;
    double m_dragStartOffsetX = 0.0;
    double m_dragStartOffsetY = 0.0;

    OffsetChangedCallback m_onOffsetChanged;
    ScaleChangedCallback m_onScaleChanged;
    ResetRequestedCallback m_onResetRequested;
};

namespace {

QIcon makeColorSwatchIcon(const QString& hexColor) {
    QPixmap pm(12, 12);
    pm.fill(QColor(hexColor));
    QPainter p(&pm);
    p.setPen(QColor(80, 80, 80));
    p.drawRect(0, 0, 11, 11);
    return QIcon(pm);
}

} // namespace

ImportSvgDialog::ImportSvgDialog(const GlyphSummary& targetGlyph, const FontSummary& fontSummary,
                                 const QString& initialFilePath, QWidget* parent)
    : QDialog(parent), m_targetGlyph(targetGlyph), m_fontSummary(fontSummary) {
    const QString uniLabel =
        (targetGlyph.unicode >= 0)
            ? QStringLiteral("U+%1").arg(targetGlyph.unicode, 4, 16, QLatin1Char('0')).toUpper()
            : tr("Unencoded");
    setWindowTitle(tr("Import SVG / PNG into Glyph %1 (#%2, %3)")
                       .arg(targetGlyph.name)
                       .arg(targetGlyph.gid)
                       .arg(uniLabel));
    setAcceptDrops(true);
    resize(860, 720);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(8);

    // File chooser row
    auto* fileRow = new QHBoxLayout();
    fileRow->addWidget(new QLabel(tr("Source File:"), this));
    m_pathEdit = new QLineEdit(this);
    const QString lastDir = loadLastSvgDirectory();
    m_pathEdit->setPlaceholderText(
        lastDir.isEmpty()
            ? tr("Choose or drop an .svg vector or .png icon file…")
            : tr("Drop an .svg / .png file or click Browse… (last: %1)").arg(lastDir));
    m_pathEdit->setReadOnly(true);
    auto* browseBtn = new QPushButton(tr("Browse…"), this);
    fileRow->addWidget(m_pathEdit, 1);
    fileRow->addWidget(browseBtn);
    rootLayout->addLayout(fileRow);

    // Side-by-side preview: Left = Original SVG/PNG, Right = Interactive Glyph Metric Placement
    auto* previewRow = new QHBoxLayout();
    previewRow->setSpacing(10);
    m_originalPreview = new SvgPreviewWidget(tr("Original Input (SVG / PNG)"), this);
    m_originalPreview->setDropHandler(
        [this](const QString& droppedPath) { loadInputFile(droppedPath); },
        tr("No file loaded\nDrop an .svg or .png file here or click Browse…"));
    m_placementPreview = new GlyphPlacementPreviewWidget(this);
    previewRow->addWidget(m_originalPreview, 1);
    previewRow->addWidget(m_placementPreview, 1);
    rootLayout->addLayout(previewRow, 1);

    m_statsLabel = new QLabel(
        tr("Select an .svg or .png file to inspect geometry, filter/trace options, and placement."),
        this);
    m_statsLabel->setWordWrap(true);
    rootLayout->addWidget(m_statsLabel);

    // Positioning & Manual Placement bar
    auto* placementGroup = new QGroupBox(tr("Icon Positioning & Manual Adjustment"), this);
    auto* placementLayout = new QHBoxLayout(placementGroup);
    placementLayout->setContentsMargins(8, 6, 8, 6);
    placementLayout->setSpacing(8);

    placementLayout->addWidget(new QLabel(tr("Mode:"), placementGroup));
    m_placementModeCombo = new QComboBox(placementGroup);
    m_placementModeCombo->addItem(
        tr("Auto (W/H ≤ 1.25 on Baseline, W/H > 1.25 Centered)"),
        static_cast<int>(GlyphPlacementMode::AutoAspectRatio));
    m_placementModeCombo->addItem(tr("Baseline (y = 0) & Horizontally Centered"),
                                  static_cast<int>(GlyphPlacementMode::BaselineCentered));
    m_placementModeCombo->addItem(tr("Centered between Baseline & Ascent"),
                                  static_cast<int>(GlyphPlacementMode::CenterInAscent));
    m_placementModeCombo->addItem(tr("Centered in Full EM Box (-Descent .. +Ascent)"),
                                  static_cast<int>(GlyphPlacementMode::CenterInEm));
    m_placementModeCombo->addItem(tr("Original SVG Coordinates"),
                                  static_cast<int>(GlyphPlacementMode::OriginalSvgCoords));
    placementLayout->addWidget(m_placementModeCombo, 1);

    placementLayout->addWidget(new QLabel(tr("Scale:"), placementGroup));
    m_scaleSpin = new QDoubleSpinBox(placementGroup);
    m_scaleSpin->setRange(10.0, 500.0);
    m_scaleSpin->setSingleStep(5.0);
    m_scaleSpin->setDecimals(1);
    m_scaleSpin->setValue(100.0);
    m_scaleSpin->setSuffix(tr(" %"));
    placementLayout->addWidget(m_scaleSpin);

    placementLayout->addWidget(new QLabel(tr("Offset X:"), placementGroup));
    m_offsetXSpin = new QDoubleSpinBox(placementGroup);
    m_offsetXSpin->setRange(-4000.0, 4000.0);
    m_offsetXSpin->setSingleStep(10.0);
    m_offsetXSpin->setDecimals(0);
    m_offsetXSpin->setValue(0.0);
    placementLayout->addWidget(m_offsetXSpin);

    placementLayout->addWidget(new QLabel(tr("Offset Y:"), placementGroup));
    m_offsetYSpin = new QDoubleSpinBox(placementGroup);
    m_offsetYSpin->setRange(-4000.0, 4000.0);
    m_offsetYSpin->setSingleStep(10.0);
    m_offsetYSpin->setDecimals(0);
    m_offsetYSpin->setValue(0.0);
    placementLayout->addWidget(m_offsetYSpin);

    m_resetPlacementBtn = new QPushButton(tr("Reset"), placementGroup);
    m_resetPlacementBtn->setToolTip(tr("Reset scale to 100% and X/Y offsets to 0"));
    placementLayout->addWidget(m_resetPlacementBtn);
    rootLayout->addWidget(placementGroup);

    // SVG-only Color filter bar
    m_colorGroup =
        new QGroupBox(tr("Detected SVG Colors Filter (uncheck colors to exclude)"), this);
    auto* colorOuter = new QVBoxLayout(m_colorGroup);
    colorOuter->setContentsMargins(8, 6, 8, 6);
    m_colorContainer = new QWidget(m_colorGroup);
    m_colorLayout = new QHBoxLayout(m_colorContainer);
    m_colorLayout->setContentsMargins(0, 0, 0, 0);
    m_colorLayout->setSpacing(10);
    m_noColorsLabel = new QLabel(tr("No color attributes detected yet"), m_colorContainer);
    m_colorLayout->addWidget(m_noColorsLabel);
    m_colorLayout->addStretch(1);
    colorOuter->addWidget(m_colorContainer);
    rootLayout->addWidget(m_colorGroup);

    // Filter / Tracer & Import options side by side
    auto* optionsRow = new QHBoxLayout();
    optionsRow->setSpacing(10);

    // 1) SVG Sanitizer Filter group (visible when .svg is loaded)
    m_svgFilterGroup = new QGroupBox(tr("SVG Sanitizer Filter"), this);
    auto* filterLayout = new QVBoxLayout(m_svgFilterGroup);
    m_stripMetadataCheck = new QCheckBox(
        tr("Strip editor metadata (Inkscape / Sodipodi / comments / styles)"), m_svgFilterGroup);
    m_removeHiddenCheck = new QCheckBox(
        tr("Remove hidden & empty elements (display:none / opacity:0)"), m_svgFilterGroup);
    m_removeBgRectsCheck =
        new QCheckBox(tr("Remove background / white rectangles"), m_svgFilterGroup);
    m_stripNoFillCheck =
        new QCheckBox(tr("Remove unfilled helper shapes (fill:none)"), m_svgFilterGroup);
    m_keepStrokesCheck = new QCheckBox(
        tr("Keep strokes as outline geometry (expand on import)"), m_svgFilterGroup);
    m_normalizeBlackCheck = new QCheckBox(
        tr("Flatten kept geometry colors to solid black (#000000)"), m_svgFilterGroup);
    m_whiteCutoutCheck = new QCheckBox(
        tr("Treat white foreground shapes (#ffffff) as cutouts / holes"), m_svgFilterGroup);

    m_stripMetadataCheck->setChecked(true);
    m_removeHiddenCheck->setChecked(true);
    m_removeBgRectsCheck->setChecked(true);
    m_stripNoFillCheck->setChecked(true);
    m_keepStrokesCheck->setChecked(false);
    m_normalizeBlackCheck->setChecked(true);
    m_whiteCutoutCheck->setChecked(true);

    auto* tinyRow = new QHBoxLayout();
    m_stripTinyCheck =
        new QCheckBox(tr("Drop tiny noise geometry (< threshold):"), m_svgFilterGroup);
    m_stripTinyCheck->setChecked(true);
    m_minExtentSpin = new QDoubleSpinBox(m_svgFilterGroup);
    m_minExtentSpin->setRange(0.05, 500.0);
    m_minExtentSpin->setSingleStep(0.5);
    m_minExtentSpin->setDecimals(2);
    m_minExtentSpin->setValue(1.0);
    m_minExtentSpin->setSuffix(tr(" px"));
    tinyRow->addWidget(m_stripTinyCheck);
    tinyRow->addWidget(m_minExtentSpin);
    tinyRow->addStretch(1);

    filterLayout->addWidget(m_stripMetadataCheck);
    filterLayout->addWidget(m_removeHiddenCheck);
    filterLayout->addWidget(m_removeBgRectsCheck);
    filterLayout->addWidget(m_stripNoFillCheck);
    filterLayout->addWidget(m_keepStrokesCheck);
    filterLayout->addWidget(m_normalizeBlackCheck);
    filterLayout->addWidget(m_whiteCutoutCheck);
    filterLayout->addLayout(tinyRow);
    filterLayout->addStretch(1);

    // 2) PNG Contour Tracer group (visible when .png is loaded; hidden in SVG mode)
    m_pngTraceGroup = new QGroupBox(tr("PNG Contour Tracer Options"), this);
    auto* pngLayout = new QVBoxLayout(m_pngTraceGroup);

    auto* bModeRow = new QHBoxLayout();
    bModeRow->addWidget(new QLabel(tr("Binarization Mode:"), m_pngTraceGroup));
    m_pngBinarizationCombo = new QComboBox(m_pngTraceGroup);
    m_pngBinarizationCombo->addItem(tr("Alpha / Luminance (Standard)"),
                                   static_cast<int>(BitmapBinarizationMode::AlphaOrLuminance));
    m_pngBinarizationCombo->addItem(tr("Color Distance (Background Keying)"),
                                   static_cast<int>(BitmapBinarizationMode::ColorDistance));
    m_pngBinarizationCombo->addItem(tr("Dominant Color Palette"),
                                   static_cast<int>(BitmapBinarizationMode::ColorPalette));
    m_pngBinarizationCombo->addItem(tr("Edge Boundaries (Sobel Filter)"),
                                   static_cast<int>(BitmapBinarizationMode::EdgeGradient));
    bModeRow->addWidget(m_pngBinarizationCombo, 1);
    pngLayout->addLayout(bModeRow);

    // Alpha / Luminance controls container
    m_pngAlphaLumaWidget = new QWidget(m_pngTraceGroup);
    auto* alphaLumaLayout = new QVBoxLayout(m_pngAlphaLumaWidget);
    alphaLumaLayout->setContentsMargins(0, 0, 0, 0);

    m_pngUseAlphaCheck = new QCheckBox(
        tr("Use alpha channel transparency (auto-fallback to luminance for opaque PNGs)"),
        m_pngAlphaLumaWidget);
    m_pngUseAlphaCheck->setChecked(true);
    alphaLumaLayout->addWidget(m_pngUseAlphaCheck);

    auto* threshRow = new QHBoxLayout();
    m_pngThresholdLabel = new QLabel(tr("Binarization Threshold:"), m_pngAlphaLumaWidget);
    threshRow->addWidget(m_pngThresholdLabel);
    m_pngThresholdSlider = new QSlider(Qt::Horizontal, m_pngAlphaLumaWidget);
    m_pngThresholdSlider->setRange(1, 254);
    m_pngThresholdSlider->setValue(128);
    m_pngThresholdSpin = new QSpinBox(m_pngAlphaLumaWidget);
    m_pngThresholdSpin->setRange(1, 254);
    m_pngThresholdSpin->setValue(128);
    threshRow->addWidget(m_pngThresholdSlider, 1);
    threshRow->addWidget(m_pngThresholdSpin);
    alphaLumaLayout->addLayout(threshRow);
    pngLayout->addWidget(m_pngAlphaLumaWidget);

    // Color Distance controls container
    m_pngColorDistanceWidget = new QWidget(m_pngTraceGroup);
    auto* distLayout = new QVBoxLayout(m_pngColorDistanceWidget);
    distLayout->setContentsMargins(0, 0, 0, 0);

    auto* bgRow = new QHBoxLayout();
    m_pngAutoDetectBgCheck = new QCheckBox(tr("Auto-detect background color"), m_pngColorDistanceWidget);
    m_pngAutoDetectBgCheck->setChecked(true);
    bgRow->addWidget(m_pngAutoDetectBgCheck);

    m_pngCustomBgColorBtn = new QPushButton(tr("Custom BG..."), m_pngColorDistanceWidget);
    m_pngCustomBgColorBtn->setEnabled(false);
    bgRow->addWidget(m_pngCustomBgColorBtn);

    m_pngDetectedBgPreview = new QLabel(m_pngColorDistanceWidget);
    m_pngDetectedBgPreview->setFixedSize(22, 22);
    m_pngDetectedBgPreview->setStyleSheet(QStringLiteral("background-color: #FFFFFF; border: 1px solid #777; border-radius: 3px;"));
    m_pngDetectedBgPreview->setToolTip(tr("Detected or chosen background color"));
    bgRow->addWidget(m_pngDetectedBgPreview);
    bgRow->addStretch(1);
    distLayout->addLayout(bgRow);

    auto* tolRow = new QHBoxLayout();
    tolRow->addWidget(new QLabel(tr("Background Tolerance:"), m_pngColorDistanceWidget));
    m_pngColorToleranceSlider = new QSlider(Qt::Horizontal, m_pngColorDistanceWidget);
    m_pngColorToleranceSlider->setRange(1, 100);
    m_pngColorToleranceSlider->setValue(15);
    m_pngColorToleranceLabel = new QLabel(QStringLiteral("15 %"), m_pngColorDistanceWidget);
    m_pngColorToleranceLabel->setMinimumWidth(40);
    tolRow->addWidget(m_pngColorToleranceSlider, 1);
    tolRow->addWidget(m_pngColorToleranceLabel);
    distLayout->addLayout(tolRow);
    pngLayout->addWidget(m_pngColorDistanceWidget);

    // Palette selection widget
    m_pngPaletteWidget = new QWidget(m_pngTraceGroup);
    auto* palOuterLayout = new QVBoxLayout(m_pngPaletteWidget);
    palOuterLayout->setContentsMargins(0, 0, 0, 0);
    palOuterLayout->addWidget(new QLabel(tr("Dominant Palette (Click colors to include/exclude):"), m_pngPaletteWidget));

    auto* palBtnContainer = new QWidget(m_pngPaletteWidget);
    m_pngPaletteLayout = new QHBoxLayout(palBtnContainer);
    m_pngPaletteLayout->setContentsMargins(0, 0, 0, 0);
    m_pngPaletteLayout->setSpacing(6);
    palOuterLayout->addWidget(palBtnContainer);

    auto* palActionRow = new QHBoxLayout();
    auto* selectAllBtn = new QPushButton(tr("Select All Non-BG"), m_pngPaletteWidget);
    auto* clearAllBtn = new QPushButton(tr("Clear All"), m_pngPaletteWidget);
    palActionRow->addWidget(selectAllBtn);
    palActionRow->addWidget(clearAllBtn);
    palActionRow->addStretch(1);
    palOuterLayout->addLayout(palActionRow);

    connect(selectAllBtn, &QPushButton::clicked, this, [this]() {
        m_selectedPaletteColors.clear();
        for (QPushButton* btn : m_pngPaletteButtons) {
            btn->setChecked(true);
        }
        refreshFilterPreview();
    });
    connect(clearAllBtn, &QPushButton::clicked, this, [this]() {
        m_selectedPaletteColors.clear();
        for (QPushButton* btn : m_pngPaletteButtons) {
            btn->setChecked(false);
        }
        refreshFilterPreview();
    });

    pngLayout->addWidget(m_pngPaletteWidget);

    m_pngInvertCheck =
        new QCheckBox(tr("Invert foreground / background (for light icons on dark background)"),
                      m_pngTraceGroup);
    m_pngInvertCheck->setChecked(false);

    auto* despeckleRow = new QHBoxLayout();
    despeckleRow->addWidget(new QLabel(tr("Ignore noise speckles smaller than:"), m_pngTraceGroup));
    m_pngDespeckleSpin = new QSpinBox(m_pngTraceGroup);
    m_pngDespeckleSpin->setRange(1, 200);
    m_pngDespeckleSpin->setValue(4);
    m_pngDespeckleSpin->setSuffix(tr(" px"));
    despeckleRow->addWidget(m_pngDespeckleSpin);
    despeckleRow->addStretch(1);

    auto* simplifyRow = new QHBoxLayout();
    simplifyRow->addWidget(
        new QLabel(tr("Polygon simplification tolerance:"), m_pngTraceGroup));
    m_pngSimplifySpin = new QDoubleSpinBox(m_pngTraceGroup);
    m_pngSimplifySpin->setRange(0.20, 8.0);
    m_pngSimplifySpin->setSingleStep(0.20);
    m_pngSimplifySpin->setDecimals(2);
    m_pngSimplifySpin->setValue(1.0);
    m_pngSimplifySpin->setSuffix(tr(" px"));
    simplifyRow->addWidget(m_pngSimplifySpin);
    simplifyRow->addStretch(1);

    auto* smoothRow = new QHBoxLayout();
    smoothRow->addWidget(new QLabel(tr("Curve smoothing:"), m_pngTraceGroup));
    m_pngSmoothingSlider = new QSlider(Qt::Horizontal, m_pngTraceGroup);
    m_pngSmoothingSlider->setRange(0, 100);
    m_pngSmoothingSlider->setValue(75);
    m_pngSmoothingValLabel = new QLabel(QStringLiteral("75 %"), m_pngTraceGroup);
    m_pngSmoothingValLabel->setMinimumWidth(40);
    smoothRow->addWidget(m_pngSmoothingSlider, 1);
    smoothRow->addWidget(m_pngSmoothingValLabel);

    pngLayout->addWidget(m_pngInvertCheck);
    pngLayout->addLayout(despeckleRow);
    pngLayout->addLayout(simplifyRow);
    pngLayout->addLayout(smoothRow);
    pngLayout->addStretch(1);
    m_pngTraceGroup->setVisible(false);

    updateBinarizationModeVisibility();

    // 3) FontForge Glyph Import Options group
    auto* importGroup = new QGroupBox(tr("FontForge Glyph Import Options"), this);
    auto* importLayout = new QVBoxLayout(importGroup);

    auto* modeRow = new QHBoxLayout();
    m_replaceRadio = new QRadioButton(tr("Replace existing contours"), importGroup);
    m_mergeRadio = new QRadioButton(tr("Merge into glyph"), importGroup);
    m_replaceRadio->setChecked(true);
    modeRow->addWidget(m_replaceRadio);
    modeRow->addWidget(m_mergeRadio);
    modeRow->addStretch(1);

    m_scaleToEmCheck = new QCheckBox(
        tr("Fit icon to font metrics (EM %1 units)").arg(fontSummary.emSize), importGroup);
    m_correctDirCheck =
        new QCheckBox(tr("Correct contour direction (clockwise / CCW)"), importGroup);
    m_removeOverlapCheck =
        new QCheckBox(tr("Remove overlapping contours after import"), importGroup);
    m_simplifyCheck = new QCheckBox(tr("Simplify curves during import"), importGroup);
    m_roundToIntCheck =
        new QCheckBox(tr("Round coordinates to integer font units"), importGroup);
    m_updateWidthCheck =
        new QCheckBox(tr("Set advance width from source dimensions"), importGroup);

    m_scaleToEmCheck->setChecked(true);
    m_correctDirCheck->setChecked(true);
    m_removeOverlapCheck->setChecked(false);
    m_simplifyCheck->setChecked(true);
    m_roundToIntCheck->setChecked(true);
    m_updateWidthCheck->setChecked(false);

    importLayout->addLayout(modeRow);
    importLayout->addWidget(m_scaleToEmCheck);
    importLayout->addWidget(m_correctDirCheck);
    importLayout->addWidget(m_removeOverlapCheck);
    importLayout->addWidget(m_simplifyCheck);
    importLayout->addWidget(m_roundToIntCheck);
    importLayout->addWidget(m_updateWidthCheck);
    importLayout->addStretch(1);

    optionsRow->addWidget(m_svgFilterGroup, 1);
    optionsRow->addWidget(m_pngTraceGroup, 1);
    optionsRow->addWidget(importGroup, 1);
    rootLayout->addLayout(optionsRow);

    auto* bottomRow = new QHBoxLayout();
    auto* saveDefBtn = new QPushButton(tr("Save as Default"), this);
    saveDefBtn->setToolTip(tr("Save current import parameters as application-wide default"));
    auto* resetDefBtn = new QPushButton(tr("Reset Defaults"), this);
    resetDefBtn->setToolTip(tr("Reset import parameters to factory defaults"));
    bottomRow->addWidget(saveDefBtn);
    bottomRow->addWidget(resetDefBtn);
    bottomRow->addStretch(1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_importBtn = buttons->addButton(tr("Import into Glyph"), QDialogButtonBox::AcceptRole);
    m_importBtn->setEnabled(false);
    bottomRow->addWidget(buttons);
    rootLayout->addLayout(bottomRow);

    // Wire signals
    connect(browseBtn, &QPushButton::clicked, this, &ImportSvgDialog::onBrowseClicked);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(saveDefBtn, &QPushButton::clicked, this, &ImportSvgDialog::onSaveAsDefaultClicked);
    connect(resetDefBtn, &QPushButton::clicked, this, &ImportSvgDialog::onResetToDefaultsClicked);

    for (QCheckBox* cb : {m_stripMetadataCheck, m_removeHiddenCheck, m_removeBgRectsCheck,
                          m_stripNoFillCheck, m_keepStrokesCheck, m_normalizeBlackCheck,
                          m_whiteCutoutCheck, m_stripTinyCheck}) {
        connect(cb, &QCheckBox::toggled, this, &ImportSvgDialog::refreshFilterPreview);
    }
    connect(m_minExtentSpin, &QDoubleSpinBox::valueChanged, this,
            &ImportSvgDialog::refreshFilterPreview);
    connect(m_stripTinyCheck, &QCheckBox::toggled, m_minExtentSpin, &QWidget::setEnabled);

    // PNG tracer signals
    connect(m_pngBinarizationCombo, &QComboBox::currentIndexChanged, this, [this]() {
        updateBinarizationModeVisibility();
        refreshFilterPreview();
    });
    connect(m_pngAutoDetectBgCheck, &QCheckBox::toggled, this, [this](bool autoDetect) {
        m_pngCustomBgColorBtn->setEnabled(!autoDetect);
        refreshFilterPreview();
    });
    connect(m_pngCustomBgColorBtn, &QPushButton::clicked, this, [this]() {
        const QColor col = QColorDialog::getColor(QColor(m_customBgColor), this, tr("Select Background Color"));
        if (col.isValid()) {
            m_customBgColor = col.rgb();
            m_pngDetectedBgPreview->setStyleSheet(
                QStringLiteral("background-color: %1; border: 1px solid #777; border-radius: 3px;").arg(col.name()));
            refreshFilterPreview();
        }
    });
    connect(m_pngColorToleranceSlider, &QSlider::valueChanged, this, [this](int val) {
        m_pngColorToleranceLabel->setText(QStringLiteral("%1 %").arg(val));
        refreshFilterPreview();
    });
    connect(m_pngUseAlphaCheck, &QCheckBox::toggled, this, &ImportSvgDialog::refreshFilterPreview);
    connect(m_pngInvertCheck, &QCheckBox::toggled, this, &ImportSvgDialog::refreshFilterPreview);
    connect(m_pngThresholdSlider, &QSlider::valueChanged, m_pngThresholdSpin, &QSpinBox::setValue);
    connect(m_pngThresholdSpin, &QSpinBox::valueChanged, m_pngThresholdSlider, &QSlider::setValue);
    connect(m_pngThresholdSpin, &QSpinBox::valueChanged, this,
            &ImportSvgDialog::refreshFilterPreview);
    connect(m_pngDespeckleSpin, &QSpinBox::valueChanged, this,
            &ImportSvgDialog::refreshFilterPreview);
    connect(m_pngSimplifySpin, &QDoubleSpinBox::valueChanged, this,
            &ImportSvgDialog::refreshFilterPreview);
    connect(m_pngSmoothingSlider, &QSlider::valueChanged, this, [this](int val) {
        m_pngSmoothingValLabel->setText(QStringLiteral("%1 %").arg(val));
        refreshFilterPreview();
    });

    // Placement signals
    connect(m_placementModeCombo, &QComboBox::currentIndexChanged, this,
            &ImportSvgDialog::updatePlacementPreview);
    connect(m_scaleSpin, &QDoubleSpinBox::valueChanged, this,
            &ImportSvgDialog::updatePlacementPreview);
    connect(m_offsetXSpin, &QDoubleSpinBox::valueChanged, this,
            &ImportSvgDialog::updatePlacementPreview);
    connect(m_offsetYSpin, &QDoubleSpinBox::valueChanged, this,
            &ImportSvgDialog::updatePlacementPreview);
    connect(m_resetPlacementBtn, &QPushButton::clicked, this,
            &ImportSvgDialog::resetPlacementControls);

    connect(m_scaleToEmCheck, &QCheckBox::toggled, this,
            &ImportSvgDialog::updatePlacementPreview);
    connect(m_updateWidthCheck, &QCheckBox::toggled, this,
            &ImportSvgDialog::updatePlacementPreview);
    connect(m_mergeRadio, &QRadioButton::toggled, this,
            &ImportSvgDialog::updatePlacementPreview);

    m_placementPreview->setCallbacks(
        [this](double newOffsetX, double newOffsetY) {
            m_syncingPlacementControls = true;
            m_offsetXSpin->setValue(newOffsetX);
            m_offsetYSpin->setValue(newOffsetY);
            m_syncingPlacementControls = false;
            updatePlacementPreview();
        },
        [this](double newScalePercent) {
            m_syncingPlacementControls = true;
            m_scaleSpin->setValue(newScalePercent);
            m_syncingPlacementControls = false;
            updatePlacementPreview();
        },
        [this]() { resetPlacementControls(); });

    applyDefaultOptions(loadDefaultImportOptions());
    updatePlacementPreview();

    if (!initialFilePath.isEmpty()) {
        loadInputFile(initialFilePath);
    }
}

void ImportSvgDialog::dragEnterEvent(QDragEnterEvent* event) {
    if (!extractDroppedImportPath(event->mimeData()).isEmpty()) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void ImportSvgDialog::dropEvent(QDropEvent* event) {
    const QString path = extractDroppedImportPath(event->mimeData());
    if (!path.isEmpty()) {
        event->acceptProposedAction();
        loadInputFile(path);
        return;
    }
    event->ignore();
}

void ImportSvgDialog::onBrowseClicked() {
    QString startDir = !m_pathEdit->text().isEmpty()
                           ? QFileInfo(m_pathEdit->text()).absolutePath()
                           : loadLastSvgDirectory();
    if (startDir.isEmpty() && !m_fontSummary.filePath.isEmpty()) {
        startDir = QFileInfo(m_fontSummary.filePath).absolutePath();
    }
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select SVG or PNG Icon File"), startDir,
        tr("Supported Icon Files (*.svg *.png);;SVG Vector Files (*.svg);;PNG Raster Icons (*.png);;All Files (*)"));
    if (!path.isEmpty()) {
        loadInputFile(path);
    }
}

void ImportSvgDialog::loadInputFile(const QString& path) {
    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile()) {
        return;
    }

    saveLastSvgDirectory(path);
    m_pathEdit->setText(path);

    const bool isPng = fi.suffix().compare(QStringLiteral("png"), Qt::CaseInsensitive) == 0;
    m_isPngInput = isPng;

    // Show only the controls relevant to the active file format
    m_colorGroup->setVisible(!isPng);
    m_svgFilterGroup->setVisible(!isPng);
    m_pngTraceGroup->setVisible(isPng);

    if (isPng) {
        m_rawSvg.clear();
        m_rawImage = QImage(path);
        m_selectedPaletteColors.clear();
        m_lastTraceResult = BitmapTraceResult{};
        updatePaletteButtons({}, 0xFFFFFFFF);
        if (m_rawImage.isNull()) {
            m_originalPreview->setTitle(tr("Original PNG Image"));
            m_originalPreview->setRasterImage(QImage(), tr("Failed to load PNG image."));
            m_placementPreview->setContent(QByteArray(), QRectF(), QRectF(), importOptions());
            m_importBtn->setEnabled(false);
            return;
        }
        m_originalPreview->setTitle(
            tr("Original PNG (%1×%2 px)").arg(m_rawImage.width()).arg(m_rawImage.height()));
        m_originalPreview->setRasterImage(m_rawImage);
        refreshFilterPreview();
        return;
    }

    m_rawImage = QImage();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_rawSvg.clear();
        m_originalPreview->setTitle(tr("Original SVG"));
        m_originalPreview->setSvgData(QByteArray(),
                                      tr("Failed to open: %1").arg(file.errorString()));
        m_placementPreview->setContent(QByteArray(), QRectF(), QRectF(), importOptions());
        m_importBtn->setEnabled(false);
        return;
    }

    m_rawSvg = file.readAll();
    m_originalPreview->setTitle(tr("Original SVG"));
    m_originalPreview->setSvgData(m_rawSvg);

    // Discover all non-hidden geometry colors before user color exclusions.
    SvgFilterOptions discoveryOpts = currentFilterOptions();
    discoveryOpts.excludedColors.clear();
    discoveryOpts.includedColors.clear();
    const SvgSanitizeResult discovery = SvgSanitizer::sanitize(m_rawSvg, discoveryOpts);
    rebuildColorCheckboxes(discovery.detectedColors);

    refreshFilterPreview();
}

void ImportSvgDialog::rebuildColorCheckboxes(const QStringList& colors) {
    for (auto it = m_colorChecks.begin(); it != m_colorChecks.end(); ++it) {
        m_colorLayout->removeWidget(it.value());
        it.value()->deleteLater();
    }
    m_colorChecks.clear();

    m_noColorsLabel->setVisible(colors.isEmpty());
    for (const QString& hex : colors) {
        auto* cb = new QCheckBox(hex, m_colorContainer);
        cb->setIcon(makeColorSwatchIcon(hex));
        cb->setChecked(true);
        cb->setToolTip(tr("Uncheck to exclude shapes colored %1").arg(hex));
        connect(cb, &QCheckBox::toggled, this, &ImportSvgDialog::refreshFilterPreview);
        m_colorLayout->insertWidget(m_colorLayout->count() - 1, cb);
        m_colorChecks.insert(hex, cb);
    }
}

SvgFilterOptions ImportSvgDialog::currentFilterOptions() const {
    SvgFilterOptions opts;
    opts.stripEditorMetadata = m_stripMetadataCheck->isChecked();
    opts.removeHiddenElements = m_removeHiddenCheck->isChecked();
    opts.removeBackgroundRects = m_removeBgRectsCheck->isChecked();
    opts.stripNoFillElements = m_stripNoFillCheck->isChecked();
    opts.keepStrokes = m_keepStrokesCheck->isChecked();
    opts.normalizeToBlackFill = m_normalizeBlackCheck->isChecked();
    opts.whiteAsCutout = m_whiteCutoutCheck->isChecked();
    opts.stripTinyPaths = m_stripTinyCheck->isChecked();
    opts.minGeometryExtent = m_minExtentSpin->value();
    for (auto it = m_colorChecks.cbegin(); it != m_colorChecks.cend(); ++it) {
        if (!it.value()->isChecked()) {
            opts.excludedColors.append(it.key());
        }
    }
    return opts;
}

BitmapTraceOptions ImportSvgDialog::currentTraceOptions() const {
    BitmapTraceOptions opts;
    opts.binarizationMode = static_cast<BitmapBinarizationMode>(
        m_pngBinarizationCombo ? m_pngBinarizationCombo->currentData().toInt() : 0);
    opts.useAlpha = m_pngUseAlphaCheck->isChecked();
    opts.threshold = m_pngThresholdSpin->value();
    opts.invert = m_pngInvertCheck->isChecked();
    opts.minSpeckleArea = m_pngDespeckleSpin->value();
    opts.simplifyTolerance = m_pngSimplifySpin->value();
    opts.curveSmoothing = static_cast<double>(m_pngSmoothingSlider->value()) / 100.0;

    opts.autoDetectBgColor = m_pngAutoDetectBgCheck ? m_pngAutoDetectBgCheck->isChecked() : true;
    opts.customBgColor = m_customBgColor;
    opts.colorTolerance = m_pngColorToleranceSlider
                              ? (static_cast<double>(m_pngColorToleranceSlider->value()) / 100.0)
                              : 0.15;
    opts.selectedPaletteColors = m_selectedPaletteColors;
    return opts;
}

double ImportSvgDialog::effectivePreviewAdvanceWidth() const {
    const int emSize = m_fontSummary.emSize > 0
                           ? m_fontSummary.emSize
                           : (m_fontSummary.ascent + m_fontSummary.descent);
    const double srcW =
        m_lastResult.width > 0.0 ? m_lastResult.width : m_lastResult.viewBox.width();
    const double srcH =
        m_lastResult.height > 0.0 ? m_lastResult.height : m_lastResult.viewBox.height();
    if (m_updateWidthCheck->isChecked() && srcW > 0.0) {
        return (m_scaleToEmCheck->isChecked() && srcH > 0.0)
                   ? srcW * (static_cast<double>(emSize) / srcH)
                   : srcW;
    }
    return m_targetGlyph.width > 0 ? static_cast<double>(m_targetGlyph.width)
                                   : static_cast<double>(emSize);
}

void ImportSvgDialog::resetPlacementControls() {
    m_syncingPlacementControls = true;
    m_scaleSpin->setValue(100.0);
    m_offsetXSpin->setValue(0.0);
    m_offsetYSpin->setValue(0.0);
    m_syncingPlacementControls = false;
    updatePlacementPreview();
}

void ImportSvgDialog::refreshFilterPreview() {
    if (m_isPngInput) {
        if (m_rawImage.isNull()) {
            m_placementPreview->setContent(QByteArray(), QRectF(), QRectF(), importOptions());
            m_importBtn->setEnabled(false);
            return;
        }

        m_lastTraceResult = BitmapTracer::trace(m_rawImage, currentTraceOptions());
        m_lastResult = SvgSanitizeResult{};
        if (!m_lastTraceResult.valid) {
            m_placementPreview->setContent(QByteArray(), QRectF(), QRectF(), importOptions(),
                                           m_lastTraceResult.errorMessage);
            m_statsLabel->setText(
                tr("<b>PNG Trace Error:</b> %1").arg(m_lastTraceResult.errorMessage.toHtmlEscaped()));
            m_importBtn->setEnabled(false);
            return;
        }

        m_lastResult.valid = true;
        m_lastResult.cleanedSvg = m_lastTraceResult.cleanedSvg;
        m_lastResult.solidSvg = m_lastTraceResult.solidSvg;
        m_lastResult.cutoutSvg = m_lastTraceResult.cutoutSvg;
        m_lastResult.viewBox = m_lastTraceResult.viewBox;
        m_lastResult.width = m_lastTraceResult.viewBox.width();
        m_lastResult.height = m_lastTraceResult.viewBox.height();
        m_lastResult.keptGeometryCount = m_lastTraceResult.totalContours;
        m_lastResult.cutoutGeometryCount = m_lastTraceResult.holeContours;
        m_currentInkBounds = m_lastTraceResult.inkBounds;

        if (m_pngDetectedBgPreview) {
            const QColor detCol(m_lastTraceResult.detectedBgColor);
            m_pngDetectedBgPreview->setStyleSheet(
                QStringLiteral("background-color: %1; border: 1px solid #777; border-radius: 3px;").arg(detCol.name()));
            m_pngDetectedBgPreview->setToolTip(tr("Detected/Active Background: %1").arg(detCol.name()));
        }

        if (m_pngBinarizationCombo &&
            m_pngBinarizationCombo->currentData().toInt() == static_cast<int>(BitmapBinarizationMode::ColorPalette)) {
            if (m_pngPaletteButtons.size() != m_lastTraceResult.extractedPalette.size()) {
                updatePaletteButtons(m_lastTraceResult.extractedPalette, m_lastTraceResult.detectedBgColor);
            }
        }

        if (m_lastTraceResult.totalContours == 0) {
            m_placementPreview->setContent(QByteArray(), m_lastResult.viewBox, QRectF(),
                                           importOptions(),
                                           tr("No foreground contours found at current threshold"));
            m_statsLabel->setText(
                tr("Traced 0 contours — adjust the threshold slider or toggle 'Invert foreground / background'."));
            m_importBtn->setEnabled(false);
            return;
        }

        QString modeStr;
        switch (currentTraceOptions().binarizationMode) {
        case BitmapBinarizationMode::ColorDistance:
            modeStr = tr("Color distance");
            break;
        case BitmapBinarizationMode::ColorPalette:
            modeStr = tr("Color palette");
            break;
        case BitmapBinarizationMode::EdgeGradient:
            modeStr = tr("Sobel edges");
            break;
        default:
            modeStr = m_lastTraceResult.usedAlphaChannel ? tr("Alpha channel") : tr("Luminance");
            break;
        }
        m_statsLabel->setText(
            tr("PNG Traced (%1) — Image: %2×%3 px · Contours: <b>%4</b> (%5 outer, %6 holes) · "
               "Control nodes: %7")
                .arg(modeStr)
                .arg(m_lastTraceResult.imageSize.width())
                .arg(m_lastTraceResult.imageSize.height())
                .arg(m_lastTraceResult.totalContours)
                .arg(m_lastTraceResult.totalContours - m_lastTraceResult.holeContours)
                .arg(m_lastTraceResult.holeContours)
                .arg(m_lastTraceResult.totalNodes));
        m_importBtn->setEnabled(true);
        updatePlacementPreview();
        return;
    }

    if (m_rawSvg.isEmpty()) {
        m_placementPreview->setContent(QByteArray(), QRectF(), QRectF(), importOptions());
        m_importBtn->setEnabled(false);
        return;
    }

    m_lastResult = SvgSanitizer::sanitize(m_rawSvg, currentFilterOptions());
    if (!m_lastResult.valid) {
        m_placementPreview->setContent(QByteArray(), QRectF(), QRectF(), importOptions(),
                                       m_lastResult.errorMessage);
        m_statsLabel->setText(
            tr("<b>SVG Error:</b> %1").arg(m_lastResult.errorMessage.toHtmlEscaped()));
        m_importBtn->setEnabled(false);
        return;
    }

    if (m_lastResult.keptGeometryCount == 0) {
        m_placementPreview->setContent(QByteArray(), m_lastResult.viewBox, QRectF(),
                                       importOptions(), tr("All geometry filtered out"));
        m_statsLabel->setText(
            tr("Kept 0 shapes (removed %1 metadata, %2 hidden/bg, %3 color-filtered, %4 tiny/empty). "
               "Relax filter options to keep outlines.")
                .arg(m_lastResult.removedMetadataCount)
                .arg(m_lastResult.removedHiddenCount)
                .arg(m_lastResult.removedColorFilteredCount)
                .arg(m_lastResult.removedTinyOrEmptyCount));
        m_importBtn->setEnabled(false);
        return;
    }

    m_currentInkBounds =
        BitmapTracer::computeSvgInkBounds(m_lastResult.cleanedSvg, m_lastResult.viewBox);

    const QString vbStr =
        m_lastResult.viewBox.isValid()
            ? QStringLiteral("%1×%2")
                  .arg(m_lastResult.viewBox.width(), 0, 'f', 1)
                  .arg(m_lastResult.viewBox.height(), 0, 'f', 1)
            : QStringLiteral("auto");
    const QString cutoutInfo =
        (m_lastResult.cutoutGeometryCount > 0)
            ? tr(" (%1 solid, %2 cutout)")
                  .arg(m_lastResult.keptGeometryCount - m_lastResult.cutoutGeometryCount)
                  .arg(m_lastResult.cutoutGeometryCount)
            : QString();
    m_statsLabel->setText(
        tr("Ready — ViewBox: %1 · Kept geometry: <b>%2</b>%3 · Removed: %4 metadata, %5 hidden/bg, "
           "%6 color-filtered, %7 tiny/empty")
            .arg(vbStr)
            .arg(m_lastResult.keptGeometryCount)
            .arg(cutoutInfo)
            .arg(m_lastResult.removedMetadataCount)
            .arg(m_lastResult.removedHiddenCount)
            .arg(m_lastResult.removedColorFilteredCount)
            .arg(m_lastResult.removedTinyOrEmptyCount));
    m_importBtn->setEnabled(true);
    updatePlacementPreview();
}

void ImportSvgDialog::updatePlacementPreview() {
    if (m_syncingPlacementControls || !m_placementPreview) {
        return;
    }
    m_placementPreview->setGlyphContext(
        m_fontSummary.ascent, m_fontSummary.descent, effectivePreviewAdvanceWidth(),
        m_targetGlyph.outline, m_mergeRadio->isChecked() && m_targetGlyph.hasOutline);

    if (m_lastResult.valid && m_lastResult.keptGeometryCount > 0) {
        m_placementPreview->setContent(m_lastResult.cleanedSvg, m_lastResult.viewBox,
                                       m_currentInkBounds, importOptions());
    } else {
        m_placementPreview->setContent(QByteArray(), QRectF(), QRectF(), importOptions());
    }
}

SvgImportOptions ImportSvgDialog::importOptions() const {
    SvgImportOptions opts;
    opts.gid = m_targetGlyph.gid;
    opts.svgData = !m_lastResult.solidSvg.isEmpty() ? m_lastResult.solidSvg : m_lastResult.cleanedSvg;
    opts.strokeSvgData = m_lastResult.strokeSvg;
    opts.cutoutSvgData = m_lastResult.cutoutSvg;
    opts.replaceExistingContours = m_replaceRadio->isChecked();
    opts.scaleToEm = m_scaleToEmCheck->isChecked();
    opts.correctDirection = m_correctDirCheck->isChecked();
    opts.removeOverlap = m_removeOverlapCheck->isChecked();
    opts.simplify = m_simplifyCheck->isChecked();
    opts.roundToInt = m_roundToIntCheck->isChecked();
    opts.updateAdvanceWidth = m_updateWidthCheck->isChecked();
    opts.svgWidth = m_lastResult.width > 0.0 ? m_lastResult.width : m_lastResult.viewBox.width();
    opts.svgHeight = m_lastResult.height > 0.0 ? m_lastResult.height : m_lastResult.viewBox.height();

    opts.placementMode = static_cast<GlyphPlacementMode>(
        m_placementModeCombo ? m_placementModeCombo->currentData().toInt() : 0);
    opts.scaleFactor = m_scaleSpin ? (m_scaleSpin->value() / 100.0) : 1.0;
    opts.offsetX = m_offsetXSpin ? m_offsetXSpin->value() : 0.0;
    opts.offsetY = m_offsetYSpin ? m_offsetYSpin->value() : 0.0;

    opts.normalizeToBlackFill = m_normalizeBlackCheck->isChecked();
    opts.whiteAsCutout = m_whiteCutoutCheck->isChecked();
    opts.keepStrokes = m_keepStrokesCheck->isChecked();
    opts.stripTinyPaths = m_stripTinyCheck->isChecked();
    opts.minGeometryExtent = m_minExtentSpin->value();
    opts.useAlpha = m_pngUseAlphaCheck->isChecked();
    opts.bitmapThreshold = m_pngThresholdSpin->value();
    opts.invertBitmap = m_pngInvertCheck->isChecked();
    opts.minSpeckleArea = m_pngDespeckleSpin->value();
    opts.curveSmoothing = m_pngSmoothingSlider->value() / 100.0;
    opts.binarizationMode = static_cast<BitmapBinarizationMode>(
        m_pngBinarizationCombo ? m_pngBinarizationCombo->currentData().toInt() : 0);
    opts.autoDetectBgColor = m_pngAutoDetectBgCheck ? m_pngAutoDetectBgCheck->isChecked() : true;
    opts.customBgColor = QColor(m_customBgColor).name();
    opts.colorTolerance = m_pngColorToleranceSlider
                              ? (static_cast<double>(m_pngColorToleranceSlider->value()) / 100.0)
                              : 0.15;
    return opts;
}

void ImportSvgDialog::applyDefaultOptions(const SvgImportOptions& opts) {
    if (m_placementModeCombo) {
        const int idx = m_placementModeCombo->findData(static_cast<int>(opts.placementMode));
        if (idx >= 0) m_placementModeCombo->setCurrentIndex(idx);
    }
    if (m_scaleSpin) m_scaleSpin->setValue((opts.scaleFactor > 0.01 ? opts.scaleFactor : 1.0) * 100.0);
    if (m_scaleToEmCheck) m_scaleToEmCheck->setChecked(opts.scaleToEm);
    if (m_simplifyCheck) m_simplifyCheck->setChecked(opts.simplify);
    if (m_correctDirCheck) m_correctDirCheck->setChecked(opts.correctDirection);
    if (m_removeOverlapCheck) m_removeOverlapCheck->setChecked(opts.removeOverlap);
    if (m_roundToIntCheck) m_roundToIntCheck->setChecked(opts.roundToInt);
    if (m_normalizeBlackCheck) m_normalizeBlackCheck->setChecked(opts.normalizeToBlackFill);
    if (m_whiteCutoutCheck) m_whiteCutoutCheck->setChecked(opts.whiteAsCutout);
    if (m_keepStrokesCheck) m_keepStrokesCheck->setChecked(opts.keepStrokes);
    if (m_stripTinyCheck) m_stripTinyCheck->setChecked(opts.stripTinyPaths);
    if (m_minExtentSpin) m_minExtentSpin->setValue(opts.minGeometryExtent > 0.0 ? opts.minGeometryExtent : 1.0);
    if (m_pngUseAlphaCheck) m_pngUseAlphaCheck->setChecked(opts.useAlpha);
    if (m_pngInvertCheck) m_pngInvertCheck->setChecked(opts.invertBitmap);
    if (m_pngThresholdSpin) m_pngThresholdSpin->setValue(opts.bitmapThreshold > 0 ? opts.bitmapThreshold : 128);
    if (m_pngDespeckleSpin) m_pngDespeckleSpin->setValue(opts.minSpeckleArea >= 0 ? opts.minSpeckleArea : 4);
    if (m_pngSmoothingSlider) m_pngSmoothingSlider->setValue(static_cast<int>((opts.curveSmoothing >= 0.0 ? opts.curveSmoothing : 0.75) * 100.0));

    if (m_pngBinarizationCombo) {
        const int idx = m_pngBinarizationCombo->findData(static_cast<int>(opts.binarizationMode));
        if (idx >= 0) m_pngBinarizationCombo->setCurrentIndex(idx);
    }
    if (m_pngAutoDetectBgCheck) {
        m_pngAutoDetectBgCheck->setChecked(opts.autoDetectBgColor);
    }
    if (!opts.customBgColor.isEmpty()) {
        const QColor c(opts.customBgColor);
        if (c.isValid()) {
            m_customBgColor = c.rgb();
            if (m_pngDetectedBgPreview) {
                m_pngDetectedBgPreview->setStyleSheet(
                    QStringLiteral("background-color: %1; border: 1px solid #777; border-radius: 3px;").arg(c.name()));
            }
        }
    }
    if (m_pngColorToleranceSlider) {
        const int tolVal = std::clamp(static_cast<int>(std::round(opts.colorTolerance * 100.0)), 1, 100);
        m_pngColorToleranceSlider->setValue(tolVal);
        if (m_pngColorToleranceLabel) {
            m_pngColorToleranceLabel->setText(QStringLiteral("%1 %").arg(tolVal));
        }
    }
    updateBinarizationModeVisibility();
}

void ImportSvgDialog::updateBinarizationModeVisibility() {
    if (!m_pngBinarizationCombo) {
        return;
    }
    const auto mode = static_cast<BitmapBinarizationMode>(
        m_pngBinarizationCombo->currentData().toInt());

    const bool isAlphaLuma = (mode == BitmapBinarizationMode::AlphaOrLuminance);
    const bool isEdge = (mode == BitmapBinarizationMode::EdgeGradient);
    const bool isDist = (mode == BitmapBinarizationMode::ColorDistance);
    const bool isPalette = (mode == BitmapBinarizationMode::ColorPalette);

    if (m_pngAlphaLumaWidget) {
        m_pngAlphaLumaWidget->setVisible(isAlphaLuma || isEdge);
    }
    if (m_pngUseAlphaCheck) {
        m_pngUseAlphaCheck->setVisible(isAlphaLuma);
    }
    if (m_pngThresholdLabel) {
        m_pngThresholdLabel->setText(isEdge ? tr("Edge Sensitivity:") : tr("Binarization Threshold:"));
    }

    if (m_pngColorDistanceWidget) {
        m_pngColorDistanceWidget->setVisible(isDist || isPalette);
    }
    if (m_pngPaletteWidget) {
        m_pngPaletteWidget->setVisible(isPalette);
    }
}

void ImportSvgDialog::updatePaletteButtons(const QVector<QRgb>& palette, QRgb bg) {
    if (!m_pngPaletteLayout) {
        return;
    }

    QLayoutItem* item = nullptr;
    while ((item = m_pngPaletteLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_pngPaletteButtons.clear();

    if (palette.isEmpty()) {
        return;
    }

    if (m_selectedPaletteColors.isEmpty()) {
        for (QRgb c : palette) {
            if (BitmapTracer::perceptualColorDistance(c, bg) >= 0.15) {
                m_selectedPaletteColors.append(c);
            }
        }
    }

    for (QRgb color : palette) {
        const QColor qcol(color);
        auto* btn = new QPushButton(m_pngPaletteWidget);
        btn->setCheckable(true);
        btn->setFixedSize(28, 28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tr("Color %1 (Click to include/exclude)").arg(qcol.name()));

        const bool isSelected = m_selectedPaletteColors.contains(color);
        btn->setChecked(isSelected);

        auto updateStyle = [btn, qcol](bool checked) {
            btn->setStyleSheet(QStringLiteral(
                "QPushButton {"
                "  background-color: %1;"
                "  border: %2;"
                "  border-radius: 4px;"
                "}"
            ).arg(qcol.name(), checked ? QStringLiteral("3px solid #0078D4") : QStringLiteral("1px solid #777")));
        };
        updateStyle(isSelected);

        connect(btn, &QPushButton::toggled, this, [this, color, updateStyle, btn](bool checked) {
            updateStyle(checked);
            if (checked) {
                if (!m_selectedPaletteColors.contains(color)) {
                    m_selectedPaletteColors.append(color);
                }
            } else {
                m_selectedPaletteColors.removeAll(color);
            }
            refreshFilterPreview();
        });

        m_pngPaletteButtons.append(btn);
        m_pngPaletteLayout->addWidget(btn);
    }
    m_pngPaletteLayout->addStretch(1);
}

void ImportSvgDialog::onSaveAsDefaultClicked() {
    saveDefaultImportOptions(importOptions());
    QMessageBox::information(this, tr("Saved Defaults"),
                             tr("Current parameters saved as application default for SVG and PNG imports."));
}

void ImportSvgDialog::onResetToDefaultsClicked() {
    resetDefaultImportOptions();
    applyDefaultOptions(SvgImportOptions());
    refreshFilterPreview();
    updatePlacementPreview();
    QMessageBox::information(this, tr("Reset Defaults"),
                             tr("Import parameters have been reset to factory defaults."));
}

} // namespace ffgui
