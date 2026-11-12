#include "ui/NewFontDialog.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace ffgui {

NewFontDialog::NewFontDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Create New Font"));
    setMinimumWidth(420);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(8);

    m_fontNameEdit = new QLineEdit(QStringLiteral("MyCustomFont"), this);
    m_familyNameEdit = new QLineEdit(QStringLiteral("MyCustomFont"), this);
    m_fullNameEdit = new QLineEdit(QStringLiteral("MyCustomFont Regular"), this);
    m_weightEdit = new QLineEdit(QStringLiteral("Regular"), this);
    m_versionEdit = new QLineEdit(QStringLiteral("1.0"), this);
    m_copyrightEdit = new QLineEdit(this);
    m_copyrightEdit->setPlaceholderText(tr("Optional copyright / license notice"));

    m_ascentSpin = new QSpinBox(this);
    m_ascentSpin->setRange(100, 4000);
    m_ascentSpin->setValue(800);
    m_ascentSpin->setSuffix(tr(" units"));

    m_descentSpin = new QSpinBox(this);
    m_descentSpin->setRange(0, 2000);
    m_descentSpin->setValue(200);
    m_descentSpin->setSuffix(tr(" units"));

    m_emSizeSpin = new QSpinBox(this);
    m_emSizeSpin->setRange(100, 6000);
    m_emSizeSpin->setValue(1000);
    m_emSizeSpin->setSuffix(tr(" units"));
    m_emSizeSpin->setReadOnly(true);

    auto updateEm = [this]() {
        m_emSizeSpin->setValue(m_ascentSpin->value() + m_descentSpin->value());
    };
    connect(m_ascentSpin, &QSpinBox::valueChanged, this, updateEm);
    connect(m_descentSpin, &QSpinBox::valueChanged, this, updateEm);

    form->addRow(tr("PostScript Name:"), m_fontNameEdit);
    form->addRow(tr("Family Name:"), m_familyNameEdit);
    form->addRow(tr("Full Name:"), m_fullNameEdit);
    form->addRow(tr("Weight:"), m_weightEdit);
    form->addRow(tr("Version:"), m_versionEdit);
    form->addRow(tr("Copyright:"), m_copyrightEdit);
    form->addRow(tr("Ascent:"), m_ascentSpin);
    form->addRow(tr("Descent:"), m_descentSpin);
    form->addRow(tr("EM Size (Ascent + Descent):"), m_emSizeSpin);

    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

NewFontOptions NewFontDialog::fontOptions() const {
    NewFontOptions opts;
    opts.fontName = m_fontNameEdit->text().trimmed().isEmpty() ? QStringLiteral("Untitled")
                                                               : m_fontNameEdit->text().trimmed();
    opts.familyName = m_familyNameEdit->text().trimmed().isEmpty() ? opts.fontName
                                                                   : m_familyNameEdit->text().trimmed();
    opts.fullName = m_fullNameEdit->text().trimmed().isEmpty() ? opts.familyName
                                                               : m_fullNameEdit->text().trimmed();
    opts.weight = m_weightEdit->text().trimmed().isEmpty() ? QStringLiteral("Regular")
                                                           : m_weightEdit->text().trimmed();
    opts.version = m_versionEdit->text().trimmed().isEmpty() ? QStringLiteral("1.0")
                                                             : m_versionEdit->text().trimmed();
    opts.copyright = m_copyrightEdit->text().trimmed();
    opts.ascent = m_ascentSpin->value();
    opts.descent = m_descentSpin->value();
    opts.emSize = opts.ascent + opts.descent;
    return opts;
}

} // namespace ffgui

