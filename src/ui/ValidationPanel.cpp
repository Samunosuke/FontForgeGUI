#include "ui/ValidationPanel.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace ffgui {

ValidationPanel::ValidationPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto* topBar = new QHBoxLayout();
    m_validateBtn = new QPushButton(tr("Validate Font"), this);
    m_validateBtn->setEnabled(false);

    m_forceCheck = new QCheckBox(tr("Force re-check all"), this);
    m_forceCheck->setChecked(true);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter issues by glyph or message…"));
    m_filterEdit->setClearButtonEnabled(true);

    m_summaryLabel = new QLabel(tr("No validation run yet"), this);

    topBar->addWidget(m_validateBtn);
    topBar->addWidget(m_forceCheck);
    topBar->addWidget(m_filterEdit, 1);
    topBar->addWidget(m_summaryLabel);
    layout->addLayout(topBar);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({tr("GID"), tr("Unicode"), tr("Glyph"), tr("Issues")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table, 1);

    connect(m_validateBtn, &QPushButton::clicked, this, [this]() {
        emit validateRequested(m_forceCheck->isChecked());
    });
    connect(m_filterEdit, &QLineEdit::textChanged, this, &ValidationPanel::onFilterChanged);
    connect(m_table, &QTableWidget::cellActivated, this, &ValidationPanel::onRowActivated);
    connect(m_table, &QTableWidget::cellClicked, this, &ValidationPanel::onRowActivated);
}

void ValidationPanel::setFontAvailable(bool available) {
    m_validateBtn->setEnabled(available);
    if (!available) {
        clear();
    }
}

void ValidationPanel::setReport(const FontValidationReport& report) {
    m_report = report;
    m_hasReport = true;

    if (report.issueGlyphCount == 0) {
        m_summaryLabel->setText(
            tr("Passed: 0 issues (%1 glyphs checked)").arg(report.totalGlyphs));
        m_summaryLabel->setStyleSheet(QStringLiteral("color: #2ea043; font-weight: 600;"));
    } else {
        m_summaryLabel->setText(tr("Found issues in %1 of %2 glyphs")
                                    .arg(report.issueGlyphCount)
                                    .arg(report.totalGlyphs));
        m_summaryLabel->setStyleSheet(QStringLiteral("color: #d9534f; font-weight: 600;"));
    }

    repopulateTable();
}

void ValidationPanel::clear() {
    m_hasReport = false;
    m_report = FontValidationReport{};
    m_table->setRowCount(0);
    m_summaryLabel->setText(tr("No validation run yet"));
    m_summaryLabel->setStyleSheet(QString());
}

void ValidationPanel::onFilterChanged() {
    if (m_hasReport) {
        repopulateTable();
    }
}

void ValidationPanel::onRowActivated(int row, int /*column*/) {
    if (row < 0 || row >= m_table->rowCount()) {
        return;
    }
    QTableWidgetItem* gidItem = m_table->item(row, 0);
    if (!gidItem) {
        return;
    }
    const int gid = gidItem->data(Qt::UserRole).toInt();
    emit glyphSelected(gid);
}

void ValidationPanel::repopulateTable() {
    m_table->setRowCount(0);
    const QString query = m_filterEdit->text().trimmed();

    for (const GlyphValidationIssue& issue : m_report.issues) {
        const QString joinedDesc = issue.descriptions.join(QStringLiteral("; "));
        const QString uniText =
            issue.unicode >= 0
                ? QStringLiteral("U+%1").arg(issue.unicode, 4, 16, QLatin1Char('0')).toUpper()
                : QStringLiteral("—");

        if (!query.isEmpty()) {
            const bool match = issue.glyphName.contains(query, Qt::CaseInsensitive) ||
                               joinedDesc.contains(query, Qt::CaseInsensitive) ||
                               uniText.contains(query, Qt::CaseInsensitive) ||
                               QString::number(issue.gid) == query;
            if (!match) {
                continue;
            }
        }

        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto* gidItem = new QTableWidgetItem(QStringLiteral("#%1").arg(issue.gid));
        gidItem->setData(Qt::UserRole, issue.gid);

        m_table->setItem(row, 0, gidItem);
        m_table->setItem(row, 1, new QTableWidgetItem(uniText));
        m_table->setItem(row, 2, new QTableWidgetItem(issue.glyphName));
        m_table->setItem(row, 3, new QTableWidgetItem(joinedDesc));
    }
}

} // namespace ffgui

