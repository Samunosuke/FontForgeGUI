#include "ui/ImportFilterSettingsDialog.hpp"
#include "model/FontSettingsUtils.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace ffgui {

ImportFilterSettingsDialog::ImportFilterSettingsDialog(const SvgImportOptions& initialOptions,
                                                       QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Import Filter & Vectorization Parameters"));
    resize(560, 480);

    auto* mainLayout = new QVBoxLayout(this);
    auto* tabWidget = new QTabWidget(this);

    // -------------------------------------------------------------------------
    // Tab 1: Placement & Sizing
    // -------------------------------------------------------------------------
    auto* tabPlacement = new QWidget(this);
    auto* formPlacement = new QFormLayout(tabPlacement);

    m_placementCombo = new QComboBox(tabPlacement);
    m_placementCombo->addItem(tr("Auto Aspect Ratio (Square/Tall on Baseline, Wide Centered)"),
                              static_cast<int>(GlyphPlacementMode::AutoAspectRatio));
    m_placementCombo->addItem(tr("Baseline Centered (Bottom on Baseline y=0)"),
                              static_cast<int>(GlyphPlacementMode::BaselineCentered));
    m_placementCombo->addItem(tr("Center in Ascent (Between Baseline and Ascent)"),
                              static_cast<int>(GlyphPlacementMode::CenterInAscent));
    m_placementCombo->addItem(tr("Center in EM Box (Between -Descent and +Ascent)"),
                              static_cast<int>(GlyphPlacementMode::CenterInEm));
    m_placementCombo->addItem(tr("Original SVG Coordinates (Raw)"),
                              static_cast<int>(GlyphPlacementMode::OriginalSvgCoords));
    formPlacement->addRow(tr("Placement Mode:"), m_placementCombo);

    m_scaleToEmCheck = new QCheckBox(tr("Scale automatically to fit EM box"), tabPlacement);
    formPlacement->addRow(QString(), m_scaleToEmCheck);

    m_scaleFactorSpin = new QDoubleSpinBox(tabPlacement);
    m_scaleFactorSpin->setRange(0.05, 10.0);
    m_scaleFactorSpin->setSingleStep(0.05);
    m_scaleFactorSpin->setDecimals(2);
    m_scaleFactorSpin->setValue(1.0);
    formPlacement->addRow(tr("Manual Scale Multiplier:"), m_scaleFactorSpin);

    m_offsetXSpin = new QDoubleSpinBox(tabPlacement);
    m_offsetXSpin->setRange(-4000.0, 4000.0);
    m_offsetXSpin->setSingleStep(5.0);
    m_offsetXSpin->setDecimals(1);
    formPlacement->addRow(tr("Offset X (Font Units):"), m_offsetXSpin);

    m_offsetYSpin = new QDoubleSpinBox(tabPlacement);
    m_offsetYSpin->setRange(-4000.0, 4000.0);
    m_offsetYSpin->setSingleStep(5.0);
    m_offsetYSpin->setDecimals(1);
    formPlacement->addRow(tr("Offset Y (Font Units):"), m_offsetYSpin);

    m_marginRatioSpin = new QDoubleSpinBox(tabPlacement);
    m_marginRatioSpin->setRange(0.0, 0.5);
    m_marginRatioSpin->setSingleStep(0.01);
    m_marginRatioSpin->setDecimals(3);
    formPlacement->addRow(tr("Padding / Margin Ratio:"), m_marginRatioSpin);

    m_simplifyCheck = new QCheckBox(tr("Simplify spline contours after import"), tabPlacement);
    m_correctDirectionCheck = new QCheckBox(tr("Correct contour directions (outer CW, inner CCW)"), tabPlacement);
    m_removeOverlapCheck = new QCheckBox(tr("Remove contour overlaps"), tabPlacement);
    m_roundToIntCheck = new QCheckBox(tr("Round point coordinates to integer"), tabPlacement);

    formPlacement->addRow(QString(), m_simplifyCheck);
    formPlacement->addRow(QString(), m_correctDirectionCheck);
    formPlacement->addRow(QString(), m_removeOverlapCheck);
    formPlacement->addRow(QString(), m_roundToIntCheck);

    tabWidget->addTab(tabPlacement, tr("Placement & Splines"));

    // -------------------------------------------------------------------------
    // Tab 2: SVG Vector Cleaning
    // -------------------------------------------------------------------------
    auto* tabVector = new QWidget(this);
    auto* formVector = new QFormLayout(tabVector);

    m_normalizeToBlackCheck = new QCheckBox(tr("Flatten foreground colors to solid black"), tabVector);
    m_whiteAsCutoutCheck = new QCheckBox(tr("Extract pure white (#FFFFFF) foreground shapes as cutouts/holes"), tabVector);
    m_keepStrokesCheck = new QCheckBox(tr("Extract outline strokes and expand to filled contours"), tabVector);
    m_stripTinyPathsCheck = new QCheckBox(tr("Strip tiny speck paths (despeckle vector artifacts)"), tabVector);

    formVector->addRow(QString(), m_normalizeToBlackCheck);
    formVector->addRow(QString(), m_whiteAsCutoutCheck);
    formVector->addRow(QString(), m_keepStrokesCheck);
    formVector->addRow(QString(), m_stripTinyPathsCheck);

    m_minGeometryExtentSpin = new QDoubleSpinBox(tabVector);
    m_minGeometryExtentSpin->setRange(0.1, 50.0);
    m_minGeometryExtentSpin->setSingleStep(0.5);
    m_minGeometryExtentSpin->setDecimals(1);
    formVector->addRow(tr("Min Speck Extent (px):"), m_minGeometryExtentSpin);

    tabWidget->addTab(tabVector, tr("SVG Vector Cleaning"));

    // -------------------------------------------------------------------------
    // Tab 3: PNG Bitmap Tracer
    // -------------------------------------------------------------------------
    auto* tabBitmap = new QWidget(this);
    auto* formBitmap = new QFormLayout(tabBitmap);

    m_useAlphaCheck = new QCheckBox(tr("Use alpha transparency channel when available"), tabBitmap);
    formBitmap->addRow(QString(), m_useAlphaCheck);

    m_bitmapThresholdSpin = new QSpinBox(tabBitmap);
    m_bitmapThresholdSpin->setRange(1, 254);
    m_bitmapThresholdSpin->setValue(128);
    formBitmap->addRow(tr("Alpha / Luminance Threshold (1..254):"), m_bitmapThresholdSpin);

    m_invertBitmapCheck = new QCheckBox(tr("Invert foreground and background"), tabBitmap);
    formBitmap->addRow(QString(), m_invertBitmapCheck);

    m_minSpeckleAreaSpin = new QSpinBox(tabBitmap);
    m_minSpeckleAreaSpin->setRange(0, 1000);
    m_minSpeckleAreaSpin->setValue(4);
    formBitmap->addRow(tr("Drop Pixel Islands Smaller Than (px²):"), m_minSpeckleAreaSpin);

    m_curveSmoothingSpin = new QDoubleSpinBox(tabBitmap);
    m_curveSmoothingSpin->setRange(0.0, 1.0);
    m_curveSmoothingSpin->setSingleStep(0.05);
    m_curveSmoothingSpin->setDecimals(2);
    m_curveSmoothingSpin->setValue(0.75);
    formBitmap->addRow(tr("Cubic Bézier Smoothing (0.0=poly, 1.0=max):"), m_curveSmoothingSpin);

    tabWidget->addTab(tabBitmap, tr("PNG Bitmap Tracer"));

    mainLayout->addWidget(tabWidget);

    // -------------------------------------------------------------------------
    // Defaults Bar & Action Buttons
    // -------------------------------------------------------------------------
    auto* defLayout = new QHBoxLayout();
    m_saveAsDefaultBtn = new QPushButton(tr("Save as Application Default"), this);
    m_resetDefaultsBtn = new QPushButton(tr("Reset to Factory Defaults"), this);
    defLayout->addWidget(m_saveAsDefaultBtn);
    defLayout->addWidget(m_resetDefaultsBtn);
    defLayout->addStretch(1);
    mainLayout->addLayout(defLayout);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    connect(m_saveAsDefaultBtn, &QPushButton::clicked, this,
            &ImportFilterSettingsDialog::onSaveAsDefaultClicked);
    connect(m_resetDefaultsBtn, &QPushButton::clicked, this,
            &ImportFilterSettingsDialog::onResetToDefaultsClicked);

    populateFromOptions(initialOptions);
}

