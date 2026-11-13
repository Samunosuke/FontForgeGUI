#include "ui/MainWindow.hpp"

#include "backend/FontWorker.hpp"
#include "model/FontSettingsUtils.hpp"
#include "model/GlyphClipboardUtils.hpp"
#include "model/GlyphListModel.hpp"
#include "model/UnicodeRangeFilter.hpp"
#include "ui/BatchImportDialog.hpp"
#include "ui/CreateGlyphDialog.hpp"
#include "ui/ExportFontDialog.hpp"
#include "ui/FontMetadataDialog.hpp"
#include "ui/ImportFilterSettingsDialog.hpp"
#include "ui/ImportFontDialog.hpp"
#include "ui/GlyphGridDelegate.hpp"
#include "ui/GlyphGridView.hpp"
#include "ui/GlyphInspectorPanel.hpp"
#include "ui/IconUtils.hpp"
#include "ui/ImportSvgDialog.hpp"
#include "ui/MoveGlyphsDialog.hpp"
#include "ui/NewFontDialog.hpp"
#include "ui/SettingsDialog.hpp"
#include "ui/ValidationPanel.hpp"

#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <algorithm>

namespace ffgui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    qRegisterMetaType<FontDocumentSnapshot>("ffgui::FontDocumentSnapshot");
    qRegisterMetaType<FontSummary>("ffgui::FontSummary");
    qRegisterMetaType<GlyphSummary>("ffgui::GlyphSummary");
    qRegisterMetaType<ExportOptions>("ffgui::ExportOptions");
    qRegisterMetaType<GlyphMetadataUpdate>("ffgui::GlyphMetadataUpdate");
    qRegisterMetaType<FontMetadataUpdate>("ffgui::FontMetadataUpdate");
    qRegisterMetaType<FontValidationReport>("ffgui::FontValidationReport");
    qRegisterMetaType<SvgImportOptions>("ffgui::SvgImportOptions");
    qRegisterMetaType<SvgImportResult>("ffgui::SvgImportResult");
    qRegisterMetaType<CreateGlyphRequest>("ffgui::CreateGlyphRequest");
    qRegisterMetaType<CreateGlyphResult>("ffgui::CreateGlyphResult");
    qRegisterMetaType<NewFontOptions>("ffgui::NewFontOptions");
    qRegisterMetaType<MoveGlyphsRequest>("ffgui::MoveGlyphsRequest");
    qRegisterMetaType<MoveGlyphsResult>("ffgui::MoveGlyphsResult");
    qRegisterMetaType<BatchImportJob>("ffgui::BatchImportJob");
    qRegisterMetaType<BatchImportResult>("ffgui::BatchImportResult");
    qRegisterMetaType<FontMergeOptions>("ffgui::FontMergeOptions");
    qRegisterMetaType<FontMergeResult>("ffgui::FontMergeResult");
    qRegisterMetaType<QVector<ffgui::GlyphSummary>>("QVector<ffgui::GlyphSummary>");

    m_areaConfig = CustomGlyphAreaConfig::createDefault();

    setupUi();
    setupMenusAndToolbar();
    syncAreaWidgetsFromConfig();
    applyCurrentAreaFilter();

    m_worker = new FontWorker();
    m_worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::started, m_worker, &FontWorker::initialize);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &FontWorker::fontLoaded, this, &MainWindow::onFontLoaded);
    connect(m_worker, &FontWorker::fontLoadFailed, this, &MainWindow::onFontLoadFailed);
    connect(m_worker, &FontWorker::fontExported, this, &MainWindow::onFontExported);
    connect(m_worker, &FontWorker::fontExportFailed, this, &MainWindow::onFontExportFailed);
    connect(m_worker, &FontWorker::glyphCreated, this, &MainWindow::onGlyphCreated);
    connect(m_worker, &FontWorker::glyphCreationFailed, this, &MainWindow::onGlyphCreationFailed);
    connect(m_worker, &FontWorker::glyphsMoved, this, &MainWindow::onGlyphsMoved);
    connect(m_worker, &FontWorker::glyphsMoveFailed, this, &MainWindow::onGlyphsMoveFailed);
    connect(m_worker, &FontWorker::glyphMetadataUpdated, this,
            &MainWindow::onGlyphMetadataUpdated);
    connect(m_worker, &FontWorker::glyphMetadataUpdateFailed, this,
            &MainWindow::onGlyphMetadataUpdateFailed);
    connect(m_worker, &FontWorker::fontMetadataUpdated, this, &MainWindow::onFontMetadataUpdated);
    connect(m_worker, &FontWorker::fontMetadataUpdateFailed, this,
            &MainWindow::onFontMetadataUpdateFailed);
    connect(m_worker, &FontWorker::fontValidated, this, &MainWindow::onFontValidated);
    connect(m_worker, &FontWorker::fontValidationFailed, this,
            &MainWindow::onFontValidationFailed);
    connect(m_worker, &FontWorker::svgImported, this, &MainWindow::onSvgImported);
    connect(m_worker, &FontWorker::svgImportFailed, this, &MainWindow::onSvgImportFailed);
    connect(m_worker, &FontWorker::batchImportCompleted, this, &MainWindow::onBatchImportCompleted);
    connect(m_worker, &FontWorker::batchImportFailed, this, &MainWindow::onBatchImportFailed);
    connect(m_worker, &FontWorker::fontMerged, this, &MainWindow::onFontMerged);
    connect(m_worker, &FontWorker::fontMergeFailed, this, &MainWindow::onFontMergeFailed);
    connect(m_worker, &FontWorker::glyphsPasted, this, &MainWindow::onGlyphsPasted);
    connect(m_worker, &FontWorker::glyphsPasteFailed, this, &MainWindow::onGlyphsPasteFailed);
    connect(m_worker, &FontWorker::fontClosed, this, &MainWindow::onFontClosed);
    connect(m_worker, &FontWorker::logMessage, this, &MainWindow::onLogMessage);
    connect(m_worker, &FontWorker::progressUpdated, this, &MainWindow::onProgressUpdated);

    m_workerThread.start();
    updateWindowTitleAndCounts();
}

MainWindow::~MainWindow() {
    if (m_workerThread.isRunning()) {
        QMetaObject::invokeMethod(m_worker, "closeFont", Qt::QueuedConnection);
        m_workerThread.quit();
        m_workerThread.wait();
    }
}

