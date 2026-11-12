#pragma once

#include "backend/FontTypes.hpp"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;

namespace ffgui {

class ExportFontDialog : public QDialog {
    Q_OBJECT

public:
    explicit ExportFontDialog(const FontSummary& summary, bool saveAsMode = false, QWidget* parent = nullptr);

    ExportOptions exportOptions() const;

private slots:
    void onFormatChanged(int index);
    void onBrowseClicked();

private:
    struct FormatEntry {
        QString label;
        QString extension;
    };

    QComboBox* m_formatCombo = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QCheckBox* m_roundCheck = nullptr;
    QCheckBox* m_opentypeCheck = nullptr;
    QCheckBox* m_dummyDsigCheck = nullptr;
    QCheckBox* m_omitHintsCheck = nullptr;
    QCheckBox* m_afmCheck = nullptr;

    QList<FormatEntry> m_formats;
};

} // namespace ffgui

