#pragma once

#include "backend/FontTypes.hpp"

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace ffgui {

class ValidationPanel : public QWidget {
    Q_OBJECT

public:
    explicit ValidationPanel(QWidget* parent = nullptr);

    void setFontAvailable(bool available);
    void setReport(const FontValidationReport& report);
    void clear();

signals:
    void validateRequested(bool force);
    void glyphSelected(int gid);

private slots:
    void onFilterChanged();
    void onRowActivated(int row, int column);

private:
    void repopulateTable();

    QPushButton* m_validateBtn = nullptr;
    QCheckBox* m_forceCheck = nullptr;
    QLineEdit* m_filterEdit = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QTableWidget* m_table = nullptr;

    FontValidationReport m_report;
    bool m_hasReport = false;
};

} // namespace ffgui