void ImportFilterSettingsDialog::populateFromOptions(const SvgImportOptions& opts) {
    const int idx = m_placementCombo->findData(static_cast<int>(opts.placementMode));
    if (idx >= 0) {
        m_placementCombo->setCurrentIndex(idx);
    }
    m_scaleToEmCheck->setChecked(opts.scaleToEm);
    m_scaleFactorSpin->setValue(opts.scaleFactor > 0.01 ? opts.scaleFactor : 1.0);
    m_offsetXSpin->setValue(opts.offsetX);
    m_offsetYSpin->setValue(opts.offsetY);
    m_marginRatioSpin->setValue(opts.marginRatio >= 0.0 ? opts.marginRatio : 0.04);
    m_simplifyCheck->setChecked(opts.simplify);
    m_correctDirectionCheck->setChecked(opts.correctDirection);
    m_removeOverlapCheck->setChecked(opts.removeOverlap);
    m_roundToIntCheck->setChecked(opts.roundToInt);

    m_normalizeToBlackCheck->setChecked(opts.normalizeToBlackFill);
    m_whiteAsCutoutCheck->setChecked(opts.whiteAsCutout);
    m_keepStrokesCheck->setChecked(opts.keepStrokes);
    m_stripTinyPathsCheck->setChecked(opts.stripTinyPaths);
    m_minGeometryExtentSpin->setValue(opts.minGeometryExtent > 0.0 ? opts.minGeometryExtent : 1.0);

    m_useAlphaCheck->setChecked(opts.useAlpha);
    m_bitmapThresholdSpin->setValue(opts.bitmapThreshold > 0 ? opts.bitmapThreshold : 128);
    m_invertBitmapCheck->setChecked(opts.invertBitmap);
    m_minSpeckleAreaSpin->setValue(opts.minSpeckleArea >= 0 ? opts.minSpeckleArea : 4);
    m_curveSmoothingSpin->setValue(opts.curveSmoothing >= 0.0 ? opts.curveSmoothing : 0.75);
}

