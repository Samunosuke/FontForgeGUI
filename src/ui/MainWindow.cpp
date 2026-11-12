#include "ui/MainWindow.hpp"

#include "backend/FontWorker.hpp"
#include "model/FontSettingsUtils.hpp"
#include "model/GlyphListModel.hpp"
#include "model/UnicodeRangeFilter.hpp"
#include "ui/BatchImportDialog.hpp"
#include "ui/CreateGlyphDialog.hpp"
#include "ui/ExportFontDialog.hpp"
#include "ui/FontMetadataDialog.hpp"
#include "ui/ImportFilterSettingsDialog.hpp"
#include "ui/GlyphGridDelegate.hpp"
#include "ui/GlyphGridView.hpp"
#include "ui/GlyphInspectorPanel.hpp"
#include "ui/ImportSvgDialog.hpp"
#include "ui/MoveGlyphsDialog.hpp"
#include "ui/NewFontDialog.hpp"
#include "ui/ValidationPanel.hpp"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
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

    // Custom Glyph Area bar
    auto* areaBar = new QHBoxLayout();
    areaBar->setSpacing(6);

    areaBar->addWidget(new QLabel(tr("Custom Glyph Area:"), central));

    m_areaCombo = new QComboBox(central);
    m_areaCombo->setMinimumWidth(150);
    areaBar->addWidget(m_areaCombo);

    m_addAreaBtn = new QPushButton(QStringLiteral("+"), central);
    m_addAreaBtn->setToolTip(tr("Create a new Custom Glyph Area"));
    m_addAreaBtn->setFixedWidth(28);
    areaBar->addWidget(m_addAreaBtn);

    m_removeAreaBtn = new QPushButton(QStringLiteral("−"), central);
    m_removeAreaBtn->setToolTip(tr("Remove current Custom Glyph Area"));
    m_removeAreaBtn->setFixedWidth(28);
    areaBar->addWidget(m_removeAreaBtn);

    areaBar->addWidget(new QLabel(tr("Unicode Ranges:"), central));

    m_rangeEdit = new QLineEdit(central);
    m_rangeEdit->setPlaceholderText(tr("e.g. U+0020-007E, U+E000-E0FF or * for all"));
    m_rangeEdit->setClearButtonEnabled(true);
    areaBar->addWidget(m_rangeEdit, 2);

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
    areaBar->addWidget(m_presetBtn);

    m_unencodedCheck = new QCheckBox(tr("Include unencoded"), central);
    areaBar->addWidget(m_unencodedCheck);

    m_saveAreasBtn = new QPushButton(tr("Save Areas"), central);
    m_saveAreasBtn->setToolTip(tr("Save Custom Glyph Areas to sidecar file (.ffgui.json)"));
    areaBar->addWidget(m_saveAreasBtn);

    m_rangeStatusLabel = new QLabel(central);
    areaBar->addWidget(m_rangeStatusLabel);

    rootLayout->addLayout(areaBar);

    // Search & Zoom bar
    auto* filterBar = new QHBoxLayout();
    filterBar->setSpacing(8);

    filterBar->addWidget(new QLabel(tr("Find Glyph:"), central));
    m_searchEdit = new QLineEdit(central);
    m_searchEdit->setPlaceholderText(tr("Filter by glyph name, Unicode (U+0041 / 0041), or GID (#42)…"));
    m_searchEdit->setClearButtonEnabled(true);
    filterBar->addWidget(m_searchEdit, 1);

    filterBar->addWidget(new QLabel(tr("Cell Size:"), central));
    m_zoomSlider = new QSlider(Qt::Horizontal, central);
    m_zoomSlider->setRange(68, 172);
    m_zoomSlider->setValue(104);
    m_zoomSlider->setFixedWidth(130);
    filterBar->addWidget(m_zoomSlider);

    m_countLabel = new QLabel(central);
    m_countLabel->setMinimumWidth(140);
    m_countLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    filterBar->addWidget(m_countLabel);

    rootLayout->addLayout(filterBar);

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

    m_newFontAction = fileMenu->addAction(tr("&New Font…"), QKeySequence::New, this,
                                         &MainWindow::onNewFontClicked);
    m_openAction = fileMenu->addAction(tr("&Open Font…"), QKeySequence::Open, this,
                                       &MainWindow::onOpenFontClicked);
    m_saveAction = fileMenu->addAction(tr("&Save Font"), QKeySequence::Save, this,
                                       &MainWindow::onSaveFontClicked);
    m_saveAsAction = fileMenu->addAction(tr("Save Font &As…"),
                                         QKeySequence(QStringLiteral("Ctrl+Shift+S")), this,
                                         &MainWindow::onSaveFontAsClicked);
    m_exportAction = fileMenu->addAction(tr("&Export Font…"), QKeySequence(QStringLiteral("Ctrl+E")),
                                         this, &MainWindow::onExportFontClicked);
    m_closeAction = fileMenu->addAction(tr("&Close Font"), QKeySequence::Close, this,
                                        &MainWindow::onCloseFontClicked);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(tr("&Import Preferences…"), this, [this]() {
        ImportFilterSettingsDialog dlg(loadDefaultImportOptions(), this);
        if (dlg.exec() == QDialog::Accepted) {
            saveDefaultImportOptions(dlg.importOptions());
            statusBar()->showMessage(tr("Default import preferences saved"), 4000);
        }
    });

    auto* fontMenu = menuBar()->addMenu(tr("F&ont"));
    m_newGlyphAction = fontMenu->addAction(tr("&New Glyph…"), QKeySequence(QStringLiteral("Ctrl+Shift+N")), this,
                                           &MainWindow::onNewGlyphClicked);
    m_moveGlyphsAction = fontMenu->addAction(tr("&Move Selected Glyphs…"),
                                             QKeySequence(QStringLiteral("Ctrl+Shift+M")), this,
                                             &MainWindow::onMoveGlyphsClicked);
    m_importSvgAction = fontMenu->addAction(tr("&Import SVG / PNG into Glyph…"),
                                            QKeySequence(QStringLiteral("Ctrl+I")), this,
                                            &MainWindow::onImportSvgClicked);
    m_batchImportAction = fontMenu->addAction(tr("&Batch Import Images…"),
                                              QKeySequence(QStringLiteral("Ctrl+Shift+B")), this,
                                              &MainWindow::onBatchImportClicked);
    m_fontMetaAction = fontMenu->addAction(tr("Font &Metadata…"),
                                           QKeySequence(QStringLiteral("Ctrl+M")), this,
                                           &MainWindow::onEditFontMetadataClicked);
    m_validateAction = fontMenu->addAction(tr("&Validate Font…"),
                                           QKeySequence(QStringLiteral("Ctrl+Shift+V")), this,
                                           &MainWindow::onValidateFontClicked);

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

    auto* toolbar = addToolBar(tr("Main Toolbar"));
    toolbar->setMovable(false);
    toolbar->addAction(m_newFontAction);
    toolbar->addAction(m_openAction);
    toolbar->addAction(m_saveAction);
    toolbar->addAction(m_exportAction);
    toolbar->addAction(m_closeAction);
    toolbar->addSeparator();
    toolbar->addAction(m_newGlyphAction);
    toolbar->addAction(m_moveGlyphsAction);
    toolbar->addAction(m_importSvgAction);
    toolbar->addAction(m_batchImportAction);
    toolbar->addAction(m_fontMetaAction);
    toolbar->addAction(m_validateAction);

    m_saveAction->setEnabled(false);
    m_saveAsAction->setEnabled(false);
    m_exportAction->setEnabled(false);
    m_closeAction->setEnabled(false);
    m_newGlyphAction->setEnabled(false);
    m_moveGlyphsAction->setEnabled(false);
    m_importSvgAction->setEnabled(false);
    m_batchImportAction->setEnabled(false);
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
    m_moveGlyphsAction->setEnabled(m_hasOpenFont && !currentSelectedGlyphs().isEmpty());
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
        m_model->setRangeFilter(filter);
    } else {
        m_rangeEdit->setStyleSheet(QStringLiteral("QLineEdit { border: 1px solid #d9534f; }"));
        m_rangeStatusLabel->setText(QStringLiteral("⚠"));
        m_rangeStatusLabel->setToolTip(parseErr);
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

} // namespace ffgui