void MainWindow::setupUi() {
    resize(1280, 820);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(8, 6, 8, 6);
    rootLayout->setSpacing(6);

    // Custom Glyph Area & Search/Zoom grid (aligned columns across both rows)
    auto* controlsGrid = new QGridLayout();
    controlsGrid->setContentsMargins(0, 0, 0, 0);
    controlsGrid->setHorizontalSpacing(8);
    controlsGrid->setVerticalSpacing(6);

    auto* areaLabel = new QLabel(tr("Custom Area:"), central);
    areaLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    controlsGrid->addWidget(areaLabel, 0, 0);

    auto* areaSelectRow = new QHBoxLayout();
    areaSelectRow->setContentsMargins(0, 0, 0, 0);
    areaSelectRow->setSpacing(4);

    m_areaCombo = new QComboBox(central);
    m_areaCombo->setMinimumWidth(145);
    areaSelectRow->addWidget(m_areaCombo, 1);

    m_addAreaBtn = new QPushButton(central);
    m_addAreaBtn->setIcon(loadTintedSvgIcon(QStringLiteral(":/icons/plus.svg")));
    m_addAreaBtn->setIconSize(QSize(14, 14));
    m_addAreaBtn->setToolTip(tr("Create a new Custom Glyph Area"));
    m_addAreaBtn->setFixedSize(26, 26);
    areaSelectRow->addWidget(m_addAreaBtn);

    m_removeAreaBtn = new QPushButton(central);
    m_removeAreaBtn->setIcon(loadTintedSvgIcon(QStringLiteral(":/icons/minus.svg")));
    m_removeAreaBtn->setIconSize(QSize(14, 14));
    m_removeAreaBtn->setToolTip(tr("Remove current Custom Glyph Area"));
    m_removeAreaBtn->setFixedSize(26, 26);
    areaSelectRow->addWidget(m_removeAreaBtn);

    controlsGrid->addLayout(areaSelectRow, 0, 1);

    auto* rangesLabel = new QLabel(tr("Ranges:"), central);
    rangesLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    controlsGrid->addWidget(rangesLabel, 0, 2);

    auto* rangeInputRow = new QHBoxLayout();
    rangeInputRow->setContentsMargins(0, 0, 0, 0);
    rangeInputRow->setSpacing(4);

    m_rangeEdit = new QLineEdit(central);
    m_rangeEdit->setPlaceholderText(tr("e.g. U+0020-007E, U+E000-E0FF or * for all"));
    m_rangeEdit->setClearButtonEnabled(true);
    rangeInputRow->addWidget(m_rangeEdit, 1);

    m_rangeStatusLabel = new QLabel(central);
    m_rangeStatusLabel->setFixedSize(18, 18);
    m_rangeStatusLabel->setAlignment(Qt::AlignCenter);
    m_rangeStatusLabel->setVisible(false);
    rangeInputRow->addWidget(m_rangeStatusLabel);

    controlsGrid->addLayout(rangeInputRow, 0, 3);

    m_presetBtn = new QPushButton(tr("Presets ▾"), central);
    auto* presetMenu = new QMenu(m_presetBtn);
    for (const UnicodeRangePreset& preset : UnicodeRangeFilter::defaultPresets()) {
        QAction* act = presetMenu->addAction(preset.name);
        connect(act, &QAction::triggered, this, [this, preset]() {
            m_rangeEdit->setText(preset.rangeSpec);
            m_unencodedCheck->setChecked(preset.includeUnencoded);
        });
    }
    m_presetBtn->setMenu(presetMenu);
    controlsGrid->addWidget(m_presetBtn, 0, 4);

    m_unencodedCheck = new QCheckBox(tr("Include unencoded"), central);
    controlsGrid->addWidget(m_unencodedCheck, 0, 5);

    m_saveAreasBtn = new QPushButton(tr("Save Areas"), central);
    m_saveAreasBtn->setIcon(loadTintedSvgIcon(QStringLiteral(":/icons/save.svg")));
    m_saveAreasBtn->setIconSize(QSize(15, 15));
    m_saveAreasBtn->setToolTip(tr("Save Custom Glyph Areas to sidecar file (.ffgui.json)"));
    controlsGrid->addWidget(m_saveAreasBtn, 0, 6);

    // Row 1: Find Glyph (aligned with Custom Area & Ranges) + Cell Size + Count
    auto* findLabelRow = new QHBoxLayout();
    findLabelRow->setContentsMargins(0, 0, 0, 0);
    findLabelRow->setSpacing(4);
    findLabelRow->addStretch(1);
    auto* findIconLabel = new QLabel(central);
    findIconLabel->setPixmap(loadTintedSvgPixmap(QStringLiteral(":/icons/search.svg"), 15));
    findIconLabel->setToolTip(tr("Filter glyphs by name, Unicode, or GID"));
    findLabelRow->addWidget(findIconLabel);
    auto* findTextLabel = new QLabel(tr("Find:"), central);
    findTextLabel->setToolTip(tr("Filter glyphs by name, Unicode, or GID"));
    findLabelRow->addWidget(findTextLabel);
    controlsGrid->addLayout(findLabelRow, 1, 0);

    m_searchEdit = new QLineEdit(central);
    m_searchEdit->setPlaceholderText(tr("Filter by glyph name, Unicode (U+0041 / 0041), or GID (#42)…"));
    m_searchEdit->setClearButtonEnabled(true);
    controlsGrid->addWidget(m_searchEdit, 1, 1, 1, 3);

    auto* zoomRow = new QHBoxLayout();
    zoomRow->setContentsMargins(0, 0, 0, 0);
    zoomRow->setSpacing(6);
    auto* zoomIconLabel = new QLabel(central);
    zoomIconLabel->setPixmap(loadTintedSvgPixmap(QStringLiteral(":/icons/zoom.svg"), 15));
    zoomIconLabel->setToolTip(tr("Cell Size (Ctrl++/Ctrl+- or Ctrl+Wheel)"));
    zoomRow->addWidget(zoomIconLabel);
    auto* zoomTextLabel = new QLabel(tr("Cell Size:"), central);
    zoomTextLabel->setToolTip(tr("Cell Size (Ctrl++/Ctrl+- or Ctrl+Wheel)"));
    zoomRow->addWidget(zoomTextLabel);
    m_zoomSlider = new QSlider(Qt::Horizontal, central);
    m_zoomSlider->setRange(68, 172);
    m_zoomSlider->setValue(104);
    m_zoomSlider->setMinimumWidth(110);
    zoomRow->addWidget(m_zoomSlider, 1);
    controlsGrid->addLayout(zoomRow, 1, 4, 1, 2);

    m_countLabel = new QLabel(central);
    m_countLabel->setMinimumWidth(110);
    m_countLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    controlsGrid->addWidget(m_countLabel, 1, 6);

    controlsGrid->setColumnStretch(1, 1);
    controlsGrid->setColumnStretch(3, 3);

    rootLayout->addLayout(controlsGrid);

    // Main Splitter: Glyph Grid + Inspector
    auto* splitter = new QSplitter(Qt::Horizontal, central);

    m_model = new GlyphListModel(this);
    m_delegate = new GlyphGridDelegate(this);

    m_gridView = new GlyphGridView(splitter);
    m_gridView->setModel(m_model);
    m_gridView->setItemDelegate(m_delegate);
    m_gridView->setViewMode(QListView::IconMode);
    m_gridView->setResizeMode(QListView::Adjust);
    m_gridView->setMovement(QListView::Static);
    m_gridView->setUniformItemSizes(true);
    m_gridView->setSpacing(4);
    m_gridView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_gridView->setMouseTracking(true);
    m_gridView->setEmptyDropEnabled(true);
    m_gridView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_gridView, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (!m_hasOpenFont) return;
        QMenu menu(this);
        menu.addAction(m_copyGlyphsAction);
        menu.addAction(m_pasteGlyphsAction);
        menu.addSeparator();
        menu.addAction(m_moveGlyphsAction);
        menu.addAction(m_importSvgAction);
        menu.exec(m_gridView->viewport()->mapToGlobal(pos));
    });

    m_inspector = new GlyphInspectorPanel(splitter);

    splitter->addWidget(m_gridView);
    splitter->addWidget(m_inspector);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    rootLayout->addWidget(splitter, 1);
    setCentralWidget(central);

    // Validation Dock (bottom)
    m_validationDock = new QDockWidget(tr("Font Validation"), this);
    m_validationDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_validationPanel = new ValidationPanel(m_validationDock);
    m_validationDock->setWidget(m_validationPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_validationDock);
    m_validationDock->hide();

    // Log Dock (bottom, compact)
    auto* logDock = new QDockWidget(tr("FontForge Log"), this);
    logDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_logView = new QPlainTextEdit(logDock);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(1000);
    m_logView->setMaximumHeight(130);
    logDock->setWidget(m_logView);
    addDockWidget(Qt::BottomDockWidgetArea, logDock);
    logDock->hide();

    tabifyDockWidget(m_validationDock, logDock);

    // Status bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setFixedWidth(180);
    m_progressBar->setVisible(false);
    statusBar()->addPermanentWidget(m_progressBar);
    statusBar()->showMessage(tr("Ready — open a font file (.ttf, .otf, .woff2, .sfd, .ufo, .pfb)"));

    // Connections
    connect(m_areaCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onAreaComboChanged);
    connect(m_rangeEdit, &QLineEdit::textChanged, this, &MainWindow::onRangeTextChanged);
    connect(m_unencodedCheck, &QCheckBox::toggled, this, &MainWindow::onIncludeUnencodedToggled);
    connect(m_addAreaBtn, &QPushButton::clicked, this, &MainWindow::onAddAreaClicked);
    connect(m_removeAreaBtn, &QPushButton::clicked, this, &MainWindow::onRemoveAreaClicked);
    connect(m_saveAreasBtn, &QPushButton::clicked, this, &MainWindow::onSaveAreaConfigClicked);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(m_zoomSlider, &QSlider::valueChanged, this, &MainWindow::onZoomChanged);
    connect(m_gridView, &GlyphGridView::zoomStepsRequested, this,
            &MainWindow::onGridZoomStepsRequested);
    connect(m_gridView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &MainWindow::onGlyphSelectionChanged);
    connect(m_gridView, &GlyphGridView::fontFileDropped, this, &MainWindow::openFontPath);

    connect(m_inspector, &GlyphInspectorPanel::glyphMetadataEditRequested, this,
            [this](const GlyphMetadataUpdate& update) {
                setBusy(true, tr("Updating glyph #%1…").arg(update.gid));
                QMetaObject::invokeMethod(m_worker, "updateGlyphMetadata", Qt::QueuedConnection,
                                          Q_ARG(ffgui::GlyphMetadataUpdate, update));
            });
    connect(m_inspector, &GlyphInspectorPanel::importSvgRequested, this,
            &MainWindow::onImportSvgClicked);
    connect(m_inspector, &GlyphInspectorPanel::editFontMetadataRequested, this,
            &MainWindow::onEditFontMetadataClicked);

    connect(m_validationPanel, &ValidationPanel::validateRequested, this, [this](bool force) {
        if (!m_hasOpenFont) {
            return;
        }
        setBusy(true, tr("Validating font…"));
        QMetaObject::invokeMethod(m_worker, "validateFont", Qt::QueuedConnection,
                                  Q_ARG(bool, force));
    });
    connect(m_validationPanel, &ValidationPanel::glyphSelected, this,
            &MainWindow::onValidationGlyphSelected);
}

