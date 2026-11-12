#include "ui/CreateGlyphDialog.hpp"

#include "model/GlyphMetadataUtils.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ffgui {

CreateGlyphDialog::CreateGlyphDialog(const FontSummary& summary, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Create New Glyph"));
    setMinimumWidth(440);

    auto* root = new QVBoxLayout(this);

    auto* hintLabel = new QLabel(
        tr("Specify a primary Unicode code point (e.g. U+E001 for a Private Use icon), "
           "a glyph name, or both. Leave Name empty to auto-generate a standard name from Unicode."),
        this);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(QStringLiteral("color: #6b7280; font-size: 11px;"));
    root->addWidget(hintLabel);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(8);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Optional if Unicode is set (e.g. icon_bl)"));

    m_unicodeEdit = new QLineEdit(this);
    m_unicodeEdit->setPlaceholderText(tr("U+E001, 0xE001, or leave empty for unencoded"));

    m_unicodeCharLabel = new QLabel(this);
    m_unicodeCharLabel->setFixedSize(28, 26);
    m_unicodeCharLabel->setAlignment(Qt::AlignCenter);
    m_unicodeCharLabel->setStyleSheet(
        QStringLiteral("background: #1f2430; border: 1px solid #303746; border-radius: 4px; "
                       "font-size: 14px; color: #e2e8f0;"));

    auto* unicodeRow = new QHBoxLayout();
    unicodeRow->setContentsMargins(0, 0, 0, 0);
    unicodeRow->setSpacing(6);
    unicodeRow->addWidget(m_unicodeEdit, 1);
    unicodeRow->addWidget(m_unicodeCharLabel);

    m_altUnicodesEdit = new QLineEdit(this);
    m_altUnicodesEdit->setPlaceholderText(tr("Comma/space separated (optional), e.g. U+F001"));

    m_advanceWidthSpin = new QSpinBox(this);
    m_advanceWidthSpin->setRange(0, 32767);
    m_advanceWidthSpin->setValue(summary.emSize > 0 ? summary.emSize : 1000);
    m_advanceWidthSpin->setSuffix(tr(" units"));

    form->addRow(tr("Name:"), m_nameEdit);
    form->addRow(tr("Unicode:"), unicodeRow);
    form->addRow(tr("Alt Unicodes:"), m_altUnicodesEdit);
    form->addRow(tr("Advance Width:"), m_advanceWidthSpin);
    root->addLayout(form);

    m_openSvgCheck = new QCheckBox(
        tr("Open \"Import SVG…\" dialog after creating glyph"), this);
    root->addWidget(m_openSvgCheck);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #dc2626; font-size: 11px;"));
    m_errorLabel->setVisible(false);
    root->addWidget(m_errorLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_createBtn = buttons->addButton(tr("Create Glyph"), QDialogButtonBox::AcceptRole);
    m_createBtn->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    connect(m_nameEdit, &QLineEdit::textChanged, this, &CreateGlyphDialog::onFieldsChanged);
    connect(m_unicodeEdit, &QLineEdit::textChanged, this, &CreateGlyphDialog::onFieldsChanged);
    connect(m_altUnicodesEdit, &QLineEdit::textChanged, this, &CreateGlyphDialog::onFieldsChanged);

    onFieldsChanged();
}

CreateGlyphRequest CreateGlyphDialog::glyphRequest() const {
    CreateGlyphRequest req;
    QString err;
    validateInputs(&req, &err);
    return req;
}

bool CreateGlyphDialog::openSvgImportAfterCreate() const {
    return m_openSvgCheck->isChecked();
}

void CreateGlyphDialog::onFieldsChanged() {
    m_unicodeCharLabel->setText(formatUnicodePreviewBadge(m_unicodeEdit->text()));

    int32_t previewCp = -1;
    if (parseSingleCodePoint(m_unicodeEdit->text(), &previewCp) && previewCp >= 0) {
        const QString autoName = suggestedGlyphName(previewCp);
        m_nameEdit->setPlaceholderText(
            autoName.isEmpty()
                ? tr("Optional — auto-derived from Unicode")
                : tr("Optional — defaults to \"%1\"").arg(autoName));
    } else {
        m_nameEdit->setPlaceholderText(tr("Required when Unicode is empty (e.g. icon_bl)"));
    }

    CreateGlyphRequest dummy;
    QString error;
    const bool valid = validateInputs(&dummy, &error);
    m_createBtn->setEnabled(valid);
    m_errorLabel->setText(error);
    m_errorLabel->setVisible(!valid && !error.isEmpty());
}

bool CreateGlyphDialog::validateInputs(CreateGlyphRequest* outRequest, QString* outError) const {
    int32_t primaryUni = -1;
    if (!parseSingleCodePoint(m_unicodeEdit->text(), &primaryUni)) {
        if (outError) {
            *outError = tr("Invalid Unicode format (use U+0041, 0x0041, decimal, or -1).");
        }
        return false;
    }

    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty() && primaryUni < 0) {
        if (outError) {
            *outError = tr("Enter a glyph name or a primary Unicode code point.");
        }
        return false;
    }

    QString nameErr;
    if (!validateGlyphName(name, &nameErr, /*allowEmpty=*/primaryUni >= 0)) {
        if (outError) {
            *outError = nameErr;
        }
        return false;
    }

    QList<int32_t> alts;
    QString altErr;
    if (!parseAltUnicodesList(m_altUnicodesEdit->text(), &alts, &altErr)) {
        if (outError) {
            *outError = altErr;
        }
        return false;
    }

    if (primaryUni >= 0 && alts.contains(primaryUni)) {
        if (outError) {
            *outError = tr("Alternate Unicode cannot duplicate the primary Unicode.");
        }
        return false;
    }

    if (outRequest) {
        outRequest->name = name;
        outRequest->unicode = primaryUni;
        outRequest->altUnicodes = alts;
        outRequest->advanceWidth = m_advanceWidthSpin->value();
    }
    if (outError) {
        outError->clear();
    }
    return true;
}

} // namespace ffgui

