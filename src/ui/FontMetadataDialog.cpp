#include "ui/FontMetadataDialog.hpp"

#include "backend/FontForgeBridge.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace ffgui {

FontMetadataDialog::FontMetadataDialog(const FontSummary& summary, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Font Metadata"));
    resize(640, 520);

    auto* rootLayout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);

    // Tab 1: General & Metrics
    auto* generalTab = new QWidget(tabs);
    auto* form = new QFormLayout(generalTab);
    form->setLabelAlignment(Qt::AlignRight);

    m_fontNameEdit = new QLineEdit(summary.fontName, generalTab);
    m_familyNameEdit = new QLineEdit(summary.familyName, generalTab);
    m_fullNameEdit = new QLineEdit(summary.fullName, generalTab);
    m_weightEdit = new QLineEdit(summary.weight, generalTab);
    m_versionEdit = new QLineEdit(summary.version, generalTab);

    m_copyrightEdit = new QPlainTextEdit(summary.copyright, generalTab);
    m_copyrightEdit->setMaximumHeight(72);

    m_ascentSpin = new QSpinBox(generalTab);
    m_ascentSpin->setRange(0, 32768);
    m_ascentSpin->setValue(summary.ascent);

    m_descentSpin = new QSpinBox(generalTab);
    m_descentSpin->setRange(0, 32768);
    m_descentSpin->setValue(summary.descent);

    m_emSizeLabel = new QLabel(generalTab);

    m_italicAngleSpin = new QDoubleSpinBox(generalTab);
    m_italicAngleSpin->setRange(-89.0, 89.0);
    m_italicAngleSpin->setDecimals(2);
    m_italicAngleSpin->setSingleStep(0.5);
    m_italicAngleSpin->setValue(summary.italicAngle);

    m_underlinePosSpin = new QDoubleSpinBox(generalTab);
    m_underlinePosSpin->setRange(-16384.0, 16384.0);
    m_underlinePosSpin->setDecimals(1);
    m_underlinePosSpin->setValue(summary.underlinePos);

    m_underlineWidthSpin = new QDoubleSpinBox(generalTab);
    m_underlineWidthSpin->setRange(0.0, 16384.0);
    m_underlineWidthSpin->setDecimals(1);
    m_underlineWidthSpin->setValue(summary.underlineWidth);

    form->addRow(tr("PostScript Font Name:"), m_fontNameEdit);
    form->addRow(tr("Family Name:"), m_familyNameEdit);
    form->addRow(tr("Full Name:"), m_fullNameEdit);
    form->addRow(tr("Weight:"), m_weightEdit);
    form->addRow(tr("Version:"), m_versionEdit);
    form->addRow(tr("Copyright:"), m_copyrightEdit);
    form->addRow(tr("Ascent:"), m_ascentSpin);
    form->addRow(tr("Descent:"), m_descentSpin);
    form->addRow(tr("EM Size:"), m_emSizeLabel);
    form->addRow(tr("Italic Angle:"), m_italicAngleSpin);
    form->addRow(tr("Underline Position:"), m_underlinePosSpin);
    form->addRow(tr("Underline Thickness:"), m_underlineWidthSpin);
    tabs->addTab(generalTab, tr("General && Metrics"));

    // Tab 2: SFNT 'name' Table
    auto* sfntTab = new QWidget(tabs);
    auto* sfntLayout = new QVBoxLayout(sfntTab);

    m_sfntTable = new QTableWidget(0, 3, sfntTab);
    m_sfntTable->setHorizontalHeaderLabels({tr("Language"), tr("Name ID"), tr("Value")});
    m_sfntTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_sfntTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_sfntTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_sfntTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sfntTable->setSelectionMode(QAbstractItemView::SingleSelection);
    sfntLayout->addWidget(m_sfntTable, 1);

    for (const SfntNameEntry& entry : summary.sfntNames) {
        appendSfntRow(entry.langId, entry.stringId, entry.value);
    }

    auto* sfntBtns = new QHBoxLayout();
    auto* addBtn = new QPushButton(tr("Add Entry"), sfntTab);
    auto* removeBtn = new QPushButton(tr("Remove Selected"), sfntTab);
    sfntBtns->addWidget(addBtn);
    sfntBtns->addWidget(removeBtn);
    sfntBtns->addStretch(1);
    sfntLayout->addLayout(sfntBtns);
    tabs->addTab(sfntTab, tr("SFNT 'name' Table"));

    rootLayout->addWidget(tabs, 1);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Apply"));
    rootLayout->addWidget(buttons);

    onEmMetricsChanged();

    connect(m_ascentSpin, &QSpinBox::valueChanged, this, &FontMetadataDialog::onEmMetricsChanged);
    connect(m_descentSpin, &QSpinBox::valueChanged, this, &FontMetadataDialog::onEmMetricsChanged);
    connect(addBtn, &QPushButton::clicked, this, &FontMetadataDialog::onAddSfntRow);
    connect(removeBtn, &QPushButton::clicked, this, &FontMetadataDialog::onRemoveSfntRow);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (m_fontNameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Font Metadata"),
                                 tr("PostScript Font Name cannot be empty."));
            return;
        }
        if (m_ascentSpin->value() + m_descentSpin->value() <= 0) {
            QMessageBox::warning(this, tr("Font Metadata"),
                                 tr("Ascent + Descent must be greater than 0."));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void FontMetadataDialog::appendSfntRow(int langId, int stringId, const QString& value) {
    const int row = m_sfntTable->rowCount();
    m_sfntTable->insertRow(row);

    auto* langCombo = new QComboBox(m_sfntTable);
    const auto langs = FontForgeBridge::commonSfntLanguages();
    int selectedLangIdx = -1;
    for (int i = 0; i < langs.size(); ++i) {
        langCombo->addItem(
            QStringLiteral("%1 (0x%2)")
                .arg(langs[i].second)
                .arg(langs[i].first, 4, 16, QLatin1Char('0')).toUpper(),
            langs[i].first);
        if (langs[i].first == langId) {
            selectedLangIdx = i;
        }
    }
    if (selectedLangIdx < 0) {
        langCombo->addItem(QStringLiteral("0x%1").arg(langId, 4, 16, QLatin1Char('0')).toUpper(),
                           langId);
        selectedLangIdx = langCombo->count() - 1;
    }
    langCombo->setCurrentIndex(selectedLangIdx);

    auto* idCombo = new QComboBox(m_sfntTable);
    const QStringList ids = FontForgeBridge::availableSfntStringIds();
    for (int i = 0; i < ids.size(); ++i) {
        idCombo->addItem(ids[i], i);
    }
    if (stringId >= 0 && stringId < idCombo->count()) {
        idCombo->setCurrentIndex(stringId);
    }

    auto* valItem = new QTableWidgetItem(value);
    m_sfntTable->setCellWidget(row, 0, langCombo);
    m_sfntTable->setCellWidget(row, 1, idCombo);
    m_sfntTable->setItem(row, 2, valItem);
}

void FontMetadataDialog::onEmMetricsChanged() {
    const int em = m_ascentSpin->value() + m_descentSpin->value();
    m_emSizeLabel->setText(QString::number(em));
}

void FontMetadataDialog::onAddSfntRow() {
    appendSfntRow(0x0409, 0, QString());
    const int row = m_sfntTable->rowCount() - 1;
    m_sfntTable->setCurrentCell(row, 2);
    m_sfntTable->editItem(m_sfntTable->item(row, 2));
}

void FontMetadataDialog::onRemoveSfntRow() {
    const int row = m_sfntTable->currentRow();
    if (row >= 0) {
        m_sfntTable->removeRow(row);
    }
}

FontMetadataUpdate FontMetadataDialog::metadataUpdate() const {
    FontMetadataUpdate update;
    update.fontName = m_fontNameEdit->text().trimmed();
    update.familyName = m_familyNameEdit->text().trimmed();
    update.fullName = m_fullNameEdit->text().trimmed();
    update.weight = m_weightEdit->text().trimmed();
    update.version = m_versionEdit->text().trimmed();
    update.copyright = m_copyrightEdit->toPlainText();
    update.ascent = m_ascentSpin->value();
    update.descent = m_descentSpin->value();
    update.italicAngle = m_italicAngleSpin->value();
    update.underlinePos = m_underlinePosSpin->value();
    update.underlineWidth = m_underlineWidthSpin->value();
    update.updateSfntNames = true;

    for (int row = 0; row < m_sfntTable->rowCount(); ++row) {
        auto* langCombo = qobject_cast<QComboBox*>(m_sfntTable->cellWidget(row, 0));
        auto* idCombo = qobject_cast<QComboBox*>(m_sfntTable->cellWidget(row, 1));
        QTableWidgetItem* valItem = m_sfntTable->item(row, 2);
        if (!langCombo || !idCombo || !valItem) {
            continue;
        }
        const QString val = valItem->text().trimmed();
        if (val.isEmpty()) {
            continue;
        }
        SfntNameEntry entry;
        entry.langId = langCombo->currentData().toInt();
        entry.stringId = idCombo->currentData().toInt();
        entry.value = val;
        update.sfntNames.append(entry);
    }
    return update;
}

} // namespace ffgui