void MainWindow::setupMenusAndToolbar() {
    auto* fileMenu = menuBar()->addMenu(tr("&File"));

    m_newFontAction = fileMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/file-plus.svg")),
                                          tr("&New Font…"), QKeySequence::New, this,
                                          &MainWindow::onNewFontClicked);
    m_newFontAction->setIconText(tr("New"));

    m_openAction = fileMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/folder-open.svg")),
                                       tr("&Open Font…"), QKeySequence::Open, this,
                                       &MainWindow::onOpenFontClicked);
    m_openAction->setIconText(tr("Open"));

    m_saveAction = fileMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/save.svg")),
                                       tr("&Save Font"), QKeySequence::Save, this,
                                       &MainWindow::onSaveFontClicked);
    m_saveAction->setIconText(tr("Save"));

    m_saveAsAction = fileMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/save.svg")),
                                         tr("Save Font &As…"),
                                         QKeySequence(QStringLiteral("Ctrl+Shift+S")), this,
                                         &MainWindow::onSaveFontAsClicked);

    m_exportAction = fileMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/file-export.svg")),
                                         tr("&Export Font…"), QKeySequence(QStringLiteral("Ctrl+E")),
                                         this, &MainWindow::onExportFontClicked);
    m_exportAction->setIconText(tr("Export"));

    m_importFontAction = fileMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/font-import.svg")),
                                             tr("&Import Glyphs from Font…"), this,
                                             &MainWindow::onImportFontClicked);
    m_importFontAction->setIconText(tr("From Font"));

    m_closeAction = fileMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/file-x.svg")),
                                        tr("&Close Font"), QKeySequence::Close, this,
                                        &MainWindow::onCloseFontClicked);
    m_closeAction->setIconText(tr("Close"));
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu(tr("&Edit"));
    m_copyGlyphsAction = editMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/copy.svg")),
                                             tr("&Copy Selected Glyph(s)"), QKeySequence::Copy, this,
                                             &MainWindow::onCopyGlyphsClicked);
    m_pasteGlyphsAction = editMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/clipboard.svg")),
                                              tr("&Paste Glyph(s)"), QKeySequence::Paste, this,
                                              &MainWindow::onPasteGlyphsClicked);
    editMenu->addSeparator();
    editMenu->addAction(tr("&Import Preferences…"), this, [this]() {
        ImportFilterSettingsDialog dlg(loadDefaultImportOptions(), this);
        if (dlg.exec() == QDialog::Accepted) {
            saveDefaultImportOptions(dlg.importOptions());
            statusBar()->showMessage(tr("Default import preferences saved"), 4000);
        }
    });
    m_settingsAction = editMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/settings.svg")),
                                           tr("&Settings…"), QKeySequence::Preferences, this,
                                           &MainWindow::onSettingsClicked);
    m_settingsAction->setIconText(tr("Settings"));

    auto* fontMenu = menuBar()->addMenu(tr("F&ont"));
    m_newGlyphAction = fontMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/glyph-plus.svg")),
                                           tr("&New Glyph…"), QKeySequence(QStringLiteral("Ctrl+Shift+N")),
                                           this, &MainWindow::onNewGlyphClicked);
    m_newGlyphAction->setIconText(tr("New Glyph"));

    m_moveGlyphsAction = fontMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/move.svg")),
                                             tr("&Move Selected Glyphs…"),
                                             QKeySequence(QStringLiteral("Ctrl+Shift+M")), this,
                                             &MainWindow::onMoveGlyphsClicked);
    m_moveGlyphsAction->setIconText(tr("Move"));

    m_importSvgAction = fontMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/image-import.svg")),
                                            tr("&Import SVG / PNG into Glyph…"),
                                            QKeySequence(QStringLiteral("Ctrl+I")), this,
                                            &MainWindow::onImportSvgClicked);
    m_importSvgAction->setIconText(tr("Import"));

    m_batchImportAction = fontMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/layers-import.svg")),
                                              tr("&Batch Import Images…"),
                                              QKeySequence(QStringLiteral("Ctrl+Shift+B")), this,
                                              &MainWindow::onBatchImportClicked);
    m_batchImportAction->setIconText(tr("Batch"));

    m_fontMetaAction = fontMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/sliders.svg")),
                                           tr("Font &Metadata…"),
                                           QKeySequence(QStringLiteral("Ctrl+M")), this,
                                           &MainWindow::onEditFontMetadataClicked);
    m_fontMetaAction->setIconText(tr("Metadata"));

    m_validateAction = fontMenu->addAction(loadTintedSvgIcon(QStringLiteral(":/icons/shield-check.svg")),
                                           tr("&Validate Font…"),
                                           QKeySequence(QStringLiteral("Ctrl+Shift+V")), this,
                                           &MainWindow::onValidateFontClicked);
    m_validateAction->setIconText(tr("Validate"));

    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("Zoom &In"), QKeySequence::ZoomIn, this,
                        [this]() { onGridZoomStepsRequested(1); });
    viewMenu->addAction(tr("Zoom &Out"), QKeySequence::ZoomOut, this,
                        [this]() { onGridZoomStepsRequested(-1); });
    viewMenu->addAction(tr("&Reset Zoom"), QKeySequence(QStringLiteral("Ctrl+0")), this,
                        [this]() { m_zoomSlider->setValue(104); });
    viewMenu->addSeparator();
    for (QDockWidget* dock : findChildren<QDockWidget*>()) {
        viewMenu->addAction(dock->toggleViewAction());
    }

    m_mainToolBar = addToolBar(tr("Main Toolbar"));
    m_mainToolBar->setMovable(false);
    m_mainToolBar->setIconSize(QSize(18, 18));
    m_mainToolBar->addAction(m_newFontAction);
    m_mainToolBar->addAction(m_openAction);
    m_mainToolBar->addAction(m_saveAction);
    m_mainToolBar->addAction(m_exportAction);
    m_mainToolBar->addAction(m_closeAction);
    m_mainToolBar->addSeparator();
    m_mainToolBar->addAction(m_newGlyphAction);
    m_mainToolBar->addAction(m_moveGlyphsAction);
    m_mainToolBar->addAction(m_importSvgAction);
    m_mainToolBar->addAction(m_batchImportAction);
    m_mainToolBar->addAction(m_importFontAction);
    m_mainToolBar->addAction(m_fontMetaAction);
    m_mainToolBar->addAction(m_validateAction);

    auto* toolbarSpacer = new QWidget(m_mainToolBar);
    toolbarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_mainToolBar->addWidget(toolbarSpacer);
    m_mainToolBar->addAction(m_settingsAction);

    applyToolbarDisplayMode(loadToolbarDisplayMode());

    m_saveAction->setEnabled(false);
    m_saveAsAction->setEnabled(false);
    m_exportAction->setEnabled(false);
    m_closeAction->setEnabled(false);
    m_newGlyphAction->setEnabled(false);
    m_moveGlyphsAction->setEnabled(false);
    m_importSvgAction->setEnabled(false);
    m_batchImportAction->setEnabled(false);
    m_importFontAction->setEnabled(false);
    m_copyGlyphsAction->setEnabled(false);
    m_pasteGlyphsAction->setEnabled(false);
    m_fontMetaAction->setEnabled(false);
    m_validateAction->setEnabled(false);
    m_saveAreasBtn->setEnabled(false);
}

