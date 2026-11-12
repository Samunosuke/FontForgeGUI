#pragma once

#include "backend/FontTypes.hpp"

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>

namespace ffgui {

class NewFontDialog : public QDialog {
    Q_OBJECT

public:
    explicit NewFontDialog(QWidget* parent = nullptr);

    NewFontOptions fontOptions() const;

private:
    QLineEdit* m_fontNameEdit = nullptr;
    QLineEdit* m_familyNameEdit = nullptr;
    QLineEdit* m_fullNameEdit = nullptr;
    QLineEdit* m_weightEdit = nullptr;
    QLineEdit* m_versionEdit = nullptr;
    QLineEdit* m_copyrightEdit = nullptr;
    QSpinBox* m_ascentSpin = nullptr;
    QSpinBox* m_descentSpin = nullptr;
    QSpinBox* m_emSizeSpin = nullptr;
};

} // namespace ffgui

