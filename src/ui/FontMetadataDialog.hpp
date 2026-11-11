#pragma once

#include "backend/FontTypes.hpp"

#include <QDialog>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QTableWidget;

namespace ffgui {

class FontMetadataDialog : public QDialog {
    Q_OBJECT

public:
    explicit FontMetadataDialog(const FontSummary& summary, QWidget* parent = nullptr);

    FontMetadataUpdate metadataUpdate() const;

private slots:
    void onEmMetricsChanged();
    void onAddSfntRow();
    void onRemoveSfntRow();

private:
    void appendSfntRow(int langId, int stringId, const QString& value);

    QLineEdit* m_fontNameEdit = nullptr;
    QLineEdit* m_familyNameEdit = nullptr;
    QLineEdit* m_fullNameEdit = nullptr;
    QLineEdit* m_weightEdit = nullptr;
    QLineEdit* m_versionEdit = nullptr;
    QPlainTextEdit* m_copyrightEdit = nullptr;

    QSpinBox* m_ascentSpin = nullptr;
    QSpinBox* m_descentSpin = nullptr;
    QLabel* m_emSizeLabel = nullptr;
    QDoubleSpinBox* m_italicAngleSpin = nullptr;
    QDoubleSpinBox* m_underlinePosSpin = nullptr;
    QDoubleSpinBox* m_underlineWidthSpin = nullptr;

    QTableWidget* m_sfntTable = nullptr;
};

} // namespace ffgui