void MainWindow::openFontPath(const QString& path) {
    if (path.isEmpty()) {
        return;
    }
    saveLastFontDirectory(path);
    setBusy(true, tr("Loading %1…").arg(QFileInfo(path).fileName()));
    QMetaObject::invokeMethod(m_worker, "openFont", Qt::QueuedConnection, Q_ARG(QString, path));
}

void MainWindow::createNewFont(const ffgui::NewFontOptions& options) {
    setBusy(true, tr("Creating new font %1…").arg(options.fontName));
    QMetaObject::invokeMethod(m_worker, "createNewFont", Qt::QueuedConnection,
                              Q_ARG(ffgui::NewFontOptions, options));
}

void MainWindow::openBatchImport(const QStringList& files, const QString& dir,
                                 const QString& jsonDef) {
    if (!m_hasOpenFont) {
        return;
    }
    BatchImportDialog dlg(m_model->fontSummary(), m_model->allGlyphs(), this);
    if (!jsonDef.isEmpty()) {
        dlg.setInitialDefinitionFile(jsonDef);
    } else if (!dir.isEmpty()) {
        dlg.setInitialDirectory(dir);
    } else if (!files.isEmpty()) {
        dlg.setInitialFiles(files);
    }
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const BatchImportJob job = dlg.batchJob();
    setBusy(true, tr("Batch importing %1 item(s)…").arg(job.items.size()));
    QMetaObject::invokeMethod(m_worker, "batchImport", Qt::QueuedConnection,
                              Q_ARG(ffgui::BatchImportJob, job));
}

void MainWindow::onNewFontClicked() {
    NewFontDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    createNewFont(dlg.fontOptions());
}

void MainWindow::onOpenFontClicked() {
    const QString filter =
        tr("All Supported Fonts (*.ttf *.otf *.woff *.woff2 *.sfd *.pfb *.pfa *.ufo *.svg *.ttc);;"
           "OpenType / TrueType (*.otf *.ttf *.ttc);;Web Fonts (*.woff *.woff2);;"
           "FontForge Project (*.sfd);;All Files (*)");
    const QString defaultDir = loadLastFontDirectory();
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Font"), defaultDir, filter);
    if (!path.isEmpty()) {
        openFontPath(path);
    }
}

void MainWindow::onSaveFontClicked() {
    if (!m_hasOpenFont) {
        return;
    }
    if (m_currentFontPath.isEmpty()) {
        onSaveFontAsClicked();
        return;
    }

    const QFileInfo fi(m_currentFontPath);
    const QString ext = fi.suffix().toLower();
    static const QStringList kDirectWritableExts = {
        QStringLiteral("sfd"),   QStringLiteral("ttf"),  QStringLiteral("otf"),
        QStringLiteral("woff"),  QStringLiteral("woff2"), QStringLiteral("pfb"),
        QStringLiteral("pfa"),   QStringLiteral("ufo"),  QStringLiteral("svg"),
    };

    if (!kDirectWritableExts.contains(ext)) {
        onSaveFontAsClicked();
        return;
    }

    ExportOptions opts;
    opts.outputPath = m_currentFontPath;
    opts.roundToInt = true;
    opts.opentypeTables = true;
    opts.omitHints = false;
    opts.dummyDsig = true;
    opts.generateAfm = false;
    opts.isSaveAs = false;

    m_savingInPlace = true;
    m_savingAs = false;
    setBusy(true, tr("Saving %1…").arg(fi.fileName()));
    QMetaObject::invokeMethod(m_worker, "exportFont", Qt::QueuedConnection,
                              Q_ARG(ffgui::ExportOptions, opts));
}

