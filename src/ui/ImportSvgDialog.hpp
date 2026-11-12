#pragma once

#include "backend/FontTypes.hpp"
#include "model/BitmapTracer.hpp"
#include "model/SvgSanitizer.hpp"

#include <QDialog>
#include <QImage>
#include <QMap>

class QCheckBox;
class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;
class QHBoxLayout;

namespace ffgui {

class SvgPreviewWidget;
class GlyphPlacementPreviewWidget;

class ImportSvgDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImportSvgDialog(const GlyphSummary& targetGlyph, const FontSummary& fontSummary,
                             const QString& initialFilePath = QString(),
                             QWidget* parent = nullptr);

    SvgImportOptions importOptions() const;
    SvgSanitizeResult sanitizeResult() const { return m_lastResult; }
    BitmapTraceResult traceResult() const { return m_lastTraceResult; }
    bool isPngMode() const { return m_isPngInput; }

    // Programmatic loader (used by tests and Drag & Drop)
    void loadInputFile(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onBrowseClicked();
    void refreshFilterPreview();
    void updatePlacementPreview();
    void resetPlacementControls();
    void onSaveAsDefaultClicked();
    void onResetToDefaultsClicked();

private:
    void rebuildColorCheckboxes(const QStringList& colors);
    void applyDefaultOptions(const SvgImportOptions& opts);
    SvgFilterOptions currentFilterOptions() const;
    BitmapTraceOptions currentTraceOptions() const;
    double effectivePreviewAdvanceWidth() const;

    GlyphSummary m_targetGlyph;
    FontSummary m_fontSummary;

    bool m_isPngInput = false;
    QByteArray m_rawSvg;
    QImage m_rawImage;
    SvgSanitizeResult m_lastResult;
    BitmapTraceResult m_lastTraceResult;
    QRectF m_currentInkBounds;

    QLineEdit* m_pathEdit = nullptr;
    SvgPreviewWidget* m_originalPreview = nullptr;
    GlyphPlacementPreviewWidget* m_placementPreview = nullptr;
    QLabel* m_statsLabel = nullptr;

    // SVG-only groups
    QGroupBox* m_colorGroup = nullptr;
    QWidget* m_colorContainer = nullptr;
    QHBoxLayout* m_colorLayout = nullptr;
    QLabel* m_noColorsLabel = nullptr;
    QMap<QString, QCheckBox*> m_colorChecks;

    QGroupBox* m_svgFilterGroup = nullptr;
    QCheckBox* m_stripMetadataCheck = nullptr;
    QCheckBox* m_removeHiddenCheck = nullptr;
    QCheckBox* m_removeBgRectsCheck = nullptr;
    QCheckBox* m_stripNoFillCheck = nullptr;
    QCheckBox* m_keepStrokesCheck = nullptr;
    QCheckBox* m_normalizeBlackCheck = nullptr;
    QCheckBox* m_whiteCutoutCheck = nullptr;
    QCheckBox* m_stripTinyCheck = nullptr;
    QDoubleSpinBox* m_minExtentSpin = nullptr;

    // PNG-only contour tracer group
    QGroupBox* m_pngTraceGroup = nullptr;
    QComboBox* m_pngBinarizationCombo = nullptr;
    QWidget* m_pngAlphaLumaWidget = nullptr;
    QCheckBox* m_pngUseAlphaCheck = nullptr;
    QLabel* m_pngThresholdLabel = nullptr;
    QSlider* m_pngThresholdSlider = nullptr;
    QSpinBox* m_pngThresholdSpin = nullptr;

    QWidget* m_pngColorDistanceWidget = nullptr;
    QCheckBox* m_pngAutoDetectBgCheck = nullptr;
    QPushButton* m_pngCustomBgColorBtn = nullptr;
    QLabel* m_pngDetectedBgPreview = nullptr;
    QSlider* m_pngColorToleranceSlider = nullptr;
    QLabel* m_pngColorToleranceLabel = nullptr;

    QWidget* m_pngPaletteWidget = nullptr;
    QHBoxLayout* m_pngPaletteLayout = nullptr;
    QVector<QPushButton*> m_pngPaletteButtons;
    QVector<QRgb> m_selectedPaletteColors;
    QRgb m_customBgColor = 0xFFFFFFFF;

    QCheckBox* m_pngInvertCheck = nullptr;
    QSpinBox* m_pngDespeckleSpin = nullptr;
    QDoubleSpinBox* m_pngSimplifySpin = nullptr;
    QSlider* m_pngSmoothingSlider = nullptr;
    QLabel* m_pngSmoothingValLabel = nullptr;

    void updateBinarizationModeVisibility();
    void updatePaletteButtons(const QVector<QRgb>& palette, QRgb bg);

    // Positioning & manual placement controls
    QComboBox* m_placementModeCombo = nullptr;
    QDoubleSpinBox* m_scaleSpin = nullptr;
    QDoubleSpinBox* m_offsetXSpin = nullptr;
    QDoubleSpinBox* m_offsetYSpin = nullptr;
    QPushButton* m_resetPlacementBtn = nullptr;
    bool m_syncingPlacementControls = false;

    // FontForge glyph import options
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
