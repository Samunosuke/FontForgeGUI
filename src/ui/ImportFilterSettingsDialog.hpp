#pragma once

#include "backend/FontTypes.hpp"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;

namespace ffgui {

class ImportFilterSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImportFilterSettingsDialog(const SvgImportOptions& initialOptions,
                                        QWidget* parent = nullptr);

    SvgImportOptions importOptions() const;

private slots:
    void onSaveAsDefaultClicked();
    void onResetToDefaultsClicked();

private:
    void populateFromOptions(const SvgImportOptions& opts);

    // Placement
    QComboBox* m_placementCombo = nullptr;
    QCheckBox* m_scaleToEmCheck = nullptr;
    QDoubleSpinBox* m_scaleFactorSpin = nullptr;
    QDoubleSpinBox* m_offsetXSpin = nullptr;
    QDoubleSpinBox* m_offsetYSpin = nullptr;
    QDoubleSpinBox* m_marginRatioSpin = nullptr;
    QCheckBox* m_simplifyCheck = nullptr;
    QCheckBox* m_correctDirectionCheck = nullptr;
    QCheckBox* m_removeOverlapCheck = nullptr;
    QCheckBox* m_roundToIntCheck = nullptr;

    // Vector Cleaning
    QCheckBox* m_normalizeToBlackCheck = nullptr;
    QCheckBox* m_whiteAsCutoutCheck = nullptr;
    QCheckBox* m_keepStrokesCheck = nullptr;
    QCheckBox* m_stripTinyPathsCheck = nullptr;
    QDoubleSpinBox* m_minGeometryExtentSpin = nullptr;

    // Bitmap Tracer
    QCheckBox* m_useAlphaCheck = nullptr;
    QSpinBox* m_bitmapThresholdSpin = nullptr;
    QCheckBox* m_invertBitmapCheck = nullptr;
    QSpinBox* m_minSpeckleAreaSpin = nullptr;
    QDoubleSpinBox* m_curveSmoothingSpin = nullptr;

    QPushButton* m_saveAsDefaultBtn = nullptr;
    QPushButton* m_resetDefaultsBtn = nullptr;
};

} // namespace ffgui

