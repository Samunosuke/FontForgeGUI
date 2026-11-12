#pragma once

#include "backend/FontTypes.hpp"

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QProgressBar>
#include <QTimer>

namespace ffgui {

class BatchImportDialog : public QDialog {
    Q_OBJECT

public:
    explicit BatchImportDialog(const FontSummary& fontSummary,
                               const QVector<GlyphSummary>& existingGlyphs,
                               QWidget* parent = nullptr);

    void setInitialFiles(const QStringList& files);
    void setInitialDirectory(const QString& dirPath);
    void setInitialDefinitionFile(const QString& jsonPath);

    BatchImportJob batchJob() const;

private slots:
    void onAddFilesClicked();
    void onAddDirectoryClicked();
    void onLoadJsonClicked();
    void onSaveJsonClicked();
    void onConfigureFilterClicked();
    void onClearClicked();
    void onStartUnicodeTextChanged();
    void onStartUnicodeChanged();
    void onRecomputeRows();

private:
    void addFilesToList(const QStringList& files);
    void updateTable();
    void syncControlsFromOptions();

    FontSummary m_fontSummary;
    QVector<GlyphSummary> m_existingGlyphs;

    QLineEdit* m_startUnicodeEdit = nullptr;
    QCheckBox* m_overwriteCheck = nullptr;
    QComboBox* m_placementCombo = nullptr;
    QDoubleSpinBox* m_scaleSpin = nullptr;
    QDoubleSpinBox* m_marginSpin = nullptr;
    QTableWidget* m_table = nullptr;
    QLabel* m_statsLabel = nullptr;
    QLabel* m_filterSummaryLabel = nullptr;
    QPushButton* m_importBtn = nullptr;
    QTimer* m_debounceTimer = nullptr;

    QVector<BatchImportItem> m_items;
    SvgImportOptions m_defaultOptions;
    int32_t m_parsedStartCp = 0xF0000;
};

} // namespace ffgui

