#pragma once

#include "backend/FontTypes.hpp"

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QCheckBox>

namespace ffgui {

class MoveGlyphsDialog : public QDialog {
    Q_OBJECT

public:
    explicit MoveGlyphsDialog(const QVector<GlyphSummary>& selectedGlyphs,
                              const FontSummary& fontSummary,
                              const QVector<GlyphSummary>& allGlyphs,
                              QWidget* parent = nullptr);

    MoveGlyphsRequest moveRequest() const;

private slots:
    void onTargetCodePointChanged();

private:
    void rebuildPreview();

    QVector<GlyphSummary> m_selectedGlyphs;
    FontSummary m_fontSummary;
    QVector<GlyphSummary> m_allGlyphs;

    QLineEdit* m_targetEdit = nullptr;
    QCheckBox* m_overwriteCheck = nullptr;
    QLabel* m_statusLabel = nullptr;
    QTableWidget* m_previewTable = nullptr;
    QPushButton* m_moveBtn = nullptr;

    int32_t m_parsedStartCp = -1;
    bool m_hasConflicts = false;
};

} // namespace ffgui

