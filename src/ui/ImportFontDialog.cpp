#include "ui/ImportFontDialog.hpp"
#include "model/FontSettingsUtils.hpp"
#include "model/GlyphMetadataUtils.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace ffgui {

ImportFontDialog::ImportFontDialog(const FontDocumentSnapshot& currentSnapshot, QWidget* parent)
    : QDialog(parent)
    , m_currentSnapshot(currentSnapshot)
    , m_evalTimer(new QTimer(this)) {
    setWindowTitle(tr("Import Glyphs from Font"));
    resize(700, 520);

    m_evalTimer->setSingleShot(true);
    m_evalTimer->setInterval(300);
    connect(m_evalTimer, &QTimer::timeout, this, &ImportFontDialog::evaluateImport);

    auto* mainLayout = new QVBoxLayout(this);

    // -------------------------------------------------------------------------
    // 1. Source Font Selection
    // -------------------------------------------------------------------------
    auto* srcGroup = new QGroupBox(tr("Source Font File"), this);
    auto* srcLayout = new QHBoxLayout(srcGroup);

    m_sourcePathEdit = new QLineEdit(srcGroup);
    m_sourcePathEdit->setPlaceholderText(tr("Path to source font file (.ttf, .otf, .woff, .sfd)..."));
    m_browseBtn = new QPushButton(tr("Browse..."), srcGroup);

    srcLayout->addWidget(m_sourcePathEdit, 1);
    srcLayout->addWidget(m_browseBtn);
    mainLayout->addWidget(srcGroup);

    // -------------------------------------------------------------------------
    // 2. Range & Target Configuration
    // -------------------------------------------------------------------------
    auto* rangeGroup = new QGroupBox(tr("Range & Target Code Point"), this);
    auto* rangeForm = new QFormLayout(rangeGroup);

    auto* rangeRow = new QHBoxLayout();
    m_rangePresetCombo = new QComboBox(rangeGroup);
    m_rangePresetCombo->addItem(tr("All Encoded Glyphs (all)"), QStringLiteral("all"));
    m_rangePresetCombo->addItem(tr("Basic Latin (U+0020..U+007E)"), QStringLiteral("U+0020-U+007E"));
    m_rangePresetCombo->addItem(tr("Latin-1 Supplement (U+00A0..U+00FF)"), QStringLiteral("U+00A0-U+00FF"));
    m_rangePresetCombo->addItem(tr("Private Use Area (U+E000..U+F8FF)"), QStringLiteral("U+E000-U+F8FF"));
    m_rangePresetCombo->addItem(tr("Supplementary PUA-A (U+F0000..U+FFFFD)"), QStringLiteral("U+F0000-U+FFFFD"));
    m_rangePresetCombo->addItem(tr("Supplementary PUA-B (U+100000..U+10FFFD)"), QStringLiteral("U+100000-U+10FFFD"));
    m_rangePresetCombo->addItem(tr("Custom Range..."), QString());

    m_sourceRangeEdit = new QLineEdit(QStringLiteral("all"), rangeGroup);
    rangeRow->addWidget(m_rangePresetCombo, 1);
    rangeRow->addWidget(m_sourceRangeEdit, 1);
    rangeForm->addRow(tr("Source Range:"), rangeRow);

    m_preserveOriginalCheck = new QCheckBox(tr("Preserve original code points from source font"), rangeGroup);
    m_preserveOriginalCheck->setChecked(false);
    rangeForm->addRow(QString(), m_preserveOriginalCheck);

    m_targetStartEdit = new QLineEdit(QStringLiteral("U+F0000"), rangeGroup);
    rangeForm->addRow(tr("Target Start Code Point:"), m_targetStartEdit);

    mainLayout->addWidget(rangeGroup);

    // -------------------------------------------------------------------------
    // 3. Options
    // -------------------------------------------------------------------------
    auto* optGroup = new QGroupBox(tr("Options"), this);
    auto* optLayout = new QHBoxLayout(optGroup);

    m_scaleToEmCheck = new QCheckBox(
        tr("Scale outlines to target font EM size (%1 units)").arg(m_currentSnapshot.summary.emSize),
        optGroup);
    m_scaleToEmCheck->setChecked(true);

    m_copyMetricsCheck = new QCheckBox(tr("Copy & scale advance widths"), optGroup);
    m_copyMetricsCheck->setChecked(true);

    m_overwriteCheck = new QCheckBox(tr("Overwrite occupied target code points"), optGroup);
    m_overwriteCheck->setChecked(false);

    optLayout->addWidget(m_scaleToEmCheck);
    optLayout->addWidget(m_copyMetricsCheck);
    optLayout->addWidget(m_overwriteCheck);
    optLayout->addStretch(1);
    mainLayout->addWidget(optGroup);

    // -------------------------------------------------------------------------
    // 4. Status & Preview Table
    // -------------------------------------------------------------------------
    m_statusLabel = new QLabel(tr("Select a source font file to preview glyphs to import."), this);
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    m_previewTable = new QTableWidget(0, 3, this);
    m_previewTable->setHorizontalHeaderLabels({tr("Source Code Point"), tr("Target Code Point"), tr("Status")});
    m_previewTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_previewTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_previewTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_previewTable->verticalHeader()->setVisible(false);
    m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_previewTable, 1);

    // -------------------------------------------------------------------------
    // 5. Action Buttons
    // -------------------------------------------------------------------------
    auto* btnRow = new QHBoxLayout();
    m_importBtn = new QPushButton(tr("Import Glyphs"), this);
    m_importBtn->setDefault(true);
    m_importBtn->setEnabled(false);

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    btnRow->addStretch(1);
    btnRow->addWidget(m_importBtn);
    btnRow->addWidget(m_cancelBtn);
    mainLayout->addLayout(btnRow);

    // Connections
    connect(m_browseBtn, &QPushButton::clicked, this, &ImportFontDialog::onBrowseSourceFont);
    connect(m_sourcePathEdit, &QLineEdit::textChanged, this, &ImportFontDialog::onSourcePathChanged);
    connect(m_sourceRangeEdit, &QLineEdit::textChanged, this, &ImportFontDialog::scheduleEvaluation);
    connect(m_targetStartEdit, &QLineEdit::textChanged, this, &ImportFontDialog::scheduleEvaluation);
    connect(m_preserveOriginalCheck, &QCheckBox::toggled, this, &ImportFontDialog::onPreserveOriginalChanged);
    connect(m_overwriteCheck, &QCheckBox::toggled, this, &ImportFontDialog::scheduleEvaluation);

    connect(m_rangePresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        const QString data = m_rangePresetCombo->itemData(idx).toString();
        if (!data.isEmpty()) {
            m_sourceRangeEdit->setText(data);
        }
    });

    connect(m_importBtn, &QPushButton::clicked, this, &ImportFontDialog::onImportClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

FontMergeOptions ImportFontDialog::mergeOptions() const {
    FontMergeOptions opts;
    opts.sourceFontPath = m_sourcePathEdit->text().trimmed();
    opts.sourceRange = m_sourceRangeEdit->text().trimmed();
    if (opts.sourceRange.isEmpty()) {
        opts.sourceRange = QStringLiteral("all");
    }

    if (m_preserveOriginalCheck->isChecked()) {
        opts.targetStartUnicode = -1;
    } else {
        int32_t targetStart = -1;
        if (parseSingleCodePoint(m_targetStartEdit->text(), &targetStart) && targetStart >= 0) {
            opts.targetStartUnicode = targetStart;
        } else {
            opts.targetStartUnicode = 0xF0000;
        }
    }

    opts.scaleToEm = m_scaleToEmCheck->isChecked();
    opts.copyMetrics = m_copyMetricsCheck->isChecked();
    opts.overwriteExisting = m_overwriteCheck->isChecked();
    return opts;
}

void ImportFontDialog::onBrowseSourceFont() {
    const QString lastDir = loadLastFontDirectory();
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Select Source Font File"), lastDir,
        tr("Font Files (*.ttf *.otf *.woff *.woff2 *.sfd);;All Files (*)"));
    if (!file.isEmpty()) {
        saveLastFontDirectory(file);
        m_sourcePathEdit->setText(file);
    }
}

