#include "ui/MainWindow.h"

#include "services/SessionStateStore.h"
#include "ui/LabelEditDelegates.h"
#include "ui/PreferenceDialog.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QStringList>
#include <QStringView>
#include <QStyle>
#include <QStyleFactory>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>
#include <stdexcept>
#include <utility>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_labelModel(new LabelTableModel(this)), m_labelTextDelegate(new LabelTextDelegate(this)),
      m_labelGroupDelegate(new LabelGroupDelegate(this))
{
    const labelminus::core::AppPreferencesLoadResult preferences =
        labelminus::core::AppPreferences::loadWithDiagnostics();
    m_preferences = preferences.preferences;
    m_preferenceWarnings = preferences.warnings;
    m_labelTableMaxTextRows = m_preferences.labelTableMaxTextRows();
    m_labelEditController =
        std::make_unique<labelminus::services::LabelEditController>(project(), m_undoStack,
                                                                    labelminus::services::LabelEditCommandTexts{
                                                                        tr("Add label"),
                                                                        tr("Edit label text"),
                                                                        tr("Change label group"),
                                                                        tr("Move label"),
                                                                        tr("Delete labels"),
                                                                        tr("Reorder labels"),
                                                                    });
    m_labelEditController->setCallbacks(
        [this](int imageIndex, int labelIndex) { refreshLabelEditSelection(imageIndex, labelIndex); },
        [this](int imageIndex, QVector<int> labelIndexes) {
            refreshLabelEditSelection(imageIndex, std::move(labelIndexes));
        },
        [this](int imageIndex) { clearLabelEditSelection(imageIndex); }, [this]() { markDirty(); });

    setWindowTitle(QStringLiteral("LabelMinus"));
    resize(1200, 800);

    createActions();
    createMenus();
    createCentralWidget();
    applyLabelTableFont();
    applyTextEditorFont();
    setEditorEnabled(false);
    m_warningLabel = new QLabel(this);
    m_warningLabel->setTextFormat(Qt::PlainText);
    m_warningLabel->setStyleSheet(QStringLiteral("QLabel { color: #b26a00; font-weight: 600; }"));
    m_warningLabel->setVisible(false);
    statusBar()->addPermanentWidget(m_warningLabel);
    statusBar()->showMessage(tr("Ready"));
    showPreferenceWarnings();
    restoreLayoutState();
    m_backupTimer = new QTimer(this);
    connect(m_backupTimer, &QTimer::timeout, this, &MainWindow::performAutoBackup);
    configureBackupTimer();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!promptToSaveIfDirty()) {
        event->ignore();
        return;
    }

    saveProjectSessionState();
    saveLayoutState();
    event->accept();
}

void MainWindow::createActions()
{
    m_newProjectAction = new QAction(tr("&New Project..."), this);
    m_newProjectAction->setShortcut(QKeySequence::New);
    connect(m_newProjectAction, &QAction::triggered, this, &MainWindow::newProject);

    m_openProjectAction = new QAction(tr("&Open LabelPlus Text..."), this);
    m_openProjectAction->setShortcut(QKeySequence::Open);
    connect(m_openProjectAction, &QAction::triggered, this, &MainWindow::openProject);

    m_saveProjectAction = new QAction(tr("&Save"), this);
    m_saveProjectAction->setShortcut(QKeySequence::Save);
    connect(m_saveProjectAction, &QAction::triggered, this, &MainWindow::saveProject);

    m_saveProjectAsAction = new QAction(tr("Save &As..."), this);
    m_saveProjectAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveProjectAsAction, &QAction::triggered, this, &MainWindow::saveProjectAs);

    m_undoAction = new QAction(tr("&Undo"), this);
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::undoLastOperation);

    m_redoAction = new QAction(tr("&Redo"), this);
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::redoLastOperation);

    m_preferencesAction = new QAction(tr("&Preferences..."), this);
    m_preferencesAction->setShortcut(QKeySequence::Preferences);
    connect(m_preferencesAction, &QAction::triggered, this, &MainWindow::openPreferences);

    m_quitAction = new QAction(tr("&Quit"), this);
    m_quitAction->setShortcut(QKeySequence::Quit);
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);

    updateEditShortcuts();
}

void MainWindow::createMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_newProjectAction);
    fileMenu->addAction(m_openProjectAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveProjectAction);
    fileMenu->addAction(m_saveProjectAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_preferencesAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAction);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
}

