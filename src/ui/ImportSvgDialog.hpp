#pragma once

#include "backend/FontTypes.hpp"
#include "model/SvgSanitizer.hpp"

#include <QDialog>
#include <QMap>

class QCheckBox;
class QDragEnterEvent;
class QDropEvent;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QHBoxLayout;

namespace ffgui {

class SvgPreviewWidget;

class ImportSvgDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImportSvgDialog(const GlyphSummary& targetGlyph, const FontSummary& fontSummary,
                             const QString& initialSvgPath = QString(),
                             QWidget* parent = nullptr);

    SvgImportOptions importOptions() const;
    SvgSanitizeResult sanitizeResult() const { return m_lastResult; }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onBrowseClicked();
    void loadSvgFile(const QString& path);
    void refreshFilterPreview();

private:
    void rebuildColorCheckboxes(const QStringList& colors);
    SvgFilterOptions currentFilterOptions() const;

    GlyphSummary m_targetGlyph;
    FontSummary m_fontSummary;

    QByteArray m_rawSvg;
    SvgSanitizeResult m_lastResult;

    QLineEdit* m_pathEdit = nullptr;
    SvgPreviewWidget* m_originalPreview = nullptr;
    SvgPreviewWidget* m_filteredPreview = nullptr;
    QLabel* m_statsLabel = nullptr;

    QWidget* m_colorContainer = nullptr;
    QHBoxLayout* m_colorLayout = nullptr;
    QLabel* m_noColorsLabel = nullptr;
    QMap<QString, QCheckBox*> m_colorChecks;

    QCheckBox* m_stripMetadataCheck = nullptr;
    QCheckBox* m_removeHiddenCheck = nullptr;
    QCheckBox* m_removeBgRectsCheck = nullptr;
    QCheckBox* m_stripNoFillCheck = nullptr;
    QCheckBox* m_keepStrokesCheck = nullptr;
    QCheckBox* m_normalizeBlackCheck = nullptr;
    QCheckBox* m_whiteCutoutCheck = nullptr;
    QCheckBox* m_stripTinyCheck = nullptr;
    QDoubleSpinBox* m_minExtentSpin = nullptr;

    QRadioButton* m_replaceRadio = nullptr;
    QRadioButton* m_mergeRadio = nullptr;
    QCheckBox* m_scaleToEmCheck = nullptr;
    QCheckBox* m_correctDirCheck = nullptr;
    QCheckBox* m_removeOverlapCheck = nullptr;
    QCheckBox* m_simplifyCheck = nullptr;
    QCheckBox* m_roundToIntCheck = nullptr;
    QCheckBox* m_updateWidthCheck = nullptr;

    QPushButton* m_importBtn = nullptr;
};

} // namespace ffgui