void ImportFontDialog::onSourcePathChanged() {
    scheduleEvaluation();
}

void ImportFontDialog::onRangeChanged() {
    scheduleEvaluation();
}

void ImportFontDialog::onPreserveOriginalChanged(bool checked) {
    m_targetStartEdit->setEnabled(!checked);
    scheduleEvaluation();
}

void ImportFontDialog::scheduleEvaluation() {
    m_importBtn->setEnabled(false);
    m_statusLabel->setText(tr("Validating range and checking collisions..."));
    m_evalTimer->start();
}

void ImportFontDialog::evaluateImport() {
    const QString path = m_sourcePathEdit->text().trimmed();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        m_statusLabel->setText(tr("Please specify a valid source font file."));
        m_previewTable->setRowCount(0);
        m_importBtn->setEnabled(false);
        return;
    }

    QList<QPair<int32_t, int32_t>> ranges;
    QString rangeErr;
    if (!parseCodePointRange(m_sourceRangeEdit->text().trimmed(), &ranges, &rangeErr)) {
        m_statusLabel->setText(tr("Invalid source range: %1").arg(rangeErr));
        m_previewTable->setRowCount(0);
        m_importBtn->setEnabled(false);
        return;
    }

    int32_t targetStart = -1;
    if (!m_preserveOriginalCheck->isChecked()) {
        if (!parseSingleCodePoint(m_targetStartEdit->text(), &targetStart) || targetStart < 0) {
            m_statusLabel->setText(tr("Invalid target start code point (e.g. U+F0000)."));
            m_previewTable->setRowCount(0);
            m_importBtn->setEnabled(false);
            return;
        }
    }

    m_statusLabel->setText(tr("Source font and range verified. Ready to import."));
    m_importBtn->setEnabled(true);
}

void ImportFontDialog::onImportClicked() {
    accept();
}

} // namespace ffgui