void MainWindow::createCentralWidget()
{
    m_rootSplitter = new QSplitter(Qt::Horizontal, this);
    m_rootSplitter->setChildrenCollapsible(false);

    auto* leftPanel = new QWidget(m_rootSplitter);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(8, 8, 8, 8);
    leftLayout->setSpacing(6);

    m_canvas = new ImageCanvas(leftPanel);
    m_canvas->setPreferences(m_preferences);
    leftLayout->addWidget(m_canvas, 1);

    auto* bottomBar = new QWidget(leftPanel);
    auto* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(6);

    m_zoomSlider = new QSlider(Qt::Horizontal, bottomBar);
    m_zoomSlider->setRange(10, 400);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setFixedWidth(180);
    bottomLayout->addWidget(new QLabel(tr("Zoom"), bottomBar));
    bottomLayout->addWidget(m_zoomSlider);
    bottomLayout->addStretch();

    m_insertGroupComboBox = new QComboBox(bottomBar);
    m_insertGroupComboBox->setMinimumWidth(120);
    auto* addGroupButton = new QToolButton(bottomBar);
    auto* removeGroupButton = new QToolButton(bottomBar);
    addGroupButton->setIcon(
        QIcon::fromTheme(QStringLiteral("document-new"), style()->standardIcon(QStyle::SP_FileIcon)));
    removeGroupButton->setIcon(
        QIcon::fromTheme(QStringLiteral("user-trash"), style()->standardIcon(QStyle::SP_TrashIcon)));
    addGroupButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    removeGroupButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    addGroupButton->setToolTip(tr("Add group"));
    removeGroupButton->setToolTip(tr("Remove selected group"));
    addGroupButton->setAutoRaise(true);
    removeGroupButton->setAutoRaise(true);
    addGroupButton->setFixedSize(24, 24);
    removeGroupButton->setFixedSize(24, 24);
    bottomLayout->addWidget(new QLabel(tr("Insert group"), bottomBar));
    bottomLayout->addWidget(m_insertGroupComboBox);
    bottomLayout->addWidget(addGroupButton);
    bottomLayout->addWidget(removeGroupButton);
    bottomLayout->addStretch();

    m_previousButton = new QPushButton(tr("Previous"), bottomBar);
    m_nextButton = new QPushButton(tr("Next"), bottomBar);
    m_imageComboBox = new QComboBox(bottomBar);
    m_imageComboBox->setMinimumWidth(160);
    bottomLayout->addWidget(m_previousButton);
    bottomLayout->addWidget(m_imageComboBox);
    bottomLayout->addWidget(m_nextButton);
    leftLayout->addWidget(bottomBar);

    auto* rightPanel = new QWidget(m_rootSplitter);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(8, 8, 8, 8);
    rightLayout->setSpacing(6);

    auto* groupBar = new QWidget(rightPanel);
    auto* groupLayout = new QHBoxLayout(groupBar);
    groupLayout->setContentsMargins(0, 0, 0, 0);
    groupLayout->setSpacing(6);
    m_groupFilterComboBox = new GroupFilterComboBox(groupBar);
    groupLayout->addWidget(new QLabel(tr("Group"), groupBar));
    groupLayout->addWidget(m_groupFilterComboBox, 1);
    rightLayout->addWidget(groupBar);

    m_rightSplitter = new QSplitter(Qt::Vertical, rightPanel);
    m_rightSplitter->setChildrenCollapsible(false);

    m_labelView = new QTableView(m_rightSplitter);
    m_defaultLabelTableFont = m_labelView->font();
    m_labelView->setModel(m_labelModel);
    m_labelView->setItemDelegateForColumn(1, m_labelTextDelegate);
    m_labelView->setItemDelegateForColumn(2, m_labelGroupDelegate);
    m_labelView->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_labelView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_labelView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_labelView->setDragEnabled(true);
    m_labelView->setAcceptDrops(true);
    m_labelView->setDropIndicatorShown(true);
    m_labelView->setDragDropMode(QAbstractItemView::InternalMove);
    m_labelView->setDragDropOverwriteMode(false);
    m_labelView->setDefaultDropAction(Qt::MoveAction);
    m_labelView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_labelView->setWordWrap(true);
    m_labelView->verticalHeader()->setVisible(false);
    m_labelView->horizontalHeader()->setStretchLastSection(false);
    m_labelView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_labelView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_labelView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_labelView->setAlternatingRowColors(true);
    auto* labelRowsResizeTimer = new QTimer(m_labelView);
    labelRowsResizeTimer->setSingleShot(true);
    labelRowsResizeTimer->setInterval(0);
    connect(labelRowsResizeTimer, &QTimer::timeout, this, &MainWindow::resizeLabelRowsToContents);
    connect(m_labelView->horizontalHeader(), &QHeaderView::sectionResized, labelRowsResizeTimer,
            [labelRowsResizeTimer]() { labelRowsResizeTimer->start(); });

    auto* editorPanel = new QWidget(m_rightSplitter);
    auto* editorLayout = new QVBoxLayout(editorPanel);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(6);

    auto* labelGroupBar = new QWidget(editorPanel);
    auto* labelGroupLayout = new QHBoxLayout(labelGroupBar);
    labelGroupLayout->setContentsMargins(0, 0, 0, 0);
    labelGroupLayout->setSpacing(6);
    m_labelGroupComboBox = new QComboBox(labelGroupBar);
    labelGroupLayout->addWidget(new QLabel(tr("Label group"), labelGroupBar));
    labelGroupLayout->addWidget(m_labelGroupComboBox, 1);
    editorLayout->addWidget(labelGroupBar);

    m_textEdit = new QPlainTextEdit(editorPanel);
    m_defaultTextEditFont = m_textEdit->font();
    m_textEdit->setPlaceholderText(tr("Selected label text"));
    editorLayout->addWidget(m_textEdit, 1);
    m_rightSplitter->setStretchFactor(0, 3);
    m_rightSplitter->setStretchFactor(1, 1);
    rightLayout->addWidget(m_rightSplitter, 1);

    m_rootSplitter->addWidget(leftPanel);
    m_rootSplitter->addWidget(rightPanel);
    m_rootSplitter->setStretchFactor(0, 3);
    m_rootSplitter->setStretchFactor(1, 2);
    setCentralWidget(m_rootSplitter);

    connect(m_canvas, &ImageCanvas::labelCreateRequested, this, &MainWindow::addLabel);
    connect(m_canvas, &ImageCanvas::labelMoveRequested, this, &MainWindow::moveLabel);
    connect(m_canvas, &ImageCanvas::labelSelected, this, &MainWindow::selectLabel);
    connect(m_canvas, &ImageCanvas::zoomPercentChanged, m_zoomSlider, &QSlider::setValue);
    connect(m_zoomSlider, &QSlider::valueChanged, m_canvas, &ImageCanvas::setZoomPercent);
    connect(m_imageComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::selectImage);
    connect(m_previousButton, &QPushButton::clicked, this,
            [this]() { selectImage(std::max(0, m_currentImageIndex - 1)); });
    connect(m_nextButton, &QPushButton::clicked, this, [this]() {
        const int lastIndex = static_cast<int>(project().images().size()) - 1;
        selectImage(std::min(lastIndex, m_currentImageIndex + 1));
    });
    connect(m_labelView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& current) {
                if (current.isValid()) {
                    selectLabel(m_labelModel->sourceIndexForRow(current.row()));
                }
            });
    auto* deleteLabelShortcut = new QAction(m_labelView);
    deleteLabelShortcut->setShortcut(QKeySequence::Delete);
    deleteLabelShortcut->setShortcutContext(Qt::WidgetShortcut);
    m_labelView->addAction(deleteLabelShortcut);
    connect(deleteLabelShortcut, &QAction::triggered, this, &MainWindow::deleteSelectedLabels);
    connect(m_labelView, &QTableView::customContextMenuRequested, this, &MainWindow::showLabelContextMenu);
    connect(m_textEdit, &QPlainTextEdit::textChanged, this, &MainWindow::updateCurrentLabelText);
    connect(m_labelModel, &LabelTableModel::labelEdited, this, &MainWindow::updateLabelFromTable, Qt::QueuedConnection);
    connect(m_labelModel, &LabelTableModel::labelsReorderRequested, this, &MainWindow::reorderLabels);
    connect(m_labelTextDelegate, &LabelTextDelegate::editorHeightHintChanged, this,
            [this](const QPersistentModelIndex& index, QWidget* editor, int height) {
                if (!index.isValid() || m_labelView == nullptr || index.model() != m_labelModel ||
                    index.column() != 1) {
                    return;
                }

                const int row = index.row();
                if (editor == nullptr || height <= 0) {
                    m_labelView->resizeRowToContents(row);
                    capLabelRowHeight(row);
                    return;
                }

                const int minimumHeight = m_labelView->verticalHeader()->minimumSectionSize();
                m_labelView->setRowHeight(row, std::max(minimumHeight, height));
                editor->setGeometry(m_labelView->visualRect(index));
            });
    connect(m_groupFilterComboBox, &GroupFilterComboBox::selectedGroupsChanged, this, &MainWindow::updateGroupFilter);
    connect(m_labelGroupComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::updateCurrentLabelGroup);
    connect(m_insertGroupComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::updateInsertGroupTextColor);
    connect(addGroupButton, &QToolButton::clicked, this, &MainWindow::addGroup);
    connect(removeGroupButton, &QToolButton::clicked, this, &MainWindow::removeGroup);
}

