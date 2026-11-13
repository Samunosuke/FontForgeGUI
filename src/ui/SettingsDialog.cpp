#include "ui/SettingsDialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace ffgui {

SettingsDialog::SettingsDialog(ToolbarDisplayMode currentMode, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Settings"));
    setMinimumWidth(420);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(12);

    auto* toolbarGroup = new QGroupBox(tr("Main Toolbar"), this);
    auto* toolbarForm = new QFormLayout(toolbarGroup);
    toolbarForm->setLabelAlignment(Qt::AlignRight);

    m_toolbarStyleCombo = new QComboBox(toolbarGroup);
    m_toolbarStyleCombo->addItem(tr("Icons and Text (Beside)"),
                                 static_cast<int>(ToolbarDisplayMode::IconAndTextBeside));
    m_toolbarStyleCombo->addItem(tr("Icons and Text (Under)"),
                                 static_cast<int>(ToolbarDisplayMode::IconAndTextUnder));
    m_toolbarStyleCombo->addItem(tr("Icons Only"),
                                 static_cast<int>(ToolbarDisplayMode::IconOnly));
    m_toolbarStyleCombo->addItem(tr("Text Only"),
                                 static_cast<int>(ToolbarDisplayMode::TextOnly));

    const int idx = m_toolbarStyleCombo->findData(static_cast<int>(currentMode));
    if (idx >= 0) {
        m_toolbarStyleCombo->setCurrentIndex(idx);
    }

    toolbarForm->addRow(tr("Button Display:"), m_toolbarStyleCombo);
    auto* toolbarHint = new QLabel(
        tr("Controls how actions appear in the top toolbar. Hovering any button always shows its full name and shortcut."),
        toolbarGroup);
    toolbarHint->setWordWrap(true);
    toolbarHint->setStyleSheet(QStringLiteral("color: #6b7280; font-size: 11px;"));
    toolbarForm->addRow(QString(), toolbarHint);
    root->addWidget(toolbarGroup);

    auto* importGroup = new QGroupBox(tr("SVG / PNG Import Defaults"), this);
    auto* importLayout = new QVBoxLayout(importGroup);
    auto* importDesc = new QLabel(
        tr("Configure default scaling, placement, color filtering, and PNG tracing parameters used for new imports."),
        importGroup);
    importDesc->setWordWrap(true);
    importDesc->setStyleSheet(QStringLiteral("color: #6b7280; font-size: 11px;"));
    importLayout->addWidget(importDesc);

    auto* openImportPrefsBtn = new QPushButton(tr("Configure Default Import Preferences…"), importGroup);
    connect(openImportPrefsBtn, &QPushButton::clicked, this,
            &SettingsDialog::openImportPreferencesRequested);
    importLayout->addWidget(openImportPrefsBtn, 0, Qt::AlignLeft);
    root->addWidget(importGroup);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    connect(m_toolbarStyleCombo, &QComboBox::currentIndexChanged, this, [this]() {
        emit toolbarDisplayModePreview(toolbarDisplayMode());
    });
}

ToolbarDisplayMode SettingsDialog::toolbarDisplayMode() const {
    return static_cast<ToolbarDisplayMode>(m_toolbarStyleCombo->currentData().toInt());
}

} // namespace ffgui

