#include "ui/BatchImportDialog.hpp"
#include "model/BatchImportDefinition.hpp"
#include "model/FontSettingsUtils.hpp"
#include "ui/ImportFilterSettingsDialog.hpp"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace ffgui {

namespace {

bool parseCp(QString str, int32_t* outCp) {
    str = str.trimmed();
    QString s;
    if (str.startsWith(QStringLiteral("U+"), Qt::CaseInsensitive) ||
        str.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        s = str.mid(2);
    } else {
        s = str;
    }
    bool ok = false;
    uint32_t val = s.toUInt(&ok, 16);
    if (!ok || val > 0x10FFFFu) {
        return false;
    }
    *outCp = static_cast<int32_t>(val);
    return true;
}

} // namespace

BatchImportDialog::BatchImportDialog(const FontSummary& fontSummary,
                                     const QVector<GlyphSummary>& existingGlyphs,
                                     QWidget* parent)
    : QDialog(parent), m_fontSummary(fontSummary), m_existingGlyphs(existingGlyphs) {
    setWindowTitle(tr("Batch Import SVGs / PNGs into Font"));
    resize(820, 600);

    m_defaultOptions = loadDefaultImportOptions();

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    connect(m_debounceTimer, &QTimer::timeout, this, &BatchImportDialog::onStartUnicodeChanged);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);

    // Top action bar: Add files / directory / JSON / configure filter
    auto* topRow = new QHBoxLayout();
    auto* addFilesBtn = new QPushButton(tr("Add Files…"), this);
    auto* addDirBtn = new QPushButton(tr("Add Folder…"), this);
    auto* loadJsonBtn = new QPushButton(tr("Load JSON…"), this);
    auto* saveJsonBtn = new QPushButton(tr("Save JSON…"), this);
    auto* configureFilterBtn = new QPushButton(tr("Import Filter Parameters…"), this);
    configureFilterBtn->setToolTip(tr("Configure SVG/PNG cleaning, colors, cutouts, stroke expansion, and PNG binarization"));
    auto* clearBtn = new QPushButton(tr("Clear"), this);

    topRow->addWidget(addFilesBtn);
    topRow->addWidget(addDirBtn);
    topRow->addWidget(loadJsonBtn);
    topRow->addWidget(saveJsonBtn);
    topRow->addWidget(configureFilterBtn);
    topRow->addStretch(1);
    topRow->addWidget(clearBtn);
    root->addLayout(topRow);

    // Options group
    auto* optsGroup = new QGroupBox(tr("Batch Import Parameters"), this);
    auto* optsLayout = new QGridLayout(optsGroup);
    optsLayout->setContentsMargins(8, 6, 8, 6);

    optsLayout->addWidget(new QLabel(tr("Start Code Point:"), optsGroup), 0, 0);
    m_startUnicodeEdit = new QLineEdit(QStringLiteral("U+F0000"), optsGroup);
    m_startUnicodeEdit->setToolTip(tr("Consecutive Unicode code point assigned to the imported items"));
    optsLayout->addWidget(m_startUnicodeEdit, 0, 1);

    optsLayout->addWidget(new QLabel(tr("Placement:"), optsGroup), 0, 2);
    m_placementCombo = new QComboBox(optsGroup);
    m_placementCombo->addItem(tr("Auto (W/H ≤ 1.25 on Baseline, > 1.25 Centered)"),
                              static_cast<int>(GlyphPlacementMode::AutoAspectRatio));
    m_placementCombo->addItem(tr("Baseline (y=0) & Centered"),
                              static_cast<int>(GlyphPlacementMode::BaselineCentered));
    m_placementCombo->addItem(tr("Centered in Ascent"),
                              static_cast<int>(GlyphPlacementMode::CenterInAscent));
    m_placementCombo->addItem(tr("Centered in Full EM Box"),
                              static_cast<int>(GlyphPlacementMode::CenterInEm));
    m_placementCombo->addItem(tr("Original SVG Coordinates"),
                              static_cast<int>(GlyphPlacementMode::OriginalSvgCoords));
    optsLayout->addWidget(m_placementCombo, 0, 3);

    optsLayout->addWidget(new QLabel(tr("Scale Factor:"), optsGroup), 1, 0);
    m_scaleSpin = new QDoubleSpinBox(optsGroup);
    m_scaleSpin->setRange(10.0, 500.0);
    m_scaleSpin->setValue(m_defaultOptions.scaleFactor * 100.0);
    m_scaleSpin->setSuffix(tr(" %"));
    optsLayout->addWidget(m_scaleSpin, 1, 1);

    optsLayout->addWidget(new QLabel(tr("Side Padding:"), optsGroup), 1, 2);
    m_marginSpin = new QDoubleSpinBox(optsGroup);
    m_marginSpin->setRange(0.0, 40.0);
    m_marginSpin->setValue(m_defaultOptions.marginRatio * 100.0);
    m_marginSpin->setSuffix(tr(" %"));
    optsLayout->addWidget(m_marginSpin, 1, 3);

    m_overwriteCheck = new QCheckBox(tr("Overwrite existing glyphs with matching Unicode"), optsGroup);
    m_overwriteCheck->setChecked(false);
    optsLayout->addWidget(m_overwriteCheck, 2, 0, 1, 2);

    m_filterSummaryLabel = new QLabel(optsGroup);
    m_filterSummaryLabel->setStyleSheet(QStringLiteral("color: #666; font-size: 11px;"));
    optsLayout->addWidget(m_filterSummaryLabel, 2, 2, 1, 2);

    root->addWidget(optsGroup);

    // Table of items
    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({
        tr("File"), tr("Glyph Name"), tr("Assigned Unicode"), tr("Format"), tr("Status")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    root->addWidget(m_table, 1);

    m_statsLabel = new QLabel(tr("No files selected for batch import."), this);
    root->addWidget(m_statsLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_importBtn = buttons->addButton(tr("Start Batch Import"), QDialogButtonBox::AcceptRole);
    m_importBtn->setEnabled(false);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    // Wire slots
    connect(addFilesBtn, &QPushButton::clicked, this, &BatchImportDialog::onAddFilesClicked);
    connect(addDirBtn, &QPushButton::clicked, this, &BatchImportDialog::onAddDirectoryClicked);
    connect(loadJsonBtn, &QPushButton::clicked, this, &BatchImportDialog::onLoadJsonClicked);
    connect(saveJsonBtn, &QPushButton::clicked, this, &BatchImportDialog::onSaveJsonClicked);
    connect(configureFilterBtn, &QPushButton::clicked, this, &BatchImportDialog::onConfigureFilterClicked);
    connect(clearBtn, &QPushButton::clicked, this, &BatchImportDialog::onClearClicked);

    // Debounced text input
    connect(m_startUnicodeEdit, &QLineEdit::textChanged, this, &BatchImportDialog::onStartUnicodeTextChanged);
    connect(m_startUnicodeEdit, &QLineEdit::editingFinished, this, [this]() {
        if (m_debounceTimer->isActive()) {
            m_debounceTimer->stop();
            onStartUnicodeChanged();
        }
    });

    connect(m_overwriteCheck, &QCheckBox::toggled, this, &BatchImportDialog::onRecomputeRows);
    connect(m_placementCombo, &QComboBox::currentIndexChanged, this, &BatchImportDialog::onRecomputeRows);
    connect(m_scaleSpin, &QDoubleSpinBox::valueChanged, this, &BatchImportDialog::onRecomputeRows);
    connect(m_marginSpin, &QDoubleSpinBox::valueChanged, this, &BatchImportDialog::onRecomputeRows);

    syncControlsFromOptions();
}

void BatchImportDialog::syncControlsFromOptions() {
    const int idx = m_placementCombo->findData(static_cast<int>(m_defaultOptions.placementMode));
    if (idx >= 0) {
        m_placementCombo->setCurrentIndex(idx);
    }
    m_scaleSpin->setValue((m_defaultOptions.scaleFactor > 0.01 ? m_defaultOptions.scaleFactor : 1.0) * 100.0);
    m_marginSpin->setValue((m_defaultOptions.marginRatio >= 0.0 ? m_defaultOptions.marginRatio : 0.04) * 100.0);

    m_filterSummaryLabel->setText(
        tr("Filters: Black=%1, Cutouts=%2, Strokes=%3, PNG Threshold=%4")
            .arg(m_defaultOptions.normalizeToBlackFill ? tr("Yes") : tr("No"))
            .arg(m_defaultOptions.whiteAsCutout ? tr("Yes") : tr("No"))
            .arg(m_defaultOptions.keepStrokes ? tr("Yes") : tr("No"))
            .arg(m_defaultOptions.bitmapThreshold));
}

void BatchImportDialog::onConfigureFilterClicked() {
    // Sync current values from quick controls before opening full dialog
    m_defaultOptions.placementMode = static_cast<GlyphPlacementMode>(m_placementCombo->currentData().toInt());
    m_defaultOptions.scaleFactor = m_scaleSpin->value() / 100.0;
    m_defaultOptions.marginRatio = m_marginSpin->value() / 100.0;

    ImportFilterSettingsDialog dlg(m_defaultOptions, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_defaultOptions = dlg.importOptions();
        syncControlsFromOptions();
        onRecomputeRows();
    }
}

void BatchImportDialog::setInitialFiles(const QStringList& files) {
    addFilesToList(files);
}

void BatchImportDialog::setInitialDirectory(const QString& dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) {
        return;
    }
    const QStringList files = dir.entryList({QStringLiteral("*.svg"), QStringLiteral("*.png")},
                                            QDir::Files, QDir::Name);
    QStringList fullPaths;
    for (const QString& f : files) {
        fullPaths.append(dir.filePath(f));
    }
    addFilesToList(fullPaths);
}

void BatchImportDialog::setInitialDefinitionFile(const QString& jsonPath) {
    BatchImportJob job;
    QString err;
    if (BatchImportDefinition::loadFromFile(jsonPath, &job, &err)) {
        m_items = job.items;
        m_defaultOptions = job.defaultOptions;
        m_overwriteCheck->setChecked(job.overwriteExisting);
        syncControlsFromOptions();
        updateTable();
    } else {
        QMessageBox::warning(this, tr("Failed to Load JSON"), err);
    }
}

BatchImportJob BatchImportDialog::batchJob() const {
    BatchImportJob job;
    job.items = m_items;
    job.overwriteExisting = m_overwriteCheck->isChecked();

    SvgImportOptions defOpts = m_defaultOptions;
    defOpts.placementMode = static_cast<GlyphPlacementMode>(m_placementCombo->currentData().toInt());
    defOpts.scaleFactor = m_scaleSpin->value() / 100.0;
    defOpts.marginRatio = m_marginSpin->value() / 100.0;
    job.defaultOptions = defOpts;

    return job;
}

void BatchImportDialog::onAddFilesClicked() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Select SVG or PNG Files"), QString(),
        tr("Images (*.svg *.png);;SVG (*.svg);;PNG (*.png);;All Files (*)"));
    if (!files.isEmpty()) {
        addFilesToList(files);
    }
}

