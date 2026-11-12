#pragma once

#include "backend/FontTypes.hpp"
#include "model/CustomGlyphAreaConfig.hpp"

#include <QMainWindow>
#include <QThread>

class QAction;
class QCheckBox;
class QComboBox;
class QDockWidget;
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
class GlyphGridView;
class GlyphInspectorPanel;
class GlyphListModel;
class ValidationPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void openFontPath(const QString& path);
    void createNewFont(const ffgui::NewFontOptions& options);
    void openBatchImport(const QStringList& files = {}, const QString& dir = {},
                         const QString& jsonDef = {});

private slots:
    void onNewFontClicked();
    void onOpenFontClicked();
    void onSaveFontClicked();
    void onSaveFontAsClicked();
    void onExportFontClicked();
    void onCloseFontClicked();
    void onEditFontMetadataClicked();
    void onValidateFontClicked();
    void onNewGlyphClicked();
    void onMoveGlyphsClicked();
    void onImportSvgClicked();
    void onBatchImportClicked();
    void onImportFontClicked();
    void onCopyGlyphsClicked();
    void onPasteGlyphsClicked();

    void onFontLoaded(const ffgui::FontDocumentSnapshot& snapshot);
    void onFontLoadFailed(const QString& error);
    void onFontExported(const QString& outputPath);
    void onFontExportFailed(const QString& error);
    void onGlyphCreated(const ffgui::CreateGlyphResult& result);
    void onGlyphCreationFailed(const QString& error);
    void onGlyphsMoved(const ffgui::MoveGlyphsResult& result);
    void onGlyphsMoveFailed(const QString& error);
    void onFontMerged(const ffgui::FontMergeResult& result);
    void onFontMergeFailed(const QString& error);
    void onGlyphsPasted(const ffgui::FontMergeResult& result);
    void onGlyphsPasteFailed(const QString& error);
    void onGlyphMetadataUpdated(const ffgui::GlyphSummary& glyph);
    void onGlyphMetadataUpdateFailed(const QString& error);
    void onFontMetadataUpdated(const ffgui::FontSummary& summary);
    void onFontMetadataUpdateFailed(const QString& error);
    void onFontValidated(const ffgui::FontValidationReport& report);
    void onFontValidationFailed(const QString& error);
    void onSvgImported(const ffgui::SvgImportResult& result);
    void onSvgImportFailed(const QString& error);
    void onBatchImportCompleted(const ffgui::BatchImportResult& result);
    void onBatchImportFailed(const QString& error);
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
    void onGridZoomStepsRequested(int steps);
    void onGlyphSelectionChanged();
    void onValidationGlyphSelected(int gid);

private:
    void setupUi();
    void setupMenusAndToolbar();
    void setBusy(bool busy, const QString& message = QString());
    void applyCurrentAreaFilter();
    void syncAreaWidgetsFromConfig();
    void updateWindowTitleAndCounts();
    void persistAreaConfigIfOpen(bool showStatus);
    void applyUpdatedGlyph(const GlyphSummary& glyph);
    const GlyphSummary* currentSelectedGlyph() const;

    QVector<GlyphSummary> currentSelectedGlyphs() const;

    QThread m_workerThread;
    FontWorker* m_worker = nullptr;

    GlyphListModel* m_model = nullptr;
    GlyphGridDelegate* m_delegate = nullptr;
    GlyphGridView* m_gridView = nullptr;
    GlyphInspectorPanel* m_inspector = nullptr;
    ValidationPanel* m_validationPanel = nullptr;
    QDockWidget* m_validationDock = nullptr;

    QAction* m_newFontAction = nullptr;
    QAction* m_openAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_saveAsAction = nullptr;
    QAction* m_exportAction = nullptr;
    QAction* m_closeAction = nullptr;
    QAction* m_fontMetaAction = nullptr;
    QAction* m_validateAction = nullptr;
    QAction* m_newGlyphAction = nullptr;
    QAction* m_moveGlyphsAction = nullptr;
    QAction* m_importSvgAction = nullptr;
    QAction* m_batchImportAction = nullptr;
    QAction* m_importFontAction = nullptr;
    QAction* m_copyGlyphsAction = nullptr;
    QAction* m_pasteGlyphsAction = nullptr;

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
    bool m_modified = false;
    bool m_savingInPlace = false;
    bool m_savingAs = false;
    bool m_openSvgAfterCreate = false;
};

} // namespace ffgui