void MainWindow::onSaveFontAsClicked() {
    if (!m_hasOpenFont) {
        return;
    }
    ExportFontDialog dlg(m_model->fontSummary(), true, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    ExportOptions opts = dlg.exportOptions();
    opts.isSaveAs = true;
    m_savingInPlace = false;
    m_savingAs = true;
    setBusy(true, tr("Saving %1…").arg(QFileInfo(opts.outputPath).fileName()));
    QMetaObject::invokeMethod(m_worker, "exportFont", Qt::QueuedConnection,
                              Q_ARG(ffgui::ExportOptions, opts));
}

void MainWindow::onExportFontClicked() {
    if (!m_hasOpenFont) {
        return;
    }
    ExportFontDialog dlg(m_model->fontSummary(), false, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    ExportOptions opts = dlg.exportOptions();
    const bool isInitialSaveForNewFont = m_currentFontPath.isEmpty();
    opts.isSaveAs = isInitialSaveForNewFont;
    m_savingInPlace = false;
    m_savingAs = isInitialSaveForNewFont;
    setBusy(true, tr("Exporting %1…").arg(QFileInfo(opts.outputPath).fileName()));
    QMetaObject::invokeMethod(m_worker, "exportFont", Qt::QueuedConnection,
                              Q_ARG(ffgui::ExportOptions, opts));
}

void MainWindow::onCloseFontClicked() {
    if (!m_hasOpenFont) {
        return;
    }
    persistAreaConfigIfOpen(false);
    QMetaObject::invokeMethod(m_worker, "closeFont", Qt::QueuedConnection);
}

void MainWindow::onEditFontMetadataClicked() {
    if (!m_hasOpenFont) {
        return;
    }
    FontMetadataDialog dlg(m_model->fontSummary(), this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const FontMetadataUpdate update = dlg.metadataUpdate();
    setBusy(true, tr("Updating font metadata…"));
    QMetaObject::invokeMethod(m_worker, "updateFontMetadata", Qt::QueuedConnection,
                              Q_ARG(ffgui::FontMetadataUpdate, update));
}

void MainWindow::onValidateFontClicked() {
    if (!m_hasOpenFont) {
        return;
    }
    m_validationDock->show();
    m_validationDock->raise();
    setBusy(true, tr("Validating font…"));
    QMetaObject::invokeMethod(m_worker, "validateFont", Qt::QueuedConnection, Q_ARG(bool, true));
}

void MainWindow::onNewGlyphClicked() {
    if (!m_hasOpenFont) {
        return;
    }
    CreateGlyphDialog dlg(m_model->fontSummary(), this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    m_openSvgAfterCreate = dlg.openSvgImportAfterCreate();
    const CreateGlyphRequest req = dlg.glyphRequest();
    setBusy(true, tr("Creating new glyph…"));
    QMetaObject::invokeMethod(m_worker, "createGlyph", Qt::QueuedConnection,
                              Q_ARG(ffgui::CreateGlyphRequest, req));
}

void MainWindow::onMoveGlyphsClicked() {
    if (!m_hasOpenFont) {
        return;
    }
    const QVector<GlyphSummary> sel = currentSelectedGlyphs();
    if (sel.isEmpty()) {
        QMessageBox::information(this, tr("Move Glyphs"),
                                 tr("Please select one or more glyphs in the grid first."));
        return;
    }
    MoveGlyphsDialog dlg(sel, m_model->fontSummary(), m_model->allGlyphs(), this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const MoveGlyphsRequest req = dlg.moveRequest();
    setBusy(true, tr("Moving %1 glyph(s) to U+%2…")
                      .arg(req.gids.size())
                      .arg(req.targetStartUnicode, 4, 16, QLatin1Char('0'))
                      .toUpper());
    QMetaObject::invokeMethod(m_worker, "moveGlyphs", Qt::QueuedConnection,
                              Q_ARG(ffgui::MoveGlyphsRequest, req));
}

void MainWindow::onBatchImportClicked() {
    openBatchImport();
}

void MainWindow::onImportSvgClicked() {
    if (!m_hasOpenFont) {
        return;
    }
    const GlyphSummary* glyph = currentSelectedGlyph();
    if (glyph == nullptr) {
        QMessageBox::information(this, tr("Import SVG"),
                                 tr("Select a glyph in the grid before importing an SVG."));
        return;
    }
    ImportSvgDialog dlg(*glyph, m_model->fontSummary(), QString(), this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const SvgImportOptions opts = dlg.importOptions();
    setBusy(true, tr("Importing SVG into %1 (#%2)…").arg(glyph->name).arg(glyph->gid));
    QMetaObject::invokeMethod(m_worker, "importSvgToGlyph", Qt::QueuedConnection,
                              Q_ARG(ffgui::SvgImportOptions, opts));
}

void MainWindow::onCopyGlyphsClicked() {
    if (!m_hasOpenFont) {
        return;
    }
    const QVector<GlyphSummary> sel = currentSelectedGlyphs();
    if (sel.isEmpty()) {
        return;
    }
    const FontSummary& s = m_model->fontSummary();
    QMimeData* mime = serializeGlyphsToMimeData(sel, s.ascent, s.descent);
    QGuiApplication::clipboard()->setMimeData(mime);
    statusBar()->showMessage(tr("Copied %1 glyph(s) to clipboard").arg(sel.size()), 4000);
}

void MainWindow::onPasteGlyphsClicked() {
    if (!m_hasOpenFont) {
        return;
    }
    const QMimeData* mime = QGuiApplication::clipboard()->mimeData();
    if (!canPasteGlyphs(mime)) {
        QMessageBox::information(this, tr("Paste Glyphs"),
                                 tr("Clipboard does not contain valid glyph data or SVG."));
        return;
    }

    QVector<GlyphSummary> glyphs = deserializeGlyphsFromMimeData(mime);
    if (glyphs.isEmpty()) {
        const QByteArray svg = getSvgFromMimeForGlyph(mime, 0);
        if (!svg.isEmpty()) {
            GlyphSummary g;
            g.name = QStringLiteral("pasted_glyph");
            g.svgData = svg;
            glyphs.append(g);
        }
    }

    if (glyphs.isEmpty()) {
        QMessageBox::warning(this, tr("Paste Glyphs"), tr("Could not extract glyph data from clipboard."));
        return;
    }

    int targetStart = -1;
    const GlyphSummary* sel = currentSelectedGlyph();
    if (sel && sel->unicode >= 0) {
        targetStart = sel->unicode;
    } else {
        int maxCp = 0xE000;
        for (const auto& g : m_model->allGlyphs()) {
            if (g.unicode >= maxCp) {
                maxCp = g.unicode + 1;
            }
        }
        targetStart = maxCp;
    }

    constexpr bool overwrite = true;
    setBusy(true, tr("Pasting %1 glyph(s) at U+%2…")
                      .arg(glyphs.size())
                      .arg(targetStart, 4, 16, QLatin1Char('0'))
                      .toUpper());
    QMetaObject::invokeMethod(m_worker, "pasteGlyphs", Qt::QueuedConnection,
                              Q_ARG(QVector<ffgui::GlyphSummary>, glyphs),
                              Q_ARG(int32_t, static_cast<int32_t>(targetStart)),
                              Q_ARG(bool, overwrite));
}

void MainWindow::onImportFontClicked() {
    if (!m_hasOpenFont) {
        return;
    }
    const FontDocumentSnapshot snap{m_model->fontSummary(), m_model->allGlyphs()};
    ImportFontDialog dlg(snap, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const FontMergeOptions opts = dlg.mergeOptions();
    setBusy(true, tr("Importing glyphs from %1…").arg(QFileInfo(opts.sourceFontPath).fileName()));
    QMetaObject::invokeMethod(m_worker, "mergeFont", Qt::QueuedConnection,
                              Q_ARG(ffgui::FontMergeOptions, opts));
}

void MainWindow::onFontLoaded(const FontDocumentSnapshot& snapshot) {
    setBusy(false);
    m_hasOpenFont = true;
    m_modified = false;
    m_currentFontPath = snapshot.summary.filePath;
    m_gridView->setEmptyDropEnabled(false);

    QString loadErr;
    const bool hadSidecar =
        CustomGlyphAreaConfig::loadForFont(m_currentFontPath, &m_areaConfig, &loadErr);
    if (!loadErr.isEmpty()) {
        onLogMessage(QStringLiteral("warning"), loadErr);
    }

    syncAreaWidgetsFromConfig();
    applyCurrentAreaFilter();
    m_model->setDocument(snapshot);
    m_inspector->setFontSummary(snapshot.summary);
    m_inspector->setGlyph(nullptr);
    m_validationPanel->setFontAvailable(true);
    m_validationPanel->clear();

    m_saveAction->setEnabled(true);
    m_saveAsAction->setEnabled(true);
    m_exportAction->setEnabled(true);
    m_closeAction->setEnabled(true);
    m_newGlyphAction->setEnabled(true);
    m_moveGlyphsAction->setEnabled(true);
    m_batchImportAction->setEnabled(true);
    m_importFontAction->setEnabled(true);
    m_pasteGlyphsAction->setEnabled(true);
    m_copyGlyphsAction->setEnabled(m_gridView->selectionModel() && !m_gridView->selectionModel()->selectedIndexes().isEmpty());
    m_fontMetaAction->setEnabled(true);
    m_validateAction->setEnabled(true);
    m_saveAreasBtn->setEnabled(true);

    if (m_model->rowCount() > 0) {
        m_gridView->setCurrentIndex(m_model->index(0, 0));
    }

    updateWindowTitleAndCounts();
    statusBar()->showMessage(
        hadSidecar
            ? tr("Loaded %1 (restored Custom Glyph Areas from .ffgui.json)")
                  .arg(QFileInfo(m_currentFontPath).fileName())
            : tr("Loaded %1 (%2 glyphs)")
                  .arg(QFileInfo(m_currentFontPath).fileName())
                  .arg(snapshot.glyphs.size()),
        6000);
}

void MainWindow::onFontLoadFailed(const QString& error) {
    setBusy(false);
    QMessageBox::critical(this, tr("Failed to Open Font"), error);
    statusBar()->showMessage(tr("Failed to open font"), 5000);
}

void MainWindow::onFontExported(const QString& outputPath) {
    const bool wasUnsavedNewFont = m_currentFontPath.isEmpty();
    const bool wasSaveAction = m_savingInPlace || m_savingAs || wasUnsavedNewFont ||
        (!m_currentFontPath.isEmpty() &&
         QFileInfo(outputPath).absoluteFilePath() == QFileInfo(m_currentFontPath).absoluteFilePath());
    m_savingInPlace = false;
    m_savingAs = false;
    setBusy(false);

    saveLastFontDirectory(outputPath);

    if (wasSaveAction) {
        m_currentFontPath = outputPath;
        m_modified = false;

        FontSummary updatedSummary = m_model->fontSummary();
        updatedSummary.filePath = outputPath;
        m_model->setFontSummary(updatedSummary);
        m_inspector->setFontSummary(updatedSummary);

        updateWindowTitleAndCounts();
        persistAreaConfigIfOpen(false);
        statusBar()->showMessage(
            tr("Saved %1").arg(QFileInfo(outputPath).fileName()), 5000);
        onLogMessage(QStringLiteral("info"), tr("Saved: %1").arg(outputPath));
    } else {
        statusBar()->showMessage(tr("Exported font to %1").arg(outputPath), 6000);
        onLogMessage(QStringLiteral("info"), tr("Exported: %1").arg(outputPath));
    }
}

void MainWindow::onFontExportFailed(const QString& error) {
    const bool wasSaveAction = m_savingInPlace || m_savingAs;
    m_savingInPlace = false;
    m_savingAs = false;
    setBusy(false);
    QMessageBox::critical(this, wasSaveAction ? tr("Save Failed") : tr("Export Failed"), error);
    statusBar()->showMessage(wasSaveAction ? tr("Font save failed") : tr("Font export failed"), 5000);
}

void MainWindow::onGlyphCreated(const CreateGlyphResult& result) {
    setBusy(false);
    m_modified = true;
    const bool openSvg = m_openSvgAfterCreate;
    m_openSvgAfterCreate = false;

    m_model->addGlyph(result.glyph, result.fontSummary);
    m_inspector->setFontSummary(result.fontSummary);

    const int row = m_model->rowForGid(result.glyph.gid);
    if (row >= 0) {
        const QModelIndex idx = m_model->index(row, 0);
        m_gridView->setCurrentIndex(idx);
        m_gridView->scrollTo(idx);
        m_inspector->setGlyph(m_model->glyphAtRow(row));
        statusBar()->showMessage(
            tr("Created glyph #%1 (%2)").arg(result.glyph.gid).arg(result.glyph.name), 5000);
    } else {
        if (m_gridView->selectionModel()) {
            m_gridView->selectionModel()->clearSelection();
        }
        m_inspector->setGlyph(&result.glyph);
        statusBar()->showMessage(
            tr("Created glyph #%1 (%2) — hidden by active Glyph Area filter")
                .arg(result.glyph.gid)
                .arg(result.glyph.name),
            6000);
    }

    m_importSvgAction->setEnabled(m_hasOpenFont && currentSelectedGlyph() != nullptr);
    updateWindowTitleAndCounts();

    if (openSvg) {
        onImportSvgClicked();
    }
}

void MainWindow::onGlyphCreationFailed(const QString& error) {
    m_openSvgAfterCreate = false;
    setBusy(false);
    QMessageBox::warning(this, tr("Create Glyph Failed"), error);
    statusBar()->showMessage(tr("Glyph creation failed"), 4000);
}

void MainWindow::onGlyphsMoved(const ffgui::MoveGlyphsResult& result) {
    setBusy(false);
    m_modified = true;
    if (!result.snapshot.glyphs.isEmpty()) {
        m_model->setDocument(result.snapshot);
        m_inspector->setFontSummary(result.snapshot.summary);
    } else {
        for (const auto& g : result.updatedGlyphs) {
            m_model->updateGlyph(g);
        }
    }
    onGlyphSelectionChanged();
    updateWindowTitleAndCounts();
    statusBar()->showMessage(tr("Successfully moved %1 glyph(s)").arg(result.movedCount), 5000);
}

void MainWindow::onGlyphsMoveFailed(const QString& error) {
    setBusy(false);
    QMessageBox::warning(this, tr("Move Glyphs Failed"), error);
    statusBar()->showMessage(tr("Move glyphs failed"), 4000);
}

void MainWindow::onBatchImportCompleted(const ffgui::BatchImportResult& result) {
    setBusy(false);
    m_modified = true;
    if (!result.snapshot.glyphs.isEmpty()) {
        m_model->setDocument(result.snapshot);
        m_inspector->setFontSummary(result.snapshot.summary);
    } else {
        for (const auto& g : result.updatedGlyphs) {
            m_model->updateGlyph(g);
        }
        m_model->setFontSummary(result.updatedFontSummary);
        m_inspector->setFontSummary(result.updatedFontSummary);
    }
    onGlyphSelectionChanged();
    updateWindowTitleAndCounts();

    if (!result.errors.isEmpty()) {
        QMessageBox::warning(
            this, tr("Batch Import Completed with Warnings"),
            tr("Imported %1 file(s). %2 file(s) skipped:\n%3")
                .arg(result.importedCount)
                .arg(result.skippedCount)
                .arg(result.errors.join(QStringLiteral("\n"))));
    } else {
        statusBar()->showMessage(tr("Batch imported %1 file(s) successfully").arg(result.importedCount), 6000);
    }
}

void MainWindow::onBatchImportFailed(const QString& error) {
    setBusy(false);
    QMessageBox::warning(this, tr("Batch Import Failed"), error);
    statusBar()->showMessage(tr("Batch import failed"), 4000);
}

void MainWindow::onGlyphMetadataUpdated(const GlyphSummary& glyph) {
    setBusy(false);
    applyUpdatedGlyph(glyph);
    statusBar()->showMessage(
        tr("Updated glyph #%1 (%2)").arg(glyph.gid).arg(glyph.name), 4000);
}

void MainWindow::onGlyphMetadataUpdateFailed(const QString& error) {
    setBusy(false);
    QMessageBox::warning(this, tr("Update Glyph Metadata Failed"), error);
    statusBar()->showMessage(tr("Glyph metadata update failed"), 4000);
}

void MainWindow::onFontMetadataUpdated(const FontSummary& summary) {
    setBusy(false);
    m_modified = true;
    m_model->setFontSummary(summary);
    m_inspector->setFontSummary(summary);
    onGlyphSelectionChanged();
    updateWindowTitleAndCounts();
    statusBar()->showMessage(tr("Updated font metadata (%1)").arg(summary.fontName), 4000);
}

void MainWindow::onFontMetadataUpdateFailed(const QString& error) {
    setBusy(false);
    QMessageBox::warning(this, tr("Update Font Metadata Failed"), error);
    statusBar()->showMessage(tr("Font metadata update failed"), 4000);
}

void MainWindow::onFontValidated(const FontValidationReport& report) {
    setBusy(false);
    m_validationDock->show();
    m_validationDock->raise();
    m_validationPanel->setReport(report);

    if (report.issueGlyphCount == 0) {
        statusBar()->showMessage(
            tr("Validation passed: 0 issues (%1 glyphs checked)").arg(report.totalGlyphs), 6000);
    } else {
        statusBar()->showMessage(
            tr("Validation finished: %1 glyph(s) with issues").arg(report.issueGlyphCount), 6000);
    }
}

void MainWindow::onFontValidationFailed(const QString& error) {
    setBusy(false);
    QMessageBox::warning(this, tr("Validation Failed"), error);
    statusBar()->showMessage(tr("Validation failed"), 4000);
}

void MainWindow::onSvgImported(const SvgImportResult& result) {
    setBusy(false);
    applyUpdatedGlyph(result.glyph);
    statusBar()->showMessage(
        tr("Imported %1 SVG contour(s) into glyph #%2 (%3)")
            .arg(result.importedContours)
            .arg(result.glyph.gid)
            .arg(result.glyph.name),
        5000);
}

void MainWindow::onSvgImportFailed(const QString& error) {
    setBusy(false);
    QMessageBox::warning(this, tr("SVG Import Failed"), error);
    statusBar()->showMessage(tr("SVG import failed"), 4000);
}

void MainWindow::onFontMerged(const FontMergeResult& result) {
    setBusy(false);
    m_modified = true;
    if (!result.snapshot.glyphs.isEmpty()) {
        m_model->setDocument(result.snapshot);
        m_inspector->setFontSummary(result.snapshot.summary);
    }
    onGlyphSelectionChanged();
    updateWindowTitleAndCounts();
    statusBar()->showMessage(tr("Successfully imported %1 glyph(s) from font").arg(result.importedCount), 6000);
}

void MainWindow::onFontMergeFailed(const QString& error) {
    setBusy(false);
    QMessageBox::warning(this, tr("Import From Font Failed"), error);
    statusBar()->showMessage(tr("Font import failed"), 4000);
}

void MainWindow::onGlyphsPasted(const FontMergeResult& result) {
    setBusy(false);
    m_modified = true;
    if (!result.snapshot.glyphs.isEmpty()) {
        m_model->setDocument(result.snapshot);
        m_inspector->setFontSummary(result.snapshot.summary);
    }
    onGlyphSelectionChanged();
    updateWindowTitleAndCounts();
    statusBar()->showMessage(tr("Successfully pasted %1 glyph(s)").arg(result.importedCount), 5000);
}

void MainWindow::onGlyphsPasteFailed(const QString& error) {
    setBusy(false);
    QMessageBox::warning(this, tr("Paste Glyphs Failed"), error);
    statusBar()->showMessage(tr("Paste failed"), 4000);
}

void MainWindow::onFontClosed() {
    m_hasOpenFont = false;
    m_modified = false;
    m_currentFontPath.clear();
    m_model->clear();
    m_gridView->setEmptyDropEnabled(true);
    m_inspector->clear();
    m_validationPanel->setFontAvailable(false);
    m_saveAction->setEnabled(false);
    m_saveAsAction->setEnabled(false);
    m_exportAction->setEnabled(false);
    m_closeAction->setEnabled(false);
    m_newGlyphAction->setEnabled(false);
    m_moveGlyphsAction->setEnabled(false);
    m_importSvgAction->setEnabled(false);
    m_batchImportAction->setEnabled(false);
    m_importFontAction->setEnabled(false);
    m_copyGlyphsAction->setEnabled(false);
    m_pasteGlyphsAction->setEnabled(false);
    m_fontMetaAction->setEnabled(false);
    m_validateAction->setEnabled(false);
    m_saveAreasBtn->setEnabled(false);
    updateWindowTitleAndCounts();
    statusBar()->showMessage(tr("Font closed"), 3000);
}

void MainWindow::onLogMessage(const QString& level, const QString& message) {
    m_logView->appendPlainText(QStringLiteral("[%1] %2").arg(level.toUpper(), message));
}

void MainWindow::onProgressUpdated(int current, int total, const QString& status) {
    if (total > 0 && current < total) {
        m_progressBar->setVisible(true);
        m_progressBar->setRange(0, total);
        m_progressBar->setValue(std::clamp(current, 0, total));
        if (!status.isEmpty()) {
            statusBar()->showMessage(status);
        }
    } else {
        m_progressBar->setVisible(false);
    }
}

void MainWindow::onAreaComboChanged(int index) {
    if (m_updatingAreaUi || index < 0 || index >= m_areaConfig.areas.size()) {
        return;
    }
    m_areaConfig.activeIndex = index;
    syncAreaWidgetsFromConfig();
    applyCurrentAreaFilter();
}

void MainWindow::onRangeTextChanged(const QString& text) {
    if (m_updatingAreaUi) {
        return;
    }
    if (m_areaConfig.activeIndex >= 0 &&
        m_areaConfig.activeIndex < m_areaConfig.areas.size()) {
        m_areaConfig.areas[m_areaConfig.activeIndex].rangeSpec = text;
    }
    applyCurrentAreaFilter();
}

void MainWindow::onIncludeUnencodedToggled(bool checked) {
    if (m_updatingAreaUi) {
        return;
    }
    if (m_areaConfig.activeIndex >= 0 &&
        m_areaConfig.activeIndex < m_areaConfig.areas.size()) {
        m_areaConfig.areas[m_areaConfig.activeIndex].includeUnencoded = checked;
    }
    applyCurrentAreaFilter();
}

void MainWindow::onAddAreaClicked() {
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("New Custom Glyph Area"), tr("Area Name:"), QLineEdit::Normal,
        tr("Custom Area %1").arg(m_areaConfig.areas.size() + 1), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    CustomGlyphArea area;
    area.name = name.trimmed();
    area.rangeSpec = m_rangeEdit->text().trimmed().isEmpty() ? QStringLiteral("*")
                                                             : m_rangeEdit->text().trimmed();
    area.includeUnencoded = m_unencodedCheck->isChecked();
    m_areaConfig.areas.append(area);
    m_areaConfig.activeIndex = m_areaConfig.areas.size() - 1;
    syncAreaWidgetsFromConfig();
    applyCurrentAreaFilter();
    persistAreaConfigIfOpen(true);
}

void MainWindow::onRemoveAreaClicked() {
    if (m_areaConfig.areas.size() <= 1) {
        return;
    }
    const int idx = m_areaConfig.activeIndex;
    if (idx >= 0 && idx < m_areaConfig.areas.size()) {
        m_areaConfig.areas.removeAt(idx);
        m_areaConfig.activeIndex = std::clamp(idx, 0, static_cast<int>(m_areaConfig.areas.size() - 1));
        syncAreaWidgetsFromConfig();
        applyCurrentAreaFilter();
        persistAreaConfigIfOpen(true);
    }
}

void MainWindow::onSaveAreaConfigClicked() {
    persistAreaConfigIfOpen(true);
}

void MainWindow::onSearchTextChanged(const QString& text) {
    m_model->setSearchText(text);
    updateWindowTitleAndCounts();
}

void MainWindow::onZoomChanged(int value) {
    m_delegate->setCellSize(value);
    m_gridView->doItemsLayout();
}

void MainWindow::onGridZoomStepsRequested(int steps) {
    if (steps == 0) {
        return;
    }
    constexpr int kStepSize = 8;
    const int target = std::clamp(m_zoomSlider->value() + steps * kStepSize,
                                  m_zoomSlider->minimum(), m_zoomSlider->maximum());
    m_zoomSlider->setValue(target);
}

void MainWindow::onGlyphSelectionChanged() {
    const GlyphSummary* g = currentSelectedGlyph();
    m_inspector->setGlyph(g);
    m_importSvgAction->setEnabled(m_hasOpenFont && g != nullptr);
    const bool hasSelection = !currentSelectedGlyphs().isEmpty();
    m_moveGlyphsAction->setEnabled(m_hasOpenFont && hasSelection);
    m_copyGlyphsAction->setEnabled(m_hasOpenFont && hasSelection);
}

void MainWindow::onValidationGlyphSelected(int gid) {
    const int row = m_model->rowForGid(gid);
    if (row >= 0) {
        const QModelIndex idx = m_model->index(row, 0);
        m_gridView->setCurrentIndex(idx);
        m_gridView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        return;
    }

    statusBar()->showMessage(
        tr("Glyph #%1 is hidden by the active Custom Glyph Area or search filter").arg(gid),
        5000);
}

void MainWindow::setBusy(bool busy, const QString& message) {
    m_openAction->setEnabled(!busy);
    m_newFontAction->setEnabled(!busy);
    m_saveAction->setEnabled(!busy && m_hasOpenFont);
    m_saveAsAction->setEnabled(!busy && m_hasOpenFont);
    m_exportAction->setEnabled(!busy && m_hasOpenFont);
    m_closeAction->setEnabled(!busy && m_hasOpenFont);
    m_newGlyphAction->setEnabled(!busy && m_hasOpenFont);
    m_moveGlyphsAction->setEnabled(!busy && m_hasOpenFont && !currentSelectedGlyphs().isEmpty());
    m_batchImportAction->setEnabled(!busy && m_hasOpenFont);
    m_importFontAction->setEnabled(!busy && m_hasOpenFont);
    m_copyGlyphsAction->setEnabled(!busy && m_hasOpenFont && !currentSelectedGlyphs().isEmpty());
    m_pasteGlyphsAction->setEnabled(!busy && m_hasOpenFont);
    m_importSvgAction->setEnabled(!busy && m_hasOpenFont && currentSelectedGlyph() != nullptr);
    m_fontMetaAction->setEnabled(!busy && m_hasOpenFont);
    m_validateAction->setEnabled(!busy && m_hasOpenFont);
    if (busy) {
        m_progressBar->setVisible(true);
        m_progressBar->setRange(0, 0);
        if (!message.isEmpty()) {
            statusBar()->showMessage(message);
        }
    } else {
        m_progressBar->setVisible(false);
    }
}

void MainWindow::syncAreaWidgetsFromConfig() {
    m_updatingAreaUi = true;
    m_areaCombo->clear();
    for (const CustomGlyphArea& area : m_areaConfig.areas) {
        m_areaCombo->addItem(area.name);
    }
    const int idx = std::clamp(m_areaConfig.activeIndex, 0,
                               std::max(0, static_cast<int>(m_areaConfig.areas.size() - 1)));
    m_areaConfig.activeIndex = idx;
    if (idx >= 0 && idx < m_areaConfig.areas.size()) {
        m_areaCombo->setCurrentIndex(idx);
        m_rangeEdit->setText(m_areaConfig.areas[idx].rangeSpec);
        m_unencodedCheck->setChecked(m_areaConfig.areas[idx].includeUnencoded);
    }
    m_removeAreaBtn->setEnabled(m_areaConfig.areas.size() > 1);
    m_updatingAreaUi = false;
}

void MainWindow::applyCurrentAreaFilter() {
    QString parseErr;
    const UnicodeRangeFilter filter = UnicodeRangeFilter::parse(
        m_rangeEdit->text(), m_unencodedCheck->isChecked(), &parseErr);

    if (filter.isValid()) {
        m_rangeEdit->setStyleSheet(QString());
        m_rangeStatusLabel->clear();
        m_rangeStatusLabel->setToolTip(QString());
        m_rangeStatusLabel->setVisible(false);
        m_model->setRangeFilter(filter);
    } else {
        m_rangeEdit->setStyleSheet(QStringLiteral("QLineEdit { border: 1px solid #d9534f; }"));
        m_rangeStatusLabel->setPixmap(
            loadTintedSvgPixmap(QStringLiteral(":/icons/alert-triangle.svg"), 16,
                                QColor(QStringLiteral("#d97706"))));
        m_rangeStatusLabel->setToolTip(parseErr);
        m_rangeStatusLabel->setVisible(true);
    }
    updateWindowTitleAndCounts();
}

void MainWindow::updateWindowTitleAndCounts() {
    if (!m_hasOpenFont) {
        setWindowTitle(QStringLiteral("FontForgeGUI v0.5"));
        m_countLabel->setText(tr("0 glyphs"));
        return;
    }

    const FontSummary& s = m_model->fontSummary();
    const QString label = !s.fullName.isEmpty() ? s.fullName
                          : (!s.fontName.isEmpty() ? s.fontName
                                                   : QFileInfo(m_currentFontPath).fileName());
    const QString dirtyPrefix = m_modified ? QStringLiteral("* ") : QString();
    setWindowTitle(QStringLiteral("%1%2 — FontForgeGUI v0.5").arg(dirtyPrefix, label));
    m_countLabel->setText(
        tr("Showing %1 / %2 glyphs").arg(m_model->filteredGlyphCount()).arg(m_model->totalGlyphCount()));
}

void MainWindow::applyUpdatedGlyph(const GlyphSummary& glyph) {
    m_modified = true;
    m_model->updateGlyph(glyph);

    const int row = m_model->rowForGid(glyph.gid);
    if (row >= 0) {
        const QModelIndex idx = m_model->index(row, 0);
        m_gridView->setCurrentIndex(idx);
        m_inspector->setGlyph(m_model->glyphAtRow(row));
    } else {
        m_inspector->setGlyph(&glyph);
    }
    m_importSvgAction->setEnabled(m_hasOpenFont && currentSelectedGlyph() != nullptr);
    updateWindowTitleAndCounts();
}

const GlyphSummary* MainWindow::currentSelectedGlyph() const {
    if (m_gridView && m_gridView->selectionModel()) {
        const QModelIndexList selected = m_gridView->selectionModel()->selectedIndexes();
        if (!selected.isEmpty()) {
            return m_model->glyphAtRow(selected.first().row());
        }
    }
    return m_inspector ? m_inspector->currentGlyph() : nullptr;
}

QVector<GlyphSummary> MainWindow::currentSelectedGlyphs() const {
    QVector<GlyphSummary> out;
    if (m_gridView && m_gridView->selectionModel()) {
        const QModelIndexList selected = m_gridView->selectionModel()->selectedIndexes();
        for (const auto& idx : selected) {
            const GlyphSummary* g = m_model->glyphAtRow(idx.row());
            if (g != nullptr) {
                out.append(*g);
            }
        }
    }
    return out;
}

void MainWindow::persistAreaConfigIfOpen(bool showStatus) {
    if (!m_hasOpenFont || m_currentFontPath.isEmpty()) {
        return;
    }
    QString err;
    if (m_areaConfig.saveForFont(m_currentFontPath, &err)) {
        if (showStatus) {
            statusBar()->showMessage(
                tr("Saved Custom Glyph Areas to %1")
                    .arg(QFileInfo(CustomGlyphAreaConfig::sidecarPathForFont(m_currentFontPath))
                             .fileName()),
                4000);
        }
    } else if (showStatus) {
        QMessageBox::warning(this, tr("Save Areas Failed"), err);
    }
}

void MainWindow::applyToolbarDisplayMode(ToolbarDisplayMode mode) {
    if (!m_mainToolBar) {
        return;
    }
    m_mainToolBar->setToolButtonStyle(toQtToolButtonStyle(mode));
}

void MainWindow::onSettingsClicked() {
    const ToolbarDisplayMode previousMode = loadToolbarDisplayMode();
    SettingsDialog dlg(previousMode, this);
    connect(&dlg, &SettingsDialog::toolbarDisplayModePreview, this,
            &MainWindow::applyToolbarDisplayMode);
    connect(&dlg, &SettingsDialog::openImportPreferencesRequested, this, [&dlg, this]() {
        ImportFilterSettingsDialog importDlg(loadDefaultImportOptions(), &dlg);
        if (importDlg.exec() == QDialog::Accepted) {
            saveDefaultImportOptions(importDlg.importOptions());
            statusBar()->showMessage(tr("Default import preferences saved"), 4000);
        }
    });

    if (dlg.exec() == QDialog::Accepted) {
        const ToolbarDisplayMode chosenMode = dlg.toolbarDisplayMode();
        saveToolbarDisplayMode(chosenMode);
        applyToolbarDisplayMode(chosenMode);
        statusBar()->showMessage(tr("Settings saved"), 3000);
    } else {
        applyToolbarDisplayMode(previousMode);
    }
}

} // namespace ffgui