void MainWindow::newProject()
{
    if (!promptToSaveIfDirty()) {
        return;
    }
    saveProjectSessionState();

    const QString directoryPath =
        QFileDialog::getExistingDirectory(this, tr("Select image folder"), QString(), QFileDialog::ShowDirsOnly);
    if (directoryPath.isEmpty()) {
        return;
    }

    const QString projectBaseName = tr("New Translation");
    labelminus::services::NewProjectResult result = m_projectController.createProjectFromImageDirectory(
        directoryPath, projectBaseName, {QStringLiteral("框内"), QStringLiteral("框外")}, false);
    if (result.status == labelminus::services::NewProjectResult::Status::ProjectFileExists) {
        QMessageBox messageBox(QMessageBox::Question, tr("Project file already exists"),
                               tr("%1 already exists. Create the project with the next available name instead?")
                                   .arg(result.existingFileName),
                               QMessageBox::NoButton, this);
        QPushButton* tryAnotherNameButton = messageBox.addButton(tr("Try another name"), QMessageBox::AcceptRole);
        messageBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
        messageBox.exec();
        if (messageBox.clickedButton() != tryAnotherNameButton) {
            return;
        }

        result = m_projectController.createProjectFromImageDirectory(
            directoryPath, projectBaseName, {QStringLiteral("框内"), QStringLiteral("框外")}, true);
    }

    if (result.status == labelminus::services::NewProjectResult::Status::NoImages) {
        QMessageBox::warning(this, tr("New project failed"), tr("No supported image files were found in this folder."));
        return;
    }
    if (result.status == labelminus::services::NewProjectResult::Status::Failed) {
        QMessageBox::critical(this, tr("New project failed"), result.error);
        return;
    }
    if (result.status == labelminus::services::NewProjectResult::Status::Created &&
        openProjectFile(result.projectPath)) {
        statusBar()->showMessage(tr("Created %1").arg(result.projectPath), 4000);
    }
}

void MainWindow::openProject()
{
    if (!promptToSaveIfDirty()) {
        return;
    }
    saveProjectSessionState();

    const QString path = QFileDialog::getOpenFileName(this, tr("Open LabelPlus text"), QString(),
                                                      tr("LabelPlus text (*.txt);;All files (*)"));

    if (path.isEmpty()) {
        return;
    }

    openProjectFile(path);
}

bool MainWindow::openProjectFile(const QString& path)
{
    try {
        m_projectController.loadFromFile(path);
        m_undoStack.clear();
        refreshProjectUi();
        restoreProjectSessionState();
        statusBar()->showMessage(tr("Loaded %1").arg(path), 4000);
        return true;
    }
    catch (const std::exception& error) {
        QMessageBox::critical(this, tr("Open failed"), QString::fromUtf8(error.what()));
        return false;
    }
}

