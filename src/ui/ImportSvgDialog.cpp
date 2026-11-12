#include "ui/ImportSvgDialog.hpp"

#include <QCheckBox>
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
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSvgRenderer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace ffgui {

namespace {

constexpr auto kSettingsOrg = "FontForgeGUI";
constexpr auto kSettingsApp = "FontForgeGUI";
constexpr auto kLastSvgDirKey = "importSvg/lastDirectory";

QString extractDroppedSvgPath(const QMimeData* mimeData) {
    if (!mimeData || !mimeData->hasUrls()) {
        return {};
    }
    for (const QUrl& url : mimeData->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString localPath = url.toLocalFile();
        const QFileInfo fi(localPath);
        if (fi.exists() && fi.isFile()
            && fi.suffix().compare(QStringLiteral("svg"), Qt::CaseInsensitive) == 0) {
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

void saveLastSvgDirectory(const QString& svgFilePath) {
    const QString dir = QFileInfo(svgFilePath).absolutePath();
    if (dir.isEmpty()) {
        return;
    }
    QSettings settings(QString::fromLatin1(kSettingsOrg), QString::fromLatin1(kSettingsApp));
    settings.setValue(QString::fromLatin1(kLastSvgDirKey), dir);
}

} // namespace

class SvgPreviewWidget : public QWidget {
public:
    explicit SvgPreviewWidget(const QString& title, QWidget* parent = nullptr)
        : QWidget(parent), m_title(title) {
        setMinimumSize(210, 190);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setDropHandler(std::function<void(const QString&)> handler,
                        const QString& emptyPlaceholder = QString()) {
        m_dropHandler = std::move(handler);
        m_emptyPlaceholder = emptyPlaceholder;
        setAcceptDrops(static_cast<bool>(m_dropHandler));
        update();
    }

    void setSvgData(const QByteArray& svgBytes, const QString& placeholderText = QString()) {
        m_placeholder = placeholderText;
        m_valid = !svgBytes.isEmpty() && m_renderer.load(svgBytes);
        if (!m_valid && m_placeholder.isEmpty() && !svgBytes.isEmpty()) {
            m_placeholder = tr("Cannot render SVG");
        }
        update();
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        if (!m_dropHandler || extractDroppedSvgPath(event->mimeData()).isEmpty()) {
            event->ignore();
            return;
        }
        m_dragHover = true;
        update();
        event->acceptProposedAction();
    }

    void dragMoveEvent(QDragMoveEvent* event) override {
        if (!m_dropHandler || extractDroppedSvgPath(event->mimeData()).isEmpty()) {
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
        const QString svgPath = extractDroppedSvgPath(event->mimeData());
        if (svgPath.isEmpty()) {
            event->ignore();
            return;
        }
        event->acceptProposedAction();
        m_dropHandler(svgPath);
    }

    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

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
                !m_emptyPlaceholder.isEmpty() ? m_emptyPlaceholder : tr("No SVG loaded");
            p.drawText(canvasRect, Qt::AlignCenter | Qt::TextWordWrap,
                       m_placeholder.isEmpty() ? fallbackText : m_placeholder);
            return;
        }

        // Subtle checkerboard so light/white background layers are visible before filtering.
        constexpr int kTile = 10;
        p.save();
        p.setClipRect(canvasRect);
        for (int y = canvasRect.top(); y <= canvasRect.bottom(); y += kTile) {
            for (int x = canvasRect.left(); x <= canvasRect.right(); x += kTile) {
                const bool dark = (((x - canvasRect.left()) / kTile)
                                   + ((y - canvasRect.top()) / kTile)) % 2 != 0;
                p.fillRect(QRect(x, y, kTile, kTile),
                           dark ? QColor(232, 232, 236) : QColor(246, 246, 249));
            }
        }

        QSizeF defSize = m_renderer.defaultSize();
        if (!defSize.isValid() || defSize.width() <= 0.0 || defSize.height() <= 0.0) {
            defSize = QSizeF(100.0, 100.0);
        }
        const QSizeF scaled = defSize.scaled(canvasRect.size() * 0.9, Qt::KeepAspectRatio);
        const QRectF target(
            canvasRect.center().x() - scaled.width() * 0.5,
            canvasRect.center().y() - scaled.height() * 0.5,
            scaled.width(), scaled.height());
        m_renderer.render(&p, target);
        p.restore();
    }

private:
    QString m_title;
    QString m_placeholder;
    QString m_emptyPlaceholder;
    QSvgRenderer m_renderer;
    std::function<void(const QString&)> m_dropHandler;
    bool m_valid = false;
    bool m_dragHover = false;
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
                                 const QString& initialSvgPath, QWidget* parent)
    : QDialog(parent), m_targetGlyph(targetGlyph), m_fontSummary(fontSummary) {
    const QString uniLabel = (targetGlyph.unicode >= 0)
        ? QStringLiteral("U+%1").arg(targetGlyph.unicode, 4, 16, QLatin1Char('0')).toUpper()
        : tr("Unencoded");
    setWindowTitle(tr("Import SVG into Glyph %1 (#%2, %3)")
                       .arg(targetGlyph.name)
                       .arg(targetGlyph.gid)
                       .arg(uniLabel));
    setAcceptDrops(true);
    resize(780, 650);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(10);

    // File chooser row
    auto* fileRow = new QHBoxLayout();
    fileRow->addWidget(new QLabel(tr("SVG File:"), this));
    m_pathEdit = new QLineEdit(this);
    const QString lastDir = loadLastSvgDirectory();
    m_pathEdit->setPlaceholderText(
        lastDir.isEmpty()
            ? tr("Choose or drop an Inkscape / vector SVG file…")
            : tr("Drop an .svg file or click Browse… (last: %1)").arg(lastDir));
    m_pathEdit->setReadOnly(true);
    auto* browseBtn = new QPushButton(tr("Browse…"), this);
    fileRow->addWidget(m_pathEdit, 1);
    fileRow->addWidget(browseBtn);
    rootLayout->addLayout(fileRow);

    // Side-by-side preview
    auto* previewRow = new QHBoxLayout();
    previewRow->setSpacing(10);
    m_originalPreview = new SvgPreviewWidget(tr("Original SVG"), this);
    m_originalPreview->setDropHandler(
        [this](const QString& droppedPath) { loadSvgFile(droppedPath); },
        tr("No SVG loaded\nDrop an .svg file here or click Browse…"));
    m_filteredPreview = new SvgPreviewWidget(tr("Filtered SVG (Sent to FontForge)"), this);
    previewRow->addWidget(m_originalPreview, 1);
    previewRow->addWidget(m_filteredPreview, 1);
    rootLayout->addLayout(previewRow, 1);

    m_statsLabel = new QLabel(tr("Select an SVG file to inspect geometry and filter statistics."), this);
    m_statsLabel->setWordWrap(true);
    rootLayout->addWidget(m_statsLabel);

    // Color filter bar
    auto* colorGroup = new QGroupBox(tr("Detected Colors Filter (uncheck colors to exclude)"), this);
    auto* colorOuter = new QVBoxLayout(colorGroup);
    colorOuter->setContentsMargins(8, 6, 8, 6);
    m_colorContainer = new QWidget(colorGroup);
    m_colorLayout = new QHBoxLayout(m_colorContainer);
    m_colorLayout->setContentsMargins(0, 0, 0, 0);
    m_colorLayout->setSpacing(10);
    m_noColorsLabel = new QLabel(tr("No color attributes detected yet"), m_colorContainer);
    m_colorLayout->addWidget(m_noColorsLabel);
    m_colorLayout->addStretch(1);
    colorOuter->addWidget(m_colorContainer);
    rootLayout->addWidget(colorGroup);

    // Filter & Import options side by side
    auto* optionsRow = new QHBoxLayout();
    optionsRow->setSpacing(10);

    auto* filterGroup = new QGroupBox(tr("SVG Sanitizer Filter"), this);
    auto* filterLayout = new QVBoxLayout(filterGroup);
    m_stripMetadataCheck = new QCheckBox(
        tr("Strip editor metadata (Inkscape / Sodipodi / comments / styles)"), filterGroup);
    m_removeHiddenCheck = new QCheckBox(
        tr("Remove hidden & empty elements (display:none / opacity:0)"), filterGroup);
    m_removeBgRectsCheck = new QCheckBox(
        tr("Remove background / white rectangles"), filterGroup);
    m_stripNoFillCheck = new QCheckBox(
        tr("Remove unfilled helper shapes (fill:none)"), filterGroup);
    m_keepStrokesCheck = new QCheckBox(
        tr("Keep strokes as outline geometry (expand on import)"), filterGroup);
    m_normalizeBlackCheck = new QCheckBox(
        tr("Flatten kept geometry colors to solid black (#000000)"), filterGroup);
    m_whiteCutoutCheck = new QCheckBox(
        tr("Treat white foreground shapes (#ffffff) as cutouts / holes"), filterGroup);

    m_stripMetadataCheck->setChecked(true);
    m_removeHiddenCheck->setChecked(true);
    m_removeBgRectsCheck->setChecked(true);
    m_stripNoFillCheck->setChecked(true);
    m_keepStrokesCheck->setChecked(false);
    m_normalizeBlackCheck->setChecked(true);
    m_whiteCutoutCheck->setChecked(true);

    auto* tinyRow = new QHBoxLayout();
    m_stripTinyCheck = new QCheckBox(tr("Drop tiny noise geometry (< threshold):"), filterGroup);
    m_stripTinyCheck->setChecked(true);
    m_minExtentSpin = new QDoubleSpinBox(filterGroup);
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
        tr("Scale SVG height to font EM (%1 units)").arg(fontSummary.emSize), importGroup);
    m_correctDirCheck = new QCheckBox(tr("Correct contour direction (clockwise / CCW)"), importGroup);
    m_removeOverlapCheck = new QCheckBox(tr("Remove overlapping contours after import"), importGroup);
    m_simplifyCheck = new QCheckBox(tr("Simplify curves during import"), importGroup);
    m_roundToIntCheck = new QCheckBox(tr("Round coordinates to integer font units"), importGroup);
    m_updateWidthCheck = new QCheckBox(tr("Set advance width from SVG dimensions"), importGroup);

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

    optionsRow->addWidget(filterGroup, 1);
    optionsRow->addWidget(importGroup, 1);
    rootLayout->addLayout(optionsRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_importBtn = buttons->addButton(tr("Import into Glyph"), QDialogButtonBox::AcceptRole);
    m_importBtn->setEnabled(false);
    rootLayout->addWidget(buttons);

    connect(browseBtn, &QPushButton::clicked, this, &ImportSvgDialog::onBrowseClicked);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    for (QCheckBox* cb : {m_stripMetadataCheck, m_removeHiddenCheck, m_removeBgRectsCheck,
                          m_stripNoFillCheck, m_keepStrokesCheck, m_normalizeBlackCheck,
                          m_whiteCutoutCheck, m_stripTinyCheck}) {
        connect(cb, &QCheckBox::toggled, this, &ImportSvgDialog::refreshFilterPreview);
    }
    connect(m_minExtentSpin, &QDoubleSpinBox::valueChanged, this,
            &ImportSvgDialog::refreshFilterPreview);
    connect(m_stripTinyCheck, &QCheckBox::toggled, m_minExtentSpin, &QWidget::setEnabled);

    if (!initialSvgPath.isEmpty()) {
        loadSvgFile(initialSvgPath);
    }
}

void ImportSvgDialog::dragEnterEvent(QDragEnterEvent* event) {
    if (!extractDroppedSvgPath(event->mimeData()).isEmpty()) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void ImportSvgDialog::dropEvent(QDropEvent* event) {
    const QString svgPath = extractDroppedSvgPath(event->mimeData());
    if (!svgPath.isEmpty()) {
        event->acceptProposedAction();
        loadSvgFile(svgPath);
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
        this, tr("Select SVG File"), startDir, tr("SVG Vector Files (*.svg);;All Files (*)"));
    if (!path.isEmpty()) {
        loadSvgFile(path);
    }
}

void ImportSvgDialog::loadSvgFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_rawSvg.clear();
        m_originalPreview->setSvgData(QByteArray(), tr("Failed to open: %1").arg(file.errorString()));
        m_filteredPreview->setSvgData(QByteArray());
        m_importBtn->setEnabled(false);
        return;
    }

    saveLastSvgDirectory(path);
    m_pathEdit->setText(path);
    m_rawSvg = file.readAll();
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

void ImportSvgDialog::refreshFilterPreview() {
    if (m_rawSvg.isEmpty()) {
        m_filteredPreview->setSvgData(QByteArray());
        m_importBtn->setEnabled(false);
        return;
    }

    m_lastResult = SvgSanitizer::sanitize(m_rawSvg, currentFilterOptions());
    if (!m_lastResult.valid) {
        m_filteredPreview->setSvgData(QByteArray(), m_lastResult.errorMessage);
        m_statsLabel->setText(tr("<b>SVG Error:</b> %1").arg(m_lastResult.errorMessage.toHtmlEscaped()));
        m_importBtn->setEnabled(false);
        return;
    }

    if (m_lastResult.keptGeometryCount == 0) {
        m_filteredPreview->setSvgData(m_lastResult.cleanedSvg, tr("All geometry filtered out"));
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

    m_filteredPreview->setSvgData(m_lastResult.cleanedSvg);
    const QString vbStr = m_lastResult.viewBox.isValid()
        ? QStringLiteral("%1×%2").arg(m_lastResult.viewBox.width(), 0, 'f', 1)
                                 .arg(m_lastResult.viewBox.height(), 0, 'f', 1)
        : QStringLiteral("auto");
    const QString cutoutInfo = (m_lastResult.cutoutGeometryCount > 0)
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
    return opts;
}

} // namespace ffgui