void BatchImportDialog::onAddDirectoryClicked() {
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select Folder with SVG / PNG Files"));
    if (!dir.isEmpty()) {
        setInitialDirectory(dir);
    }
}

void BatchImportDialog::onLoadJsonClicked() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Batch Import Definition"), QString(), tr("JSON Files (*.json);;All Files (*)"));
    if (!path.isEmpty()) {
        setInitialDefinitionFile(path);
    }
}

void BatchImportDialog::onSaveJsonClicked() {
    if (m_items.isEmpty()) {
        QMessageBox::information(this, tr("Save JSON"), tr("No items to save. Add files first."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Batch Import Definition"), QStringLiteral("import_def.json"),
        tr("JSON Files (*.json);;All Files (*)"));
    if (!path.isEmpty()) {
        QString err;
        if (!BatchImportDefinition::saveToFile(path, batchJob(), &err)) {
            QMessageBox::warning(this, tr("Save Failed"), err);
        }
    }
}

void BatchImportDialog::onClearClicked() {
    m_items.clear();
    updateTable();
}

void BatchImportDialog::onStartUnicodeTextChanged() {
    m_importBtn->setEnabled(false);
    m_statsLabel->setText(tr("Evaluating code points…"));
    m_debounceTimer->start(300);
}

void BatchImportDialog::onStartUnicodeChanged() {
    int32_t cp = -1;
    if (parseCp(m_startUnicodeEdit->text(), &cp)) {
        m_parsedStartCp = cp;
    } else {
        m_parsedStartCp = -1;
    }
    onRecomputeRows();
}

void BatchImportDialog::onRecomputeRows() {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_parsedStartCp >= 0) {
            m_items[i].unicode = m_parsedStartCp + i;
        }
    }
    updateTable();
}

