#pragma once

#include "backend/FontTypes.hpp"

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace ffgui {

/**
 * Dialog for creating a new glyph in the currently open font with the same
 * metadata fields as the Glyph Metadata Editor (Name, Unicode, Alt Unicodes)
 * plus initial advance width and optional SVG import follow-up.
 */
class CreateGlyphDialog : public QDialog {
    Q_OBJECT

public:
    explicit CreateGlyphDialog(const FontSummary& summary, QWidget* parent = nullptr);

    CreateGlyphRequest glyphRequest() const;
    bool openSvgImportAfterCreate() const;

private slots:
    void onFieldsChanged();

private:
    bool validateInputs(CreateGlyphRequest* outRequest, QString* outError) const;

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_unicodeEdit = nullptr;
    QLabel* m_unicodeCharLabel = nullptr;
    QLineEdit* m_altUnicodesEdit = nullptr;
    QSpinBox* m_advanceWidthSpin = nullptr;
    QCheckBox* m_openSvgCheck = nullptr;
    QLabel* m_errorLabel = nullptr;
    QPushButton* m_createBtn = nullptr;
};

} // namespace ffgui

