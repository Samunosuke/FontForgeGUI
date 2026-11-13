#pragma once

#include "model/FontSettingsUtils.hpp"

#include <QDialog>

class QComboBox;

namespace ffgui {

/**
 * Application preferences dialog (toolbar button style and shortcut to
 * default SVG/PNG import preferences).
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(ToolbarDisplayMode currentMode, QWidget* parent = nullptr);

    ToolbarDisplayMode toolbarDisplayMode() const;

signals:
    void toolbarDisplayModePreview(ffgui::ToolbarDisplayMode mode);
    void openImportPreferencesRequested();

private:
    QComboBox* m_toolbarStyleCombo = nullptr;
};

} // namespace ffgui