void BatchImportDialog::addFilesToList(const QStringList& files) {
    for (const QString& f : files) {
        const QFileInfo fi(f);
        if (!fi.exists() || !fi.isFile()) {
            continue;
        }
        BatchImportItem item;
        item.filePath = f;
        item.glyphName = fi.completeBaseName().replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")),
                                                       QStringLiteral("_"));
        m_items.append(std::move(item));
    }
    onRecomputeRows();
}

void BatchImportDialog::updateTable() {
    const int count = m_items.size();

    m_table->setUpdatesEnabled(false);
    if (m_table->rowCount() != count) {
        m_table->setRowCount(count);
    }

    if (count == 0) {
        m_table->setUpdatesEnabled(true);
        m_statsLabel->setText(tr("No files selected for batch import."));
        m_importBtn->setEnabled(false);
        return;
    }

    if (m_parsedStartCp < 0) {
        m_table->setUpdatesEnabled(true);
        m_statsLabel->setText(tr("<font color='#d9534f'>Invalid start code point entered.</font>"));
        m_importBtn->setEnabled(false);
        return;
    }

    int conflictCount = 0;
    for (int i = 0; i < count; ++i) {
        const BatchImportItem& item = m_items[i];
        const QFileInfo fi(item.filePath);

        auto setOrUpdateItem = [this, i](int col, const QString& text, const QColor* fg = nullptr) {
            QTableWidgetItem* it = m_table->item(i, col);
            if (!it) {
                it = new QTableWidgetItem(text);
                if (fg) it->setForeground(*fg);
                m_table->setItem(i, col, it);
            } else {
                it->setText(text);
                if (fg) it->setForeground(*fg);
            }
        };

        setOrUpdateItem(0, fi.fileName());
        setOrUpdateItem(1, item.glyphName);

        const QString uniStr = (item.unicode >= 0)
                                   ? QStringLiteral("U+%1").arg(item.unicode, 4, 16, QLatin1Char('0')).toUpper()
                                   : tr("None");
        setOrUpdateItem(2, uniStr);
        setOrUpdateItem(3, fi.suffix().toUpper());

        QString statusText = tr("New");
        QColor statusColor(40, 167, 69); // Green
        for (const auto& existing : m_existingGlyphs) {
            if (existing.unicode == item.unicode) {
                ++conflictCount;
                statusText = m_overwriteCheck->isChecked() ? tr("Overwrite (#%1)").arg(existing.gid)
                                                           : tr("Exists (#%1)").arg(existing.gid);
                statusColor = m_overwriteCheck->isChecked() ? QColor(245, 158, 11) : QColor(220, 53, 69);
                break;
            }
        }
        setOrUpdateItem(4, statusText, &statusColor);
    }

    m_table->setUpdatesEnabled(true);

    if (conflictCount > 0 && !m_overwriteCheck->isChecked()) {
        m_statsLabel->setText(
            tr("<font color='#d9534f'><b>Warning:</b> %1 code points already exist in font. Check 'Overwrite' or change start code point.</font>")
                .arg(conflictCount));
        m_importBtn->setEnabled(false);
    } else {
        m_statsLabel->setText(
            tr("<font color='#10b981'>Ready to import %1 file(s)%2.</font>")
                .arg(count)
                .arg(conflictCount > 0 ? tr(" (%1 will overwrite existing)").arg(conflictCount) : QString()));
        m_importBtn->setEnabled(true);
    }
}

} // namespace ffgui
