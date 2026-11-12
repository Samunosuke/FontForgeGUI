#include "ui/ExportFontDialog.hpp"
#include "model/FontSettingsUtils.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace ffgui {

ExportFontDialog::ExportFontDialog(const FontSummary& summary, bool saveAsMode, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(saveAsMode ? tr("Save Font As") : tr("Export Font"));
    resize(520, 320);

    m_formats = {
        {tr("OpenType CFF (.otf)"), QStringLiteral(".otf")},
        {tr("TrueType (.ttf)"), QStringLiteral(".ttf")},
        {tr("WOFF2 Web Font (.woff2)"), QStringLiteral(".woff2")},
        {tr("WOFF Web Font (.woff)"), QStringLiteral(".woff")},
        {tr("FontForge Project (.sfd)"), QStringLiteral(".sfd")},
        {tr("UFO v3 (.ufo)"), QStringLiteral(".ufo")},
        {tr("PostScript Type1 Binary (.pfb)"), QStringLiteral(".pfb")},
        {tr("SVG Font (.svg)"), QStringLiteral(".svg")},
    };

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    m_formatCombo = new QComboBox(this);
    int defaultFormatIndex = 1; // Default to TTF (.ttf)
    const QString srcSuffix = QFileInfo(summary.filePath).suffix().toLower();
    for (int i = 0; i < m_formats.size(); ++i) {
        m_formatCombo->addItem(m_formats[i].label);
        if (!srcSuffix.isEmpty() && m_formats[i].extension.endsWith(srcSuffix, Qt::CaseInsensitive)) {
            defaultFormatIndex = i;
        }
    }
    m_formatCombo->setCurrentIndex(defaultFormatIndex);
    form->addRow(tr("Format:"), m_formatCombo);

    auto* pathRow = new QHBoxLayout();
    m_pathEdit = new QLineEdit(this);
    auto* browseBtn = new QPushButton(tr("Browse…"), this);
    pathRow->addWidget(m_pathEdit, 1);
    pathRow->addWidget(browseBtn);
    form->addRow(tr("Output File:"), pathRow);

    layout->addLayout(form);

    auto* optsGroup = new QGroupBox(tr("Export Options"), this);
    auto* optsLayout = new QVBoxLayout(optsGroup);
    m_roundCheck = new QCheckBox(tr("Round coordinates to integer"), optsGroup);
    m_roundCheck->setChecked(true);
    m_opentypeCheck = new QCheckBox(tr("Generate OpenType tables (GPOS/GSUB)"), optsGroup);
    m_opentypeCheck->setChecked(true);
    m_dummyDsigCheck = new QCheckBox(tr("Generate dummy DSIG table (SFNT)"), optsGroup);
    m_dummyDsigCheck->setChecked(true);
    m_omitHintsCheck = new QCheckBox(tr("Omit hinting / instructions"), optsGroup);
    m_afmCheck = new QCheckBox(tr("Generate AFM file (PostScript formats)"), optsGroup);

    optsLayout->addWidget(m_roundCheck);
    optsLayout->addWidget(m_opentypeCheck);
    optsLayout->addWidget(m_dummyDsigCheck);
    optsLayout->addWidget(m_omitHintsCheck);
    optsLayout->addWidget(m_afmCheck);
    layout->addWidget(optsGroup);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(saveAsMode ? tr("Save") : tr("Export"));
    layout->addWidget(buttons);

    // Default output path: alongside source font or last used directory
    QFileInfo srcInfo(summary.filePath);
    const QString baseName =
        !summary.fontName.isEmpty() ? summary.fontName
        : (!srcInfo.completeBaseName().isEmpty() ? srcInfo.completeBaseName()
                                                : QStringLiteral("UntitledFont"));

    QString dir = srcInfo.absolutePath();
    if (summary.filePath.isEmpty() || dir.isEmpty() || dir == QStringLiteral(".")) {
        dir = loadLastFontDirectory();
        if (dir.isEmpty() || !QDir(dir).exists()) {
            dir = QDir::homePath();
        }
    }
    m_pathEdit->setText(dir + QLatin1Char('/') + baseName + m_formats[defaultFormatIndex].extension);

    connect(m_formatCombo, &QComboBox::currentIndexChanged, this,
            &ExportFontDialog::onFormatChanged);
    connect(browseBtn, &QPushButton::clicked, this, &ExportFontDialog::onBrowseClicked);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        const QString out = m_pathEdit->text().trimmed();
        if (out.isEmpty()) {
            QMessageBox::warning(this, windowTitle(), tr("Please specify an output file path."));
            return;
        }
        saveLastFontDirectory(out);
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

ExportOptions ExportFontDialog::exportOptions() const {
    ExportOptions opts;
    opts.outputPath = m_pathEdit->text().trimmed();
    opts.roundToInt = m_roundCheck->isChecked();
    opts.opentypeTables = m_opentypeCheck->isChecked();
    opts.dummyDsig = m_dummyDsigCheck->isChecked();
    opts.omitHints = m_omitHintsCheck->isChecked();
    opts.generateAfm = m_afmCheck->isChecked();
    return opts;
}

void ExportFontDialog::onFormatChanged(int index) {
    if (index < 0 || index >= m_formats.size()) {
        return;
    }
    const QString ext = m_formats[index].extension;
    const QString current = m_pathEdit->text().trimmed();
    if (current.isEmpty()) {
        return;
    }
    const QFileInfo fi(current);
    m_pathEdit->setText(fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + ext);
}

void ExportFontDialog::onBrowseClicked() {
    const int idx = m_formatCombo->currentIndex();
    const QString ext = (idx >= 0 && idx < m_formats.size()) ? m_formats[idx].extension
                                                             : QStringLiteral(".otf");
    const QString filter = QStringLiteral("Font (*%1);;All Files (*)").arg(ext);
    const QString chosen =
        QFileDialog::getSaveFileName(this, windowTitle(), m_pathEdit->text(), filter);
    if (!chosen.isEmpty()) {
        m_pathEdit->setText(chosen);
        saveLastFontDirectory(chosen);
    }
}

} // namespace ffgui

