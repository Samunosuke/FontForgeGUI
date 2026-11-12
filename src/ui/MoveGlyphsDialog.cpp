#include "ui/MoveGlyphsDialog.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

namespace ffgui {

namespace {

bool parseCp(QString str, int32_t* outCp) {
    str = str.trimmed();
    if (str.startsWith(QStringLiteral("U+"), Qt::CaseInsensitive) ||
        str.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        str = str.mid(2);
    }
    bool ok = false;
    uint32_t val = str.toUInt(&ok, 16);
    if (!ok || val > 0x10FFFFu) {
        return false;
    }
    *outCp = static_cast<int32_t>(val);
    return true;
}

} // namespace

MoveGlyphsDialog::MoveGlyphsDialog(const QVector<GlyphSummary>& selectedGlyphs,
                                   const FontSummary& fontSummary,
                                   const QVector<GlyphSummary>& allGlyphs,
                                   QWidget* parent)
    : QDialog(parent),
      m_selectedGlyphs(selectedGlyphs),
      m_fontSummary(fontSummary),
      m_allGlyphs(allGlyphs) {
    setWindowTitle(tr("Move Selected Glyphs"));
    resize(640, 480);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);

    auto* headerLabel = new QLabel(
        tr("Move <b>%1</b> selected glyph(s) to a continuous range starting at a new Unicode code point.")
            .arg(m_selectedGlyphs.size()), this);
    root->addWidget(headerLabel);

    auto* targetRow = new QHBoxLayout();
    targetRow->addWidget(new QLabel(tr("Target Start Code Point:"), this));
    m_targetEdit = new QLineEdit(this);
    m_targetEdit->setPlaceholderText(tr("e.g. U+F0000 or 0xF0000"));
    targetRow->addWidget(m_targetEdit, 1);
    root->addLayout(targetRow);

    m_overwriteCheck = new QCheckBox(
        tr("Overwrite / unencode existing glyphs if target code points are already occupied"), this);
    m_overwriteCheck->setChecked(false);
    root->addWidget(m_overwriteCheck);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    root->addWidget(m_statusLabel);

