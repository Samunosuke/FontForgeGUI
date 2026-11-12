#pragma once

#include "backend/FontTypes.hpp"

#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTimer;

namespace ffgui {

class ImportFontDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImportFontDialog(const FontDocumentSnapshot& currentSnapshot, QWidget* parent = nullptr);
    ~ImportFontDialog() override = default;

    FontMergeOptions mergeOptions() const;

private slots:
    void onBrowseSourceFont();
    void onSourcePathChanged();
    void onRangeChanged();
    void onPreserveOriginalChanged(bool checked);
    void scheduleEvaluation();
    void evaluateImport();
    void onImportClicked();

private:
    FontDocumentSnapshot m_currentSnapshot;

    QLineEdit* m_sourcePathEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;

    QComboBox* m_rangePresetCombo = nullptr;
    QLineEdit* m_sourceRangeEdit = nullptr;

    QCheckBox* m_preserveOriginalCheck = nullptr;
    QLineEdit* m_targetStartEdit = nullptr;

    QCheckBox* m_scaleToEmCheck = nullptr;
    QCheckBox* m_copyMetricsCheck = nullptr;
    QCheckBox* m_overwriteCheck = nullptr;

    QLabel* m_statusLabel = nullptr;
    QTableWidget* m_previewTable = nullptr;
    QPushButton* m_importBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;

    QTimer* m_evalTimer = nullptr;
};

} // namespace ffgui

