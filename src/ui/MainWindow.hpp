#pragma once

#include "backend/FontTypes.hpp"
#include "model/CustomGlyphAreaConfig.hpp"

#include <QMainWindow>
#include <QThread>

class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSlider;

namespace ffgui {

class FontWorker;
class GlyphGridDelegate;
class GlyphInspectorPanel;
class GlyphListModel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void openFontPath(const QString& path);

private slots:
    void onOpenFontClicked();
    void onExportFontClicked();
    void onCloseFontClicked();

    void onFontLoaded(const ffgui::FontDocumentSnapshot& snapshot);
    void onFontLoadFailed(const QString& error);
    void onFontExported(const QString& outputPath);
    void onFontExportFailed(const QString& error);
    void onFontClosed();
    void onLogMessage(const QString& level, const QString& message);
    void onProgressUpdated(int current, int total, const QString& status);

    void onAreaComboChanged(int index);
    void onRangeTextChanged(const QString& text);
    void onIncludeUnencodedToggled(bool checked);
    void onAddAreaClicked();
    void onRemoveAreaClicked();
    void onSaveAreaConfigClicked();
    void onSearchTextChanged(const QString& text);
    void onZoomChanged(int value);
    void onGlyphSelectionChanged();

private:
    void setupUi();
    void setupMenusAndToolbar();
    void setBusy(bool busy, const QString& message = QString());
    void applyCurrentAreaFilter();
    void syncAreaWidgetsFromConfig();
    void updateWindowTitleAndCounts();
    void persistAreaConfigIfOpen(bool showStatus);

    QThread m_workerThread;
    FontWorker* m_worker = nullptr;

    GlyphListModel* m_model = nullptr;
    GlyphGridDelegate* m_delegate = nullptr;
    QListView* m_gridView = nullptr;
    GlyphInspectorPanel* m_inspector = nullptr;

    QAction* m_openAction = nullptr;
    QAction* m_exportAction = nullptr;
    QAction* m_closeAction = nullptr;

    QComboBox* m_areaCombo = nullptr;
    QLineEdit* m_rangeEdit = nullptr;
    QCheckBox* m_unencodedCheck = nullptr;
    QPushButton* m_presetBtn = nullptr;
    QPushButton* m_addAreaBtn = nullptr;
    QPushButton* m_removeAreaBtn = nullptr;
    QPushButton* m_saveAreasBtn = nullptr;
    QLabel* m_rangeStatusLabel = nullptr;

    QLineEdit* m_searchEdit = nullptr;
    QSlider* m_zoomSlider = nullptr;
    QLabel* m_countLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPlainTextEdit* m_logView = nullptr;

    QString m_currentFontPath;
    CustomGlyphAreaConfig m_areaConfig;
    bool m_updatingAreaUi = false;
    bool m_hasOpenFont = false;
};

} // namespace ffgui