    m_previewTable = new QTableWidget(this);
    m_previewTable->setColumnCount(5);
    m_previewTable->setHorizontalHeaderLabels({
        tr("Glyph Name"), tr("Current GID"), tr("Current Unicode"), tr("Target Unicode"), tr("Status")
    });
    m_previewTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_previewTable->setSelectionMode(QAbstractItemView::NoSelection);
    root->addWidget(m_previewTable, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_moveBtn = buttons->addButton(tr("Move Glyphs"), QDialogButtonBox::AcceptRole);
    m_moveBtn->setEnabled(false);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    connect(m_debounceTimer, &QTimer::timeout, this, &MoveGlyphsDialog::onTargetCodePointChanged);

    connect(m_targetEdit, &QLineEdit::textChanged, this, &MoveGlyphsDialog::onTargetTextChanged);
    connect(m_targetEdit, &QLineEdit::editingFinished, this, [this]() {
        if (m_debounceTimer->isActive()) {
            m_debounceTimer->stop();
            onTargetCodePointChanged();
        }
    });
    connect(m_overwriteCheck, &QCheckBox::toggled, this, &MoveGlyphsDialog::rebuildPreview);

    // Initial default proposal: first selected glyph's unicode or U+F0000
    if (!m_selectedGlyphs.isEmpty() && m_selectedGlyphs.first().unicode >= 0) {
        m_targetEdit->setText(
            QStringLiteral("U+%1").arg(m_selectedGlyphs.first().unicode, 4, 16, QLatin1Char('0')).toUpper());
    } else {
        m_targetEdit->setText(QStringLiteral("U+F0000"));
    }
}

MoveGlyphsRequest MoveGlyphsDialog::moveRequest() const {
    MoveGlyphsRequest req;
    for (const auto& g : m_selectedGlyphs) {
        req.gids.append(g.gid);
    }
    req.targetStartUnicode = m_parsedStartCp;
    req.overwriteOccupied = m_overwriteCheck->isChecked();
    return req;
}

void MoveGlyphsDialog::onTargetTextChanged() {
    m_moveBtn->setEnabled(false);
    m_statusLabel->setText(tr("Evaluating code points…"));
    m_debounceTimer->start(300);
}

void MoveGlyphsDialog::onTargetCodePointChanged() {
    int32_t cp = -1;
    if (parseCp(m_targetEdit->text(), &cp)) {
        m_parsedStartCp = cp;
    } else {
        m_parsedStartCp = -1;
    }
    rebuildPreview();
}

void MoveGlyphsDialog::rebuildPreview() {
    const int count = m_selectedGlyphs.size();

    m_previewTable->setUpdatesEnabled(false);
    if (m_previewTable->rowCount() != count) {
        m_previewTable->setRowCount(count);
    }

    if (m_parsedStartCp < 0) {
        m_previewTable->setUpdatesEnabled(true);
        m_statusLabel->setText(tr("<font color='#d9534f'>Please enter a valid hexadecimal Unicode (e.g. U+F0000 or 0xF0000).</font>"));
        m_moveBtn->setEnabled(false);
        return;
    }

    if (static_cast<int64_t>(m_parsedStartCp) + count - 1 > 0x10FFFF) {
        m_previewTable->setUpdatesEnabled(true);
        m_statusLabel->setText(tr("<font color='#d9534f'>Target range exceeds maximum valid Unicode code point (U+10FFFF).</font>"));
        m_moveBtn->setEnabled(false);
        return;
    }

    QSet<int> selectedGids;
    for (const auto& g : m_selectedGlyphs) {
        selectedGids.insert(g.gid);
    }

    int conflictCount = 0;
    for (int i = 0; i < count; ++i) {
        const GlyphSummary& g = m_selectedGlyphs[i];
        const int32_t targetCp = m_parsedStartCp + i;

        auto setOrUpdateItem = [this, i](int col, const QString& text, const QColor* fg = nullptr) {
            QTableWidgetItem* it = m_previewTable->item(i, col);
            if (!it) {
                it = new QTableWidgetItem(text);
                if (fg) it->setForeground(*fg);
                m_previewTable->setItem(i, col, it);
            } else {
                it->setText(text);
                if (fg) it->setForeground(*fg);
            }
        };

        setOrUpdateItem(0, g.name);
        setOrUpdateItem(1, QString::number(g.gid));
        setOrUpdateItem(2, g.unicode >= 0 ? QStringLiteral("U+%1").arg(g.unicode, 4, 16, QLatin1Char('0')).toUpper()
                                          : tr("Unencoded"));

        const QString targetStr = QStringLiteral("U+%1").arg(targetCp, 4, 16, QLatin1Char('0')).toUpper();
        setOrUpdateItem(3, targetStr);

        // Check if targetCp is occupied by an unselected glyph
        QString statusText = tr("Free");
        QColor statusColor(40, 167, 69); // Green
        for (const auto& other : m_allGlyphs) {
            if (other.unicode == targetCp && !selectedGids.contains(other.gid)) {
                ++conflictCount;
                statusText = tr("Conflict (#%1: %2)").arg(other.gid).arg(other.name);
                statusColor = QColor(220, 53, 69); // Red
                break;
            }
        }
        setOrUpdateItem(4, statusText, &statusColor);
    }

    m_previewTable->setUpdatesEnabled(true);

    m_hasConflicts = (conflictCount > 0);
    if (m_hasConflicts && !m_overwriteCheck->isChecked()) {
        m_statusLabel->setText(
            tr("<font color='#d9534f'><b>Warning:</b> %1 target slot(s) already occupied. Check the overwrite option or choose another start code point.</font>")
                .arg(conflictCount));
        m_moveBtn->setEnabled(false);
    } else if (m_hasConflicts && m_overwriteCheck->isChecked()) {
        m_statusLabel->setText(
            tr("<font color='#f59e0b'><b>Caution:</b> %1 existing glyph(s) will be unencoded during move.</font>")
                .arg(conflictCount));
        m_moveBtn->setEnabled(true);
    } else {
        m_statusLabel->setText(tr("<font color='#10b981'>All %1 target code point slots are free.</font>").arg(count));
        m_moveBtn->setEnabled(true);
    }
}

} // namespace ffgui