SvgImportOptions ImportFilterSettingsDialog::importOptions() const {
    SvgImportOptions opts;
    opts.placementMode = static_cast<GlyphPlacementMode>(
        m_placementCombo->currentData().toInt());
    opts.scaleToEm = m_scaleToEmCheck->isChecked();
    opts.scaleFactor = m_scaleFactorSpin->value();
    opts.offsetX = m_offsetXSpin->value();
    opts.offsetY = m_offsetYSpin->value();
    opts.marginRatio = m_marginRatioSpin->value();
    opts.simplify = m_simplifyCheck->isChecked();
    opts.correctDirection = m_correctDirectionCheck->isChecked();
    opts.removeOverlap = m_removeOverlapCheck->isChecked();
    opts.roundToInt = m_roundToIntCheck->isChecked();

    opts.normalizeToBlackFill = m_normalizeToBlackCheck->isChecked();
    opts.whiteAsCutout = m_whiteAsCutoutCheck->isChecked();
    opts.keepStrokes = m_keepStrokesCheck->isChecked();
    opts.stripTinyPaths = m_stripTinyPathsCheck->isChecked();
    opts.minGeometryExtent = m_minGeometryExtentSpin->value();

    opts.useAlpha = m_useAlphaCheck->isChecked();
    opts.bitmapThreshold = m_bitmapThresholdSpin->value();
    opts.invertBitmap = m_invertBitmapCheck->isChecked();
    opts.minSpeckleArea = m_minSpeckleAreaSpin->value();
    opts.curveSmoothing = m_curveSmoothingSpin->value();
    return opts;
}

void ImportFilterSettingsDialog::onSaveAsDefaultClicked() {
    saveDefaultImportOptions(importOptions());
    QMessageBox::information(this, tr("Import Defaults"),
                             tr("Current parameters were saved as the application default for future imports."));
}

void ImportFilterSettingsDialog::onResetToDefaultsClicked() {
    resetDefaultImportOptions();
    populateFromOptions(SvgImportOptions());
    QMessageBox::information(this, tr("Reset Defaults"),
                             tr("Import parameters have been reset to factory defaults."));
}

} // namespace ffgui