void MainWindow::openPreferences()
{
    auto* dialog = new PreferenceDialog(labelminus::core::AppPreferences::defaultFilePath(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &PreferenceDialog::preferencesApplied, this, &MainWindow::applyPreferences);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

bool MainWindow::saveProject()
{
    if (project().isEmpty()) {
        return true;
    }

    if (project().filePath().isEmpty()) {
        return saveProjectAs();
    }

    try {
        m_projectController.save();
        updateWindowTitle();
        statusBar()->showMessage(tr("Saved %1").arg(project().filePath()), 4000);
        return true;
    }
    catch (const std::exception& error) {
        QMessageBox::critical(this, tr("Save failed"), QString::fromUtf8(error.what()));
        return false;
    }
}

bool MainWindow::saveProjectAs()
{
    if (project().isEmpty()) {
        return true;
    }

    const QString path = QFileDialog::getSaveFileName(this, tr("Save LabelPlus text"), project().filePath(),
                                                      tr("LabelPlus text (*.txt);;All files (*)"));
    if (path.isEmpty()) {
        return false;
    }

    const QString oldPath = project().filePath();
    try {
        m_projectController.saveAs(path);
        updateWindowTitle();
        statusBar()->showMessage(tr("Saved %1").arg(project().filePath()), 4000);
        return true;
    }
    catch (const std::exception& error) {
        project().setFilePath(oldPath);
        QMessageBox::critical(this, tr("Save failed"), QString::fromUtf8(error.what()));
        return false;
    }
}

void MainWindow::addGroup()
{
    bool ok = false;
    const QString group =
        QInputDialog::getText(this, tr("Add group"), tr("Group name:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || group.isEmpty()) {
        return;
    }
    if (!project().groups().contains(group)) {
        project().groups().append(group);
        markDirty();
    }
    refreshGroupUi();
    m_insertGroupComboBox->setCurrentText(group);
    refreshCurrentLabelUi();
}

void MainWindow::removeGroup()
{
    const QString group = m_insertGroupComboBox->currentText();
    if (group.isEmpty() || project().groups().size() <= 1) {
        return;
    }

    const QString fallback = project().groups().first();
    for (labelminus::core::ImageEntry& image : project().images()) {
        for (labelminus::core::Label& label : image.labels) {
            if (label.group() == group) {
                label.setGroup(fallback);
            }
        }
    }

    project().groups().removeAll(group);
    refreshGroupUi();
    refreshCurrentLabelUi();
    markDirty();
}

void MainWindow::updateGroupFilter(const QStringList& groups)
{
    if (m_isUpdatingUi) {
        return;
    }

    m_labelModel->setGroupFilter(groups);
    m_canvas->setVisibleGroups(groups);
    resizeLabelRowsToContents();

    if (m_currentLabelIndex >= 0 && m_labelModel->rowForSourceIndex(m_currentLabelIndex) < 0) {
        m_currentLabelIndex = -1;
        m_canvas->setSelectedLabel(-1);
        m_textEdit->clear();
        setEditorEnabled(false);
    }
}

void MainWindow::selectImage(int index)
{
    if (m_isUpdatingUi || index < 0 || index >= project().images().size()) {
        return;
    }

    saveProjectSessionState();
    m_currentImageIndex = index;
    m_currentLabelIndex = -1;
    refreshImageUi();
}

void MainWindow::selectLabel(int index)
{
    labelminus::core::ImageEntry* image = currentImage();
    if (image == nullptr || index < 0 || index >= image->labels.size()) {
        m_currentLabelIndex = -1;
        setEditorEnabled(false);
        return;
    }

    m_isUpdatingUi = true;
    m_currentLabelIndex = index;
    m_canvas->setSelectedLabel(index);
    m_textEdit->setPlainText(image->labels.at(index).text());
    m_labelGroupComboBox->setCurrentText(image->labels.at(index).group());
    const int visibleRow = m_labelModel->rowForSourceIndex(index);
    if (visibleRow >= 0) {
        m_labelView->selectRow(visibleRow);
        m_labelView->resizeRowToContents(visibleRow);
        capLabelRowHeight(visibleRow);
    }
    else {
        m_labelView->clearSelection();
    }
    setEditorEnabled(true);
    m_isUpdatingUi = false;
}

void MainWindow::addLabel(QPointF normalizedPosition)
{
    if (currentImage() == nullptr || m_labelEditController == nullptr) {
        return;
    }

    const QString group =
        m_insertGroupComboBox->currentText().isEmpty() ? QStringLiteral("框内") : m_insertGroupComboBox->currentText();
    if (!m_groupFilterComboBox->selectedGroups().contains(group)) {
        statusBar()->showMessage(tr("The insert group is hidden by the current filter."), 4000);
        return;
    }

    const labelminus::services::LabelEditResult result = m_labelEditController->addLabel(
        m_currentImageIndex, labelminus::core::Label(QString(), group, normalizedPosition));
    if (!result.changed) {
        return;
    }
    m_labelModel->refresh();
    refreshCanvasLabels();
    selectLabel(result.selectedLabelIndex);
}

void MainWindow::deleteSelectedLabels()
{
    const QVector<int> labelIndexes = selectedLabelIndexes();
    if (currentImage() == nullptr || labelIndexes.isEmpty() || m_labelEditController == nullptr) {
        return;
    }

    const labelminus::services::LabelEditResult result =
        m_labelEditController->deleteLabels(m_currentImageIndex, labelIndexes);
    if (!result.changed) {
        return;
    }

    m_currentLabelIndex = -1;
    refreshCanvasLabels();
    m_labelModel->refresh();
    m_labelView->clearSelection();
    m_textEdit->clear();
    setEditorEnabled(false);
}

void MainWindow::changeSelectedLabelsGroup(const QString& group)
{
    const QVector<int> labelIndexes = selectedLabelIndexes();
    if (currentImage() == nullptr || labelIndexes.isEmpty() || m_labelEditController == nullptr) {
        return;
    }

    const labelminus::services::LabelEditResult result =
        m_labelEditController->changeLabelsGroup(m_currentImageIndex, labelIndexes, group);
    if (!result.changed) {
        return;
    }

    m_labelModel->refresh();
    refreshCanvasLabels();
    if (m_labelModel->rowForSourceIndex(result.selectedLabelIndex) >= 0) {
        selectLabel(result.selectedLabelIndex);
    }
    else {
        m_currentLabelIndex = -1;
        m_canvas->setSelectedLabel(-1);
        m_labelView->clearSelection();
        m_textEdit->clear();
        setEditorEnabled(false);
    }
}

void MainWindow::showLabelContextMenu(const QPoint& position)
{
    const QModelIndex clickedIndex = m_labelView->indexAt(position);
    if (clickedIndex.isValid() && !m_labelView->selectionModel()->isSelected(clickedIndex)) {
        m_labelView->selectRow(clickedIndex.row());
    }

    const QVector<int> labelIndexes = selectedLabelIndexes();
    if (labelIndexes.isEmpty()) {
        return;
    }

    QMenu menu(this);
    QAction* deleteAction = menu.addAction(tr("Delete selected labels"));
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteSelectedLabels);
    menu.addSeparator();

    for (const QString& group : project().groups()) {
        auto* groupAction = new QWidgetAction(&menu);
        auto* groupButton = new QPushButton(group, &menu);
        groupButton->setFlat(true);
        groupButton->setCursor(Qt::PointingHandCursor);
        groupButton->setMinimumWidth(180);
        groupButton->setStyleSheet(QStringLiteral("QPushButton { text-align: left; padding: 4px 18px; border: none; }"
                                                  "QPushButton:hover { background: palette(highlight); }"));
        const QColor color = colorForGroup(group);
        if (color.isValid()) {
            groupButton->setStyleSheet(QStringLiteral("QPushButton { color: %1; text-align: left; padding: 4px 18px; "
                                                      "border: none; }"
                                                      "QPushButton:hover { background: palette(highlight); }")
                                           .arg(color.name()));
        }
        groupAction->setDefaultWidget(groupButton);
        menu.addAction(groupAction);
        connect(groupButton, &QPushButton::clicked, &menu, [this, &menu, group]() {
            menu.close();
            changeSelectedLabelsGroup(group);
        });
    }

    menu.exec(m_labelView->viewport()->mapToGlobal(position));
}

void MainWindow::reorderLabels(QVector<int> sourceIndexes, int visibleDropRow)
{
    labelminus::core::ImageEntry* image = currentImage();
    if (image == nullptr || sourceIndexes.isEmpty() || m_labelEditController == nullptr) {
        return;
    }

    const int visibleRowCount = m_labelModel->rowCount();
    visibleDropRow = std::clamp(visibleDropRow, 0, visibleRowCount);
    int insertBeforeSourceIndex = static_cast<int>(image->labels.size());
    if (visibleDropRow < visibleRowCount) {
        insertBeforeSourceIndex = m_labelModel->sourceIndexForRow(visibleDropRow);
        if (insertBeforeSourceIndex < 0) {
            return;
        }
    }

    const labelminus::services::LabelEditResult result =
        m_labelEditController->reorderLabels(m_currentImageIndex, sourceIndexes, insertBeforeSourceIndex);
    if (!result.changed) {
        return;
    }

    refreshCurrentLabelUi();
    selectLabelIndexes(result.selectedLabelIndexes);
}

void MainWindow::updateCurrentLabelText()
{
    if (m_isUpdatingUi) {
        return;
    }

    labelminus::core::ImageEntry* image = currentImage();
    if (image == nullptr || m_currentLabelIndex < 0 || m_currentLabelIndex >= image->labels.size()) {
        return;
    }

    const QString newText = m_textEdit->toPlainText();
    if (image->labels.at(m_currentLabelIndex).text() == newText) {
        return;
    }

    if (m_labelEditController != nullptr) {
        m_labelEditController->setLabelText(m_currentImageIndex, m_currentLabelIndex, newText);
    }
    refreshCanvasLabels();
    m_labelModel->labelChanged(m_currentLabelIndex);
    const int visibleRow = m_labelModel->rowForSourceIndex(m_currentLabelIndex);
    if (visibleRow >= 0) {
        m_labelView->resizeRowToContents(visibleRow);
        capLabelRowHeight(visibleRow);
    }
}

void MainWindow::updateCurrentLabelGroup(int index)
{
    if (m_isUpdatingUi || index < 0) {
        return;
    }

    labelminus::core::ImageEntry* image = currentImage();
    if (image == nullptr || m_currentLabelIndex < 0 || m_currentLabelIndex >= image->labels.size()) {
        return;
    }

    const QString oldGroup = image->labels.at(m_currentLabelIndex).group();
    const QString newGroup = m_labelGroupComboBox->itemText(index);
    if (oldGroup == newGroup) {
        return;
    }

    if (m_labelEditController == nullptr) {
        return;
    }
    const labelminus::services::LabelEditResult result =
        m_labelEditController->setLabelGroup(m_currentImageIndex, m_currentLabelIndex, newGroup);
    if (!result.changed) {
        return;
    }
    m_labelModel->refresh();
    refreshCanvasLabels();
    selectLabel(m_currentLabelIndex);
}

void MainWindow::updateLabelFromTable(int sourceIndex, int column, QVariant oldValue, QVariant newValue)
{
    labelminus::core::ImageEntry* image = currentImage();
    if (image == nullptr || sourceIndex < 0 || sourceIndex >= image->labels.size()) {
        return;
    }

    if (column == 1 && m_labelEditController != nullptr) {
        m_labelEditController->registerLabelTextUndo(m_currentImageIndex, sourceIndex, oldValue.toString(),
                                                     newValue.toString());
        refreshCanvasLabels();
    }
    if (column == 2 && m_labelEditController != nullptr) {
        m_labelEditController->registerLabelGroupUndo(m_currentImageIndex, sourceIndex, oldValue.toString(),
                                                      newValue.toString());
        m_labelModel->refresh();
        refreshCanvasLabels();
    }

    selectLabel(sourceIndex);
    markDirty();
}

void MainWindow::moveLabel(int index, QPointF normalizedPosition)
{
    labelminus::core::ImageEntry* image = currentImage();
    if (image == nullptr || index < 0 || index >= image->labels.size()) {
        return;
    }

    if (m_labelEditController == nullptr) {
        return;
    }

    const labelminus::services::LabelEditResult result =
        m_labelEditController->setLabelPosition(m_currentImageIndex, index, normalizedPosition);
    if (!result.changed) {
        refreshCanvasLabels();
        selectLabel(index);
        return;
    }

    refreshCanvasLabels();
    selectLabel(index);
}

void MainWindow::undoLastOperation()
{
    m_undoStack.undo();
}

void MainWindow::redoLastOperation()
{
    m_undoStack.redo();
}

void MainWindow::updateEditShortcuts()
{
    if (m_undoAction != nullptr) {
        m_undoAction->setShortcut(m_preferences.undoShortcut());
    }
    if (m_redoAction != nullptr) {
        m_redoAction->setShortcut(m_preferences.redoShortcut());
    }
}

QVector<int> MainWindow::selectedLabelIndexes() const
{
    QVector<int> labelIndexes;
    if (m_labelView == nullptr || m_labelModel == nullptr || m_labelView->selectionModel() == nullptr) {
        return labelIndexes;
    }

    const QModelIndexList rows = m_labelView->selectionModel()->selectedRows();
    labelIndexes.reserve(rows.size());
    for (const QModelIndex& row : rows) {
        const int sourceIndex = m_labelModel->sourceIndexForRow(row.row());
        if (sourceIndex >= 0) {
            labelIndexes.append(sourceIndex);
        }
    }

    std::sort(labelIndexes.begin(), labelIndexes.end());
    labelIndexes.erase(std::unique(labelIndexes.begin(), labelIndexes.end()), labelIndexes.end());
    return labelIndexes;
}

void MainWindow::selectLabelIndexes(const QVector<int>& sourceIndexes)
{
    labelminus::core::ImageEntry* image = currentImage();
    if (image == nullptr || m_labelView == nullptr || m_labelView->selectionModel() == nullptr) {
        return;
    }

    QVector<int> visibleSourceIndexes;
    visibleSourceIndexes.reserve(sourceIndexes.size());
    for (int sourceIndex : sourceIndexes) {
        if (sourceIndex >= 0 && sourceIndex < image->labels.size() &&
            m_labelModel->rowForSourceIndex(sourceIndex) >= 0) {
            visibleSourceIndexes.append(sourceIndex);
        }
    }
    std::sort(visibleSourceIndexes.begin(), visibleSourceIndexes.end());
    visibleSourceIndexes.erase(std::unique(visibleSourceIndexes.begin(), visibleSourceIndexes.end()),
                               visibleSourceIndexes.end());

    QSignalBlocker selectionBlocker(m_labelView->selectionModel());
    m_labelView->clearSelection();

    if (visibleSourceIndexes.isEmpty()) {
        m_isUpdatingUi = true;
        m_currentLabelIndex = -1;
        m_canvas->setSelectedLabel(-1);
        m_textEdit->clear();
        setEditorEnabled(false);
        m_isUpdatingUi = false;
        return;
    }

    const int primarySourceIndex = visibleSourceIndexes.last();
    const labelminus::core::Label& primaryLabel = image->labels.at(primarySourceIndex);

    m_isUpdatingUi = true;
    m_currentLabelIndex = primarySourceIndex;
    m_canvas->setSelectedLabel(primarySourceIndex);
    m_textEdit->setPlainText(primaryLabel.text());
    m_labelGroupComboBox->setCurrentText(primaryLabel.group());
    setEditorEnabled(true);

    QItemSelection selection;
    for (int sourceIndex : visibleSourceIndexes) {
        const int visibleRow = m_labelModel->rowForSourceIndex(sourceIndex);
        if (visibleRow < 0) {
            continue;
        }
        selection.select(m_labelModel->index(visibleRow, 0),
                         m_labelModel->index(visibleRow, m_labelModel->columnCount() - 1));
        m_labelView->resizeRowToContents(visibleRow);
        capLabelRowHeight(visibleRow);
    }

    m_labelView->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    const int primaryVisibleRow = m_labelModel->rowForSourceIndex(primarySourceIndex);
    if (primaryVisibleRow >= 0) {
        m_labelView->selectionModel()->setCurrentIndex(m_labelModel->index(primaryVisibleRow, 0),
                                                       QItemSelectionModel::Current | QItemSelectionModel::Rows);
    }
    m_isUpdatingUi = false;
}

void MainWindow::refreshProjectUi()
{
    m_isUpdatingUi = true;
    m_imageComboBox->clear();
    for (const labelminus::core::ImageEntry& image : project().images()) {
        m_imageComboBox->addItem(image.name);
    }
    refreshGroupUi();
    m_isUpdatingUi = false;
    m_labelModel->setGroupFilter(project().groups());

    m_currentImageIndex = project().images().isEmpty() ? -1 : 0;
    refreshImageUi();
    updateWindowTitle();
}

void MainWindow::refreshImageUi()
{
    m_isUpdatingUi = true;
    const labelminus::core::ImageEntry* image = currentImage();
    if (image == nullptr) {
        m_labelModel->setLabels(nullptr);
        m_textEdit->clear();
        setEditorEnabled(false);
        m_isUpdatingUi = false;
        return;
    }

    m_imageComboBox->setCurrentIndex(m_currentImageIndex);
    m_previousButton->setEnabled(m_currentImageIndex > 0);
    m_nextButton->setEnabled(m_currentImageIndex >= 0 && m_currentImageIndex < project().images().size() - 1);
    m_canvas->setImage(image->path, image->labels);
    m_labelModel->setLabels(&project().images()[m_currentImageIndex].labels);
    resizeLabelRowsToContents();
    m_textEdit->clear();
    setEditorEnabled(false);
    m_isUpdatingUi = false;
}

void MainWindow::refreshCanvasLabels()
{
    const labelminus::core::ImageEntry* image = currentImage();
    if (image == nullptr) {
        return;
    }

    m_canvas->setLabels(image->labels);
}

void MainWindow::refreshCurrentLabelUi()
{
    if (currentImage() == nullptr) {
        return;
    }

    m_labelModel->refresh();
    refreshCanvasLabels();
    resizeLabelRowsToContents();
}

void MainWindow::refreshLabelEditSelection(int imageIndex, int labelIndex)
{
    if (imageIndex != m_currentImageIndex) {
        m_currentImageIndex = imageIndex;
        refreshImageUi();
    }
    else {
        refreshCurrentLabelUi();
    }

    if (m_labelModel->rowForSourceIndex(labelIndex) >= 0) {
        selectLabel(labelIndex);
    }
    else {
        m_currentLabelIndex = -1;
        m_canvas->setSelectedLabel(-1);
        m_labelView->clearSelection();
        m_textEdit->clear();
        setEditorEnabled(false);
    }
}

void MainWindow::refreshLabelEditSelection(int imageIndex, QVector<int> labelIndexes)
{
    if (imageIndex != m_currentImageIndex) {
        m_currentImageIndex = imageIndex;
        refreshImageUi();
    }
    else {
        refreshCurrentLabelUi();
    }
    selectLabelIndexes(labelIndexes);
}

void MainWindow::clearLabelEditSelection(int imageIndex)
{
    if (imageIndex != m_currentImageIndex) {
        m_currentImageIndex = imageIndex;
        refreshImageUi();
    }
    else {
        refreshCurrentLabelUi();
    }

    m_currentLabelIndex = -1;
    m_canvas->setSelectedLabel(-1);
    m_labelView->clearSelection();
    m_textEdit->clear();
    setEditorEnabled(false);
}

void MainWindow::refreshGroupUi()
{
    m_isUpdatingUi = true;
    const QString previousInsertGroup = m_insertGroupComboBox->currentText();
    m_labelGroupComboBox->clear();
    m_insertGroupComboBox->clear();
    if (project().groups().isEmpty()) {
        project().setGroups({QStringLiteral("框内"), QStringLiteral("框外")});
    }
    m_groupFilterComboBox->setGroups(project().groups(), m_preferences.groupStyles());
    m_canvas->setGroups(project().groups());
    m_canvas->setVisibleGroups(m_groupFilterComboBox->selectedGroups());
    m_labelModel->setGroups(project().groups(), m_preferences.groupStyles());
    m_labelGroupDelegate->setGroups(project().groups(), m_preferences.groupStyles());
    m_labelGroupComboBox->addItems(project().groups());
    m_insertGroupComboBox->addItems(project().groups());
    applyGroupStylesToCombo(m_insertGroupComboBox);
    if (!previousInsertGroup.isEmpty()) {
        m_insertGroupComboBox->setCurrentText(previousInsertGroup);
    }
    updateInsertGroupTextColor();
    m_isUpdatingUi = false;
}

void MainWindow::resizeLabelRowsToContents()
{
    if (m_labelView == nullptr) {
        return;
    }

    m_labelView->resizeRowsToContents();
    for (int row = 0; row < m_labelModel->rowCount(); ++row) {
        capLabelRowHeight(row);
    }
}

void MainWindow::capLabelRowHeight(int row)
{
    if (m_labelView == nullptr || row < 0 || row >= m_labelModel->rowCount()) {
        return;
    }

    const QFontMetrics metrics(m_labelView->font());
    const int contentHeight = metrics.lineSpacing() * std::max(1, m_labelTableMaxTextRows);
    const int verticalMargin = 10;
    const int maximumHeight =
        std::max(m_labelView->verticalHeader()->minimumSectionSize(), contentHeight + verticalMargin);
    if (m_labelView->rowHeight(row) > maximumHeight) {
        m_labelView->setRowHeight(row, maximumHeight);
    }
}

void MainWindow::restoreLayoutState()
{
    const labelminus::services::WindowLayoutState state = m_sessionStateStore.loadWindowLayout();

    if (!state.geometry.isEmpty()) {
        restoreGeometry(state.geometry);
    }

    if (!state.windowState.isEmpty()) {
        restoreState(state.windowState);
    }

    if (!state.rootSplitterState.isEmpty() && m_rootSplitter != nullptr) {
        m_rootSplitter->restoreState(state.rootSplitterState);
    }

    if (!state.rightSplitterState.isEmpty() && m_rightSplitter != nullptr) {
        m_rightSplitter->restoreState(state.rightSplitterState);
    }
}

void MainWindow::saveLayoutState() const
{
    labelminus::services::WindowLayoutState state;
    state.geometry = saveGeometry();
    state.windowState = saveState();
    if (m_rootSplitter != nullptr) {
        state.rootSplitterState = m_rootSplitter->saveState();
    }
    if (m_rightSplitter != nullptr) {
        state.rightSplitterState = m_rightSplitter->saveState();
    }
    m_sessionStateStore.saveWindowLayout(state);
}

void MainWindow::restoreProjectSessionState()
{
    if (project().isEmpty() || project().filePath().isEmpty() || project().images().isEmpty()) {
        return;
    }

    const labelminus::services::ProjectSessionState state =
        m_sessionStateStore.loadProjectSession(project().filePath());
    if (!state.isValid) {
        return;
    }

    int imageIndex = state.imageIndex;
    if (!state.imageName.isEmpty()) {
        for (int i = 0; i < project().images().size(); ++i) {
            if (project().images().at(i).name == state.imageName) {
                imageIndex = i;
                break;
            }
        }
    }
    if (imageIndex < 0 || imageIndex >= project().images().size()) {
        imageIndex = 0;
    }

    m_currentImageIndex = imageIndex;
    m_currentLabelIndex = -1;
    refreshImageUi();
    m_canvas->restoreView(state.zoomPercent, state.viewCenter);
    {
        const QSignalBlocker zoomBlocker(m_zoomSlider);
        m_zoomSlider->setValue(m_canvas->zoomPercent());
    }

    const labelminus::core::ImageEntry* image = currentImage();
    if (image != nullptr && state.selectedLabelIndex >= 0 && state.selectedLabelIndex < image->labels.size() &&
        !image->labels.at(state.selectedLabelIndex).isDeleted() &&
        m_labelModel->rowForSourceIndex(state.selectedLabelIndex) >= 0) {
        selectLabel(state.selectedLabelIndex);
    }
}

void MainWindow::saveProjectSessionState() const
{
    if (project().isEmpty() || project().filePath().isEmpty() || m_currentImageIndex < 0 ||
        m_currentImageIndex >= project().images().size() || m_canvas == nullptr) {
        return;
    }

    labelminus::services::ProjectSessionState state;
    state.isValid = true;
    state.imageIndex = m_currentImageIndex;
    state.imageName = project().images().at(m_currentImageIndex).name;
    state.zoomPercent = m_canvas->zoomPercent();
    state.viewCenter = m_canvas->normalizedViewCenter();
    state.selectedLabelIndex = m_currentLabelIndex;
    m_sessionStateStore.saveProjectSession(project().filePath(), state);
}

void MainWindow::configureBackupTimer()
{
    if (m_backupTimer == nullptr) {
        return;
    }

    m_backupTimer->start(m_preferences.backupIntervalSeconds() * 1000);
}

void MainWindow::performAutoBackup()
{
    const labelminus::services::AutoBackupResult result = m_projectController.performAutoBackup(m_preferences);
    switch (result.status) {
    case labelminus::services::AutoBackupResult::Status::Skipped:
        return;
    case labelminus::services::AutoBackupResult::Status::Saved:
        statusBar()->showMessage(tr("Auto backed up %1").arg(result.path), 4000);
        return;
    case labelminus::services::AutoBackupResult::Status::Failed:
        if (QFileInfo(result.error).isAbsolute()) {
            statusBar()->showMessage(tr("Auto backup failed: could not create %1").arg(result.error), 4000);
        }
        else {
            statusBar()->showMessage(tr("Auto backup failed: %1").arg(result.error), 4000);
        }
        return;
    }
}

void MainWindow::applyLabelTableFont()
{
    if (m_labelView == nullptr) {
        return;
    }

    QFont font = m_defaultLabelTableFont;
    if (!m_preferences.labelTableFontFamily().isEmpty()) {
        font.setFamily(m_preferences.labelTableFontFamily());
    }
    if (m_preferences.labelTableFontPointSize() > 0.0) {
        font.setPointSizeF(m_preferences.labelTableFontPointSize());
    }

    m_labelView->setFont(font);
    m_labelView->viewport()->setFont(font);
    resizeLabelRowsToContents();
}

void MainWindow::applyTextEditorFont()
{
    if (m_textEdit == nullptr) {
        return;
    }

    QFont font = m_defaultTextEditFont;
    if (!m_preferences.labelTextEditorFontFamily().isEmpty()) {
        font.setFamily(m_preferences.labelTextEditorFontFamily());
    }
    if (m_preferences.labelTextEditorFontPointSize() > 0.0) {
        font.setPointSizeF(m_preferences.labelTextEditorFontPointSize());
    }
    m_textEdit->setFont(font);
}

void MainWindow::showPreferenceWarnings()
{
    if (m_warningLabel == nullptr) {
        return;
    }

    if (m_preferenceWarnings.isEmpty()) {
        m_warningLabel->clear();
        m_warningLabel->setToolTip({});
        m_warningLabel->setVisible(false);
        return;
    }

    QStringList messages;
    messages.reserve(m_preferenceWarnings.size());
    for (const labelminus::core::AppPreferenceWarning& warning : m_preferenceWarnings) {
        messages.append(preferenceWarningText(warning));
    }

    m_warningLabel->setText(tr("Preference warnings"));
    m_warningLabel->setToolTip(messages.join(QStringLiteral("\n")));
    m_warningLabel->setVisible(true);
}

void MainWindow::applyPreferences(labelminus::core::AppPreferencesLoadResult result)
{
    m_preferences = result.preferences;
    m_preferenceWarnings = std::move(result.warnings);
    m_labelTableMaxTextRows = m_preferences.labelTableMaxTextRows();

    const QString styleName = m_preferences.applicationStyle().isEmpty()
                                  ? qApp->property("labelminus.defaultStyle").toString()
                                  : m_preferences.applicationStyle();
    if (!styleName.isEmpty() && QStyleFactory::keys().contains(styleName, Qt::CaseInsensitive)) {
        QApplication::setStyle(styleName);
    }

    if (m_canvas != nullptr) {
        m_canvas->setPreferences(m_preferences);
    }

    refreshGroupUi();
    refreshCurrentLabelUi();
    applyLabelTableFont();
    applyTextEditorFont();
    updateEditShortcuts();
    configureBackupTimer();
    showPreferenceWarnings();
    statusBar()->showMessage(tr("Preferences applied"), 4000);
}

QString MainWindow::preferenceWarningText(const labelminus::core::AppPreferenceWarning& warning) const
{
    using labelminus::core::AppPreferenceWarningType;

    switch (warning.type) {
    case AppPreferenceWarningType::FileNotReadable:
        return tr("Could not read preference.json; using default preferences.");
    case AppPreferenceWarningType::InvalidJson:
        return tr("preference.json is not valid JSON: %1; using default preferences.").arg(warning.detail);
    case AppPreferenceWarningType::RootNotObject:
        return tr("preference.json must contain a JSON object; using default preferences.");
    case AppPreferenceWarningType::AppearanceNotObject:
        return tr("appearance must be a JSON object; using default appearance preferences.");
    case AppPreferenceWarningType::AppearanceStyleWrongType:
        return tr("%1 must be a string; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::LabelMarkerNotObject:
        return tr("labelMarker must be a JSON object; using default marker preferences.");
    case AppPreferenceWarningType::MarkerSizeWrongType:
        return tr("%1 must be a positive number; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::MarkerSizeOutOfRange:
        return tr("%1 must be a positive number; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::LabelTableNotObject:
        return tr("labelTable must be a JSON object; using default label table preferences.");
    case AppPreferenceWarningType::LabelTableMaxTextRowsWrongType:
        return tr("%1 must be a positive integer; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::LabelTableMaxTextRowsOutOfRange:
        return tr("%1 must be a positive integer; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::LabelTableFontFamilyWrongType:
        return tr("%1 must be a string; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::LabelTableFontPointSizeWrongType:
        return tr("%1 must be zero or a positive number; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::LabelTableFontPointSizeOutOfRange:
        return tr("%1 must be zero or a positive number; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::LabelTextEditorNotObject:
        return tr("labelTextEditor must be a JSON object; using default text editor preferences.");
    case AppPreferenceWarningType::LabelTextEditorFontFamilyWrongType:
        return tr("%1 must be a string; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::LabelTextEditorFontPointSizeWrongType:
        return tr("%1 must be zero or a positive number; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::LabelTextEditorFontPointSizeOutOfRange:
        return tr("%1 must be zero or a positive number; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::GroupStylesNotArray:
        return tr("groupStyles must be an array; group styles will use defaults.");
    case AppPreferenceWarningType::GroupStyleNotObject:
        return tr("groupStyles[%1] must be a JSON object; this group style will use defaults.").arg(warning.index);
    case AppPreferenceWarningType::InvalidGroupStyleColor:
        return tr("groupStyles[%1].groupColor is not a valid color; this color was skipped.").arg(warning.index);
    case AppPreferenceWarningType::GroupStyleMarkerSizeWrongType:
        return tr("%1 must be a positive number; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::GroupStyleMarkerSizeOutOfRange:
        return tr("%1 must be a positive number; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::GroupStyleMarkerStyleInvalid:
        return tr("groupStyles[%1].markerStyle must be circle or square; using the default value.").arg(warning.index);
    case AppPreferenceWarningType::InputNotObject:
        return tr("input must be a JSON object; using default input preferences.");
    case AppPreferenceWarningType::MoveLabelModifierInvalid:
        return tr("%1 must be a modifier name or modifier combination; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::UndoShortcutInvalid:
    case AppPreferenceWarningType::RedoShortcutInvalid:
        return tr("%1 must be a valid key sequence; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::BackupPathWrongType:
        return tr("%1 must be a non-empty string; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::BackupIntervalWrongType:
        return tr("%1 must be a positive integer; using the default value.").arg(warning.key);
    case AppPreferenceWarningType::BackupIntervalOutOfRange:
        return tr("%1 must be a positive integer; using the default value.").arg(warning.key);
    }

    return tr("Unknown preference warning.");
}

void MainWindow::markDirty()
{
    if (!m_isUpdatingUi) {
        m_projectController.markDirty();
        updateWindowTitle();
    }
}

void MainWindow::setDirty(bool dirty)
{
    if (m_projectController.isDirty() == dirty) {
        return;
    }

    m_projectController.setDirty(dirty);
    updateWindowTitle();
}

bool MainWindow::promptToSaveIfDirty()
{
    if (!m_projectController.isDirty()) {
        return true;
    }

    const QMessageBox::StandardButton result = QMessageBox::warning(
        this, tr("Unsaved changes"), tr("The current project has unsaved changes. Do you want to save them?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);

    if (result == QMessageBox::Cancel) {
        return false;
    }

    if (result == QMessageBox::Save) {
        return saveProject();
    }

    return true;
}

void MainWindow::updateWindowTitle()
{
    QString title = QStringLiteral("LabelMinus");
    if (!project().filePath().isEmpty()) {
        title += QStringLiteral(" - %1").arg(project().filePath());
    }
    if (m_projectController.isDirty()) {
        title += QStringLiteral(" *");
    }
    setWindowTitle(title);
}

void MainWindow::applyGroupStylesToCombo(QComboBox* comboBox)
{
    for (int i = 0; i < comboBox->count(); ++i) {
        const QColor color = colorForGroup(comboBox->itemText(i));
        if (color.isValid()) {
            comboBox->setItemData(i, color, Qt::ForegroundRole);
        }
    }
}

void MainWindow::updateInsertGroupTextColor()
{
    const QColor color = colorForGroup(m_insertGroupComboBox->currentText());
    m_insertGroupComboBox->setStyleSheet(color.isValid() ? QStringLiteral("QComboBox { color: %1; }").arg(color.name())
                                                         : QString());
}

QColor MainWindow::colorForGroup(const QString& group) const
{
    const int index = static_cast<int>(project().groups().indexOf(group));
    if (index < 0 || index >= static_cast<int>(m_preferences.groupStyles().size())) {
        return {};
    }
    return m_preferences.groupStyles().at(index).groupColor;
}

void MainWindow::setEditorEnabled(bool enabled)
{
    m_textEdit->setEnabled(enabled);
    m_labelGroupComboBox->setEnabled(enabled);
}

labelminus::core::Project& MainWindow::project() noexcept
{
    return m_projectController.project();
}

const labelminus::core::Project& MainWindow::project() const noexcept
{
    return m_projectController.project();
}

labelminus::core::ImageEntry* MainWindow::currentImage()
{
    if (m_currentImageIndex < 0 || m_currentImageIndex >= project().images().size()) {
        return nullptr;
    }
    return &project().images()[m_currentImageIndex];
}

const labelminus::core::ImageEntry* MainWindow::currentImage() const
{
    if (m_currentImageIndex < 0 || m_currentImageIndex >= project().images().size()) {
        return nullptr;
    }
    return &project().images().at(m_currentImageIndex);
}
