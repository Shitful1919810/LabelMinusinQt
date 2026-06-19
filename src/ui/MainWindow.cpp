#include "ui/MainWindow.h"

#include "core/LabelPlusDocument.h"
#include "ui/LabelEditDelegates.h"
#include "ui/PreferenceDialog.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
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
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QStringList>
#include <QStringView>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>
#include <utility>

namespace {
constexpr QLatin1StringView layoutGroup{"layout"};
constexpr QLatin1StringView geometryKey{"geometry"};
constexpr QLatin1StringView windowStateKey{"windowState"};
constexpr QLatin1StringView rootSplitterKey{"rootSplitter"};
constexpr QLatin1StringView rightSplitterKey{"rightSplitter"};

bool labelEquals(const labelminus::core::Label& lhs, const labelminus::core::Label& rhs)
{
    return lhs.text() == rhs.text() && lhs.group() == rhs.group() && lhs.position() == rhs.position() &&
           lhs.isDeleted() == rhs.isDeleted();
}

bool labelVectorsEqual(const QVector<labelminus::core::Label>& lhs, const QVector<labelminus::core::Label>& rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (int i = 0; i < lhs.size(); ++i) {
        if (!labelEquals(lhs.at(i), rhs.at(i))) {
            return false;
        }
    }
    return true;
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_labelModel(new LabelTableModel(this)), m_labelTextDelegate(new LabelTextDelegate(this)),
      m_labelGroupDelegate(new LabelGroupDelegate(this))
{
    const labelminus::core::AppPreferencesLoadResult preferences =
        labelminus::core::AppPreferences::loadWithDiagnostics();
    m_preferences = preferences.preferences;
    m_preferenceWarnings = preferences.warnings;
    m_labelTableMaxTextRows = m_preferences.labelTableMaxTextRows();

    setWindowTitle(QStringLiteral("LabelMinus"));
    resize(1200, 800);

    createActions();
    createMenus();
    createCentralWidget();
    setEditorEnabled(false);
    m_warningLabel = new QLabel(this);
    m_warningLabel->setTextFormat(Qt::PlainText);
    m_warningLabel->setStyleSheet(QStringLiteral("QLabel { color: #b26a00; font-weight: 600; }"));
    m_warningLabel->setVisible(false);
    statusBar()->addPermanentWidget(m_warningLabel);
    statusBar()->showMessage(tr("Ready"));
    showPreferenceWarnings();
    restoreLayoutState();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!promptToSaveIfDirty()) {
        event->ignore();
        return;
    }

    saveLayoutState();
    event->accept();
}

void MainWindow::createActions()
{
    m_openProjectAction = new QAction(tr("&Open LabelPlus Text..."), this);
    m_openProjectAction->setShortcut(QKeySequence::Open);
    connect(m_openProjectAction, &QAction::triggered, this, &MainWindow::openProject);

    m_saveProjectAction = new QAction(tr("&Save"), this);
    m_saveProjectAction->setShortcut(QKeySequence::Save);
    connect(m_saveProjectAction, &QAction::triggered, this, &MainWindow::saveProject);

    m_saveProjectAsAction = new QAction(tr("Save &As..."), this);
    m_saveProjectAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveProjectAsAction, &QAction::triggered, this, &MainWindow::saveProjectAs);

    m_preferencesAction = new QAction(tr("&Preferences..."), this);
    m_preferencesAction->setShortcut(QKeySequence::Preferences);
    connect(m_preferencesAction, &QAction::triggered, this, &MainWindow::openPreferences);

    m_quitAction = new QAction(tr("&Quit"), this);
    m_quitAction->setShortcut(QKeySequence::Quit);
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::createMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_openProjectAction);
    fileMenu->addAction(m_saveProjectAction);
    fileMenu->addAction(m_saveProjectAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_preferencesAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAction);
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
    connect(m_canvas, &ImageCanvas::undoRequested, this, &MainWindow::undoLastOperation);
    connect(m_canvas, &ImageCanvas::zoomPercentChanged, m_zoomSlider, &QSlider::setValue);
    connect(m_zoomSlider, &QSlider::valueChanged, m_canvas, &ImageCanvas::setZoomPercent);
    connect(m_imageComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::selectImage);
    connect(m_previousButton, &QPushButton::clicked, this,
            [this]() { selectImage(std::max(0, m_currentImageIndex - 1)); });
    connect(m_nextButton, &QPushButton::clicked, this, [this]() {
        const int lastIndex = static_cast<int>(m_project.images().size()) - 1;
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
    connect(m_groupFilterComboBox, &GroupFilterComboBox::selectedGroupsChanged, this, &MainWindow::updateGroupFilter);
    connect(m_labelGroupComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::updateCurrentLabelGroup);
    connect(m_insertGroupComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::updateInsertGroupTextColor);
    connect(addGroupButton, &QToolButton::clicked, this, &MainWindow::addGroup);
    connect(removeGroupButton, &QToolButton::clicked, this, &MainWindow::removeGroup);
}

void MainWindow::openProject()
{
    if (!promptToSaveIfDirty()) {
        return;
    }

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
        m_project = labelminus::core::LabelPlusDocument::loadFromFile(path);
        m_undoStack.clear();
        setDirty(false);
        refreshProjectUi();
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
    if (m_project.isEmpty()) {
        return true;
    }

    if (m_project.filePath().isEmpty()) {
        return saveProjectAs();
    }

    try {
        labelminus::core::LabelPlusDocument::saveToFile(m_project, m_project.filePath());
        setDirty(false);
        statusBar()->showMessage(tr("Saved %1").arg(m_project.filePath()), 4000);
        return true;
    }
    catch (const std::exception& error) {
        QMessageBox::critical(this, tr("Save failed"), QString::fromUtf8(error.what()));
        return false;
    }
}

bool MainWindow::saveProjectAs()
{
    if (m_project.isEmpty()) {
        return true;
    }

    const QString path = QFileDialog::getSaveFileName(this, tr("Save LabelPlus text"), m_project.filePath(),
                                                      tr("LabelPlus text (*.txt);;All files (*)"));
    if (path.isEmpty()) {
        return false;
    }

    const QString oldPath = m_project.filePath();
    m_project.setFilePath(path);
    if (!saveProject()) {
        m_project.setFilePath(oldPath);
        return false;
    }
    return true;
}

void MainWindow::addGroup()
{
    bool ok = false;
    const QString group =
        QInputDialog::getText(this, tr("Add group"), tr("Group name:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || group.isEmpty()) {
        return;
    }
    if (!m_project.groups().contains(group)) {
        m_project.groups().append(group);
        markDirty();
    }
    refreshGroupUi();
    m_insertGroupComboBox->setCurrentText(group);
    refreshImageUi();
}

void MainWindow::removeGroup()
{
    const QString group = m_insertGroupComboBox->currentText();
    if (group.isEmpty() || m_project.groups().size() <= 1) {
        return;
    }

    const QString fallback = m_project.groups().first();
    for (labelminus::core::ImageEntry& image : m_project.images()) {
        for (labelminus::core::Label& label : image.labels) {
            if (label.group() == group) {
                label.setGroup(fallback);
            }
        }
    }

    m_project.groups().removeAll(group);
    refreshGroupUi();
    refreshImageUi();
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
    if (m_isUpdatingUi || index < 0 || index >= m_project.images().size()) {
        return;
    }

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
    m_textEditUndoImageIndex = m_currentImageIndex;
    m_textEditUndoLabelIndex = index;
    m_textEditUndoOriginalText = image->labels.at(index).text();
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
    labelminus::core::ImageEntry* image = currentImage();
    if (image == nullptr) {
        return;
    }

    const QString group =
        m_insertGroupComboBox->currentText().isEmpty() ? QStringLiteral("框内") : m_insertGroupComboBox->currentText();
    if (!m_groupFilterComboBox->selectedGroups().contains(group)) {
        statusBar()->showMessage(tr("The insert group is hidden by the current filter."), 4000);
        return;
    }

    image->labels.append(labelminus::core::Label(QString(), group, normalizedPosition));
    const int imageIndex = m_currentImageIndex;
    const int labelIndex = static_cast<int>(image->labels.size()) - 1;
    const labelminus::core::Label addedLabel = image->labels.last();
    m_undoStack.push({
        tr("Add label"),
        [this, imageIndex, labelIndex, addedLabel]() {
            if (imageIndex < 0 || imageIndex >= m_project.images().size()) {
                return;
            }

            labelminus::core::ImageEntry& targetImage = m_project.images()[imageIndex];
            if (labelIndex < 0 || labelIndex >= targetImage.labels.size()) {
                return;
            }

            const labelminus::core::Label& currentLabel = targetImage.labels.at(labelIndex);
            if (currentLabel.position() != addedLabel.position() || currentLabel.group() != addedLabel.group() ||
                currentLabel.text() != addedLabel.text()) {
                return;
            }

            targetImage.labels.removeAt(labelIndex);
            m_currentImageIndex = imageIndex;
            m_currentLabelIndex = -1;
            refreshImageUi();
            markDirty();
        },
    });
    m_labelModel->refresh();
    refreshImageUi();
    selectLabel(static_cast<int>(image->labels.size()) - 1);
    markDirty();
}

void MainWindow::deleteSelectedLabels()
{
    labelminus::core::ImageEntry* image = currentImage();
    const QVector<int> labelIndexes = selectedLabelIndexes();
    if (image == nullptr || labelIndexes.isEmpty()) {
        return;
    }

    QVector<bool> oldDeleted;
    QVector<bool> newDeleted;
    QVector<int> changedIndexes;
    for (int labelIndex : labelIndexes) {
        if (labelIndex < 0 || labelIndex >= image->labels.size() || image->labels.at(labelIndex).isDeleted()) {
            continue;
        }
        changedIndexes.append(labelIndex);
        oldDeleted.append(false);
        newDeleted.append(true);
        image->labels[labelIndex].setDeleted(true);
    }

    if (changedIndexes.isEmpty()) {
        return;
    }

    pushBatchLabelDeletedUndo(m_currentImageIndex, changedIndexes, oldDeleted, newDeleted);
    m_currentLabelIndex = -1;
    m_canvas->setImage(image->path, image->labels);
    m_labelModel->refresh();
    m_labelView->clearSelection();
    m_textEdit->clear();
    setEditorEnabled(false);
    markDirty();
}

void MainWindow::changeSelectedLabelsGroup(const QString& group)
{
    labelminus::core::ImageEntry* image = currentImage();
    const QVector<int> labelIndexes = selectedLabelIndexes();
    if (image == nullptr || labelIndexes.isEmpty() || !m_project.groups().contains(group)) {
        return;
    }

    QVector<int> changedIndexes;
    QVector<QString> oldGroups;
    QVector<QString> newGroups;
    for (int labelIndex : labelIndexes) {
        if (labelIndex < 0 || labelIndex >= image->labels.size() || image->labels.at(labelIndex).isDeleted() ||
            image->labels.at(labelIndex).group() == group) {
            continue;
        }

        changedIndexes.append(labelIndex);
        oldGroups.append(image->labels.at(labelIndex).group());
        newGroups.append(group);
        image->labels[labelIndex].setGroup(group);
    }

    if (changedIndexes.isEmpty()) {
        return;
    }

    pushBatchLabelGroupUndo(m_currentImageIndex, changedIndexes, oldGroups, newGroups);
    m_labelModel->refresh();
    m_canvas->setImage(image->path, image->labels);
    if (m_labelModel->rowForSourceIndex(changedIndexes.last()) >= 0) {
        selectLabel(changedIndexes.last());
    }
    else {
        m_currentLabelIndex = -1;
        m_canvas->setSelectedLabel(-1);
        m_labelView->clearSelection();
        m_textEdit->clear();
        setEditorEnabled(false);
    }
    markDirty();
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

    for (const QString& group : m_project.groups()) {
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
    if (image == nullptr || sourceIndexes.isEmpty()) {
        return;
    }

    std::sort(sourceIndexes.begin(), sourceIndexes.end());
    sourceIndexes.erase(std::unique(sourceIndexes.begin(), sourceIndexes.end()), sourceIndexes.end());
    sourceIndexes.erase(std::remove_if(sourceIndexes.begin(), sourceIndexes.end(),
                                       [image](int sourceIndex) {
                                           return sourceIndex < 0 || sourceIndex >= image->labels.size() ||
                                                  image->labels.at(sourceIndex).isDeleted();
                                       }),
                        sourceIndexes.end());
    if (sourceIndexes.isEmpty()) {
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

    struct IndexedLabel {
        int oldIndex;
        labelminus::core::Label label;
    };

    const QVector<labelminus::core::Label> oldLabels = image->labels;
    QVector<IndexedLabel> movingLabels;
    QVector<IndexedLabel> remainingLabels;
    movingLabels.reserve(sourceIndexes.size());
    remainingLabels.reserve(oldLabels.size() - sourceIndexes.size());

    for (int i = 0; i < oldLabels.size(); ++i) {
        IndexedLabel indexedLabel{i, oldLabels.at(i)};
        if (std::binary_search(sourceIndexes.cbegin(), sourceIndexes.cend(), i)) {
            movingLabels.append(std::move(indexedLabel));
        }
        else {
            remainingLabels.append(std::move(indexedLabel));
        }
    }
    if (movingLabels.isEmpty()) {
        return;
    }

    int remainingInsertIndex = 0;
    for (int i = 0; i < insertBeforeSourceIndex; ++i) {
        if (!std::binary_search(sourceIndexes.cbegin(), sourceIndexes.cend(), i)) {
            ++remainingInsertIndex;
        }
    }
    remainingInsertIndex = std::clamp(remainingInsertIndex, 0, static_cast<int>(remainingLabels.size()));

    QVector<IndexedLabel> reorderedLabels;
    reorderedLabels.reserve(oldLabels.size());
    for (int i = 0; i < remainingInsertIndex; ++i) {
        reorderedLabels.append(std::move(remainingLabels[i]));
    }
    for (IndexedLabel& label : movingLabels) {
        reorderedLabels.append(std::move(label));
    }
    for (int i = remainingInsertIndex; i < remainingLabels.size(); ++i) {
        reorderedLabels.append(std::move(remainingLabels[i]));
    }

    QVector<labelminus::core::Label> newLabels;
    QVector<int> newSelectedIndexes;
    newLabels.reserve(reorderedLabels.size());
    newSelectedIndexes.reserve(sourceIndexes.size());
    for (int i = 0; i < reorderedLabels.size(); ++i) {
        if (std::binary_search(sourceIndexes.cbegin(), sourceIndexes.cend(), reorderedLabels.at(i).oldIndex)) {
            newSelectedIndexes.append(i);
        }
        newLabels.append(reorderedLabels.at(i).label);
    }

    if (labelVectorsEqual(oldLabels, newLabels)) {
        return;
    }

    const int imageIndex = m_currentImageIndex;
    image->labels = newLabels;
    pushLabelOrderUndo(imageIndex, oldLabels, sourceIndexes);
    refreshImageUi();
    selectLabelIndexes(newSelectedIndexes);
    markDirty();
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

    if (m_textEditUndoImageIndex == m_currentImageIndex && m_textEditUndoLabelIndex == m_currentLabelIndex) {
        pushLabelTextUndo(m_currentImageIndex, m_currentLabelIndex, m_textEditUndoOriginalText, newText);
        m_textEditUndoImageIndex = -1;
        m_textEditUndoLabelIndex = -1;
    }

    image->labels[m_currentLabelIndex].setText(newText);
    m_labelModel->labelChanged(m_currentLabelIndex);
    const int visibleRow = m_labelModel->rowForSourceIndex(m_currentLabelIndex);
    if (visibleRow >= 0) {
        m_labelView->resizeRowToContents(visibleRow);
        capLabelRowHeight(visibleRow);
    }
    markDirty();
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

    image->labels[m_currentLabelIndex].setGroup(newGroup);
    pushLabelGroupUndo(m_currentImageIndex, m_currentLabelIndex, oldGroup, newGroup);
    m_labelModel->refresh();
    m_canvas->setImage(image->path, image->labels);
    selectLabel(m_currentLabelIndex);
    markDirty();
}

void MainWindow::updateLabelFromTable(int sourceIndex, int column, QVariant oldValue, QVariant newValue)
{
    labelminus::core::ImageEntry* image = currentImage();
    if (image == nullptr || sourceIndex < 0 || sourceIndex >= image->labels.size()) {
        return;
    }

    if (column == 1) {
        pushLabelTextUndo(m_currentImageIndex, sourceIndex, oldValue.toString(), newValue.toString());
    }
    if (column == 2) {
        pushLabelGroupUndo(m_currentImageIndex, sourceIndex, oldValue.toString(), newValue.toString());
        m_labelModel->refresh();
        m_canvas->setImage(image->path, image->labels);
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

    const QPointF oldPosition = image->labels.at(index).position();
    image->labels[index].setPosition(normalizedPosition);
    const QPointF newPosition = image->labels.at(index).position();
    if (oldPosition == newPosition) {
        m_canvas->setImage(image->path, image->labels);
        selectLabel(index);
        return;
    }

    pushLabelPositionUndo(m_currentImageIndex, index, oldPosition, newPosition);
    m_canvas->setImage(image->path, image->labels);
    selectLabel(index);
    markDirty();
}

void MainWindow::undoLastOperation()
{
    m_undoStack.undo();
}

void MainWindow::applyLabelText(int imageIndex, int labelIndex, const QString& text)
{
    if (imageIndex < 0 || imageIndex >= m_project.images().size()) {
        return;
    }
    labelminus::core::ImageEntry& image = m_project.images()[imageIndex];
    if (labelIndex < 0 || labelIndex >= image.labels.size()) {
        return;
    }

    image.labels[labelIndex].setText(text);
    m_currentImageIndex = imageIndex;
    refreshImageUi();
    selectLabel(labelIndex);
    markDirty();
}

void MainWindow::applyLabelGroup(int imageIndex, int labelIndex, const QString& group)
{
    if (imageIndex < 0 || imageIndex >= m_project.images().size()) {
        return;
    }
    labelminus::core::ImageEntry& image = m_project.images()[imageIndex];
    if (labelIndex < 0 || labelIndex >= image.labels.size() || !m_project.groups().contains(group)) {
        return;
    }

    image.labels[labelIndex].setGroup(group);
    m_currentImageIndex = imageIndex;
    refreshImageUi();
    selectLabel(labelIndex);
    markDirty();
}

void MainWindow::applyLabelPosition(int imageIndex, int labelIndex, QPointF normalizedPosition)
{
    if (imageIndex < 0 || imageIndex >= m_project.images().size()) {
        return;
    }
    labelminus::core::ImageEntry& image = m_project.images()[imageIndex];
    if (labelIndex < 0 || labelIndex >= image.labels.size()) {
        return;
    }

    image.labels[labelIndex].setPosition(normalizedPosition);
    m_currentImageIndex = imageIndex;
    refreshImageUi();
    selectLabel(labelIndex);
    markDirty();
}

void MainWindow::applyLabelDeleted(int imageIndex, int labelIndex, bool deleted)
{
    if (imageIndex < 0 || imageIndex >= m_project.images().size()) {
        return;
    }
    labelminus::core::ImageEntry& image = m_project.images()[imageIndex];
    if (labelIndex < 0 || labelIndex >= image.labels.size()) {
        return;
    }

    image.labels[labelIndex].setDeleted(deleted);
    m_currentImageIndex = imageIndex;
    refreshImageUi();
    if (!deleted) {
        selectLabel(labelIndex);
    }
    else {
        m_currentLabelIndex = -1;
        setEditorEnabled(false);
    }
    markDirty();
}

void MainWindow::applyLabelOrder(int imageIndex, QVector<labelminus::core::Label> labels, QVector<int> selectedIndexes)
{
    if (imageIndex < 0 || imageIndex >= m_project.images().size()) {
        return;
    }

    m_project.images()[imageIndex].labels = std::move(labels);
    m_currentImageIndex = imageIndex;
    refreshImageUi();
    selectLabelIndexes(selectedIndexes);
    markDirty();
}

void MainWindow::applyBatchLabelGroups(int imageIndex, QVector<int> labelIndexes, QVector<QString> groups)
{
    if (imageIndex < 0 || imageIndex >= m_project.images().size() || labelIndexes.size() != groups.size()) {
        return;
    }

    labelminus::core::ImageEntry& image = m_project.images()[imageIndex];
    int lastValidIndex = -1;
    for (int i = 0; i < labelIndexes.size(); ++i) {
        const int labelIndex = labelIndexes.at(i);
        if (labelIndex < 0 || labelIndex >= image.labels.size() || !m_project.groups().contains(groups.at(i))) {
            continue;
        }
        image.labels[labelIndex].setGroup(groups.at(i));
        lastValidIndex = labelIndex;
    }

    m_currentImageIndex = imageIndex;
    refreshImageUi();
    if (lastValidIndex >= 0 && m_labelModel->rowForSourceIndex(lastValidIndex) >= 0) {
        selectLabel(lastValidIndex);
    }
    else {
        m_currentLabelIndex = -1;
        setEditorEnabled(false);
    }
    markDirty();
}

void MainWindow::applyBatchLabelDeleted(int imageIndex, QVector<int> labelIndexes, QVector<bool> deleted)
{
    if (imageIndex < 0 || imageIndex >= m_project.images().size() || labelIndexes.size() != deleted.size()) {
        return;
    }

    labelminus::core::ImageEntry& image = m_project.images()[imageIndex];
    int lastRestoredIndex = -1;
    for (int i = 0; i < labelIndexes.size(); ++i) {
        const int labelIndex = labelIndexes.at(i);
        if (labelIndex < 0 || labelIndex >= image.labels.size()) {
            continue;
        }
        image.labels[labelIndex].setDeleted(deleted.at(i));
        if (!deleted.at(i)) {
            lastRestoredIndex = labelIndex;
        }
    }

    m_currentImageIndex = imageIndex;
    refreshImageUi();
    if (lastRestoredIndex >= 0 && m_labelModel->rowForSourceIndex(lastRestoredIndex) >= 0) {
        selectLabel(lastRestoredIndex);
    }
    else {
        m_currentLabelIndex = -1;
        setEditorEnabled(false);
    }
    markDirty();
}

void MainWindow::pushLabelTextUndo(int imageIndex, int labelIndex, const QString& oldText, const QString& newText)
{
    if (oldText == newText) {
        return;
    }

    m_undoStack.push({
        tr("Edit label text"),
        [this, imageIndex, labelIndex, oldText]() { applyLabelText(imageIndex, labelIndex, oldText); },
    });
}

void MainWindow::pushLabelGroupUndo(int imageIndex, int labelIndex, const QString& oldGroup, const QString& newGroup)
{
    if (oldGroup == newGroup) {
        return;
    }

    m_undoStack.push({
        tr("Change label group"),
        [this, imageIndex, labelIndex, oldGroup]() { applyLabelGroup(imageIndex, labelIndex, oldGroup); },
    });
}

void MainWindow::pushLabelPositionUndo(int imageIndex, int labelIndex, QPointF oldPosition, QPointF newPosition)
{
    if (oldPosition == newPosition) {
        return;
    }

    m_undoStack.push({
        tr("Move label"),
        [this, imageIndex, labelIndex, oldPosition]() { applyLabelPosition(imageIndex, labelIndex, oldPosition); },
    });
}

void MainWindow::pushLabelOrderUndo(int imageIndex, QVector<labelminus::core::Label> oldLabels,
                                    QVector<int> oldSelectedIndexes)
{
    m_undoStack.push({
        tr("Reorder labels"),
        [this, imageIndex, oldLabels = std::move(oldLabels),
         oldSelectedIndexes = std::move(oldSelectedIndexes)]() mutable {
            applyLabelOrder(imageIndex, std::move(oldLabels), std::move(oldSelectedIndexes));
        },
    });
}

void MainWindow::pushBatchLabelGroupUndo(int imageIndex, QVector<int> labelIndexes, QVector<QString> oldGroups,
                                         QVector<QString> newGroups)
{
    if (labelIndexes.isEmpty() || labelIndexes.size() != oldGroups.size() || labelIndexes.size() != newGroups.size()) {
        return;
    }

    m_undoStack.push({
        tr("Change label group"),
        [this, imageIndex, labelIndexes = std::move(labelIndexes), oldGroups = std::move(oldGroups)]() mutable {
            applyBatchLabelGroups(imageIndex, std::move(labelIndexes), std::move(oldGroups));
        },
    });
}

void MainWindow::pushBatchLabelDeletedUndo(int imageIndex, QVector<int> labelIndexes, QVector<bool> oldDeleted,
                                           QVector<bool> newDeleted)
{
    if (labelIndexes.isEmpty() || labelIndexes.size() != oldDeleted.size() ||
        labelIndexes.size() != newDeleted.size()) {
        return;
    }

    m_undoStack.push({
        tr("Delete labels"),
        [this, imageIndex, labelIndexes = std::move(labelIndexes), oldDeleted = std::move(oldDeleted)]() mutable {
            applyBatchLabelDeleted(imageIndex, std::move(labelIndexes), std::move(oldDeleted));
        },
    });
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
    m_textEditUndoImageIndex = m_currentImageIndex;
    m_textEditUndoLabelIndex = primarySourceIndex;
    m_textEditUndoOriginalText = primaryLabel.text();
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
    for (const labelminus::core::ImageEntry& image : m_project.images()) {
        m_imageComboBox->addItem(image.name);
    }
    refreshGroupUi();
    m_isUpdatingUi = false;
    m_labelModel->setGroupFilter(m_project.groups());

    m_currentImageIndex = m_project.images().isEmpty() ? -1 : 0;
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
    m_nextButton->setEnabled(m_currentImageIndex >= 0 && m_currentImageIndex < m_project.images().size() - 1);
    m_canvas->setImage(image->path, image->labels);
    m_labelModel->setLabels(&m_project.images()[m_currentImageIndex].labels);
    resizeLabelRowsToContents();
    m_textEdit->clear();
    setEditorEnabled(false);
    m_isUpdatingUi = false;
}

void MainWindow::refreshGroupUi()
{
    m_isUpdatingUi = true;
    const QString previousInsertGroup = m_insertGroupComboBox->currentText();
    m_labelGroupComboBox->clear();
    m_insertGroupComboBox->clear();
    if (m_project.groups().isEmpty()) {
        m_project.setGroups({QStringLiteral("框内"), QStringLiteral("框外")});
    }
    m_groupFilterComboBox->setGroups(m_project.groups(), m_preferences.groupStyles());
    m_canvas->setGroups(m_project.groups());
    m_canvas->setVisibleGroups(m_groupFilterComboBox->selectedGroups());
    m_labelModel->setGroups(m_project.groups(), m_preferences.groupStyles());
    m_labelGroupDelegate->setGroups(m_project.groups(), m_preferences.groupStyles());
    m_labelGroupComboBox->addItems(m_project.groups());
    m_insertGroupComboBox->addItems(m_project.groups());
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
    QSettings settings;
    settings.beginGroup(layoutGroup);

    const QByteArray geometry = settings.value(geometryKey).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    const QByteArray windowState = settings.value(windowStateKey).toByteArray();
    if (!windowState.isEmpty()) {
        restoreState(windowState);
    }

    const QByteArray rootSplitterState = settings.value(rootSplitterKey).toByteArray();
    if (!rootSplitterState.isEmpty() && m_rootSplitter != nullptr) {
        m_rootSplitter->restoreState(rootSplitterState);
    }

    const QByteArray rightSplitterState = settings.value(rightSplitterKey).toByteArray();
    if (!rightSplitterState.isEmpty() && m_rightSplitter != nullptr) {
        m_rightSplitter->restoreState(rightSplitterState);
    }
}

void MainWindow::saveLayoutState() const
{
    QSettings settings;
    settings.beginGroup(layoutGroup);
    settings.setValue(geometryKey, saveGeometry());
    settings.setValue(windowStateKey, saveState());
    if (m_rootSplitter != nullptr) {
        settings.setValue(rootSplitterKey, m_rootSplitter->saveState());
    }
    if (m_rightSplitter != nullptr) {
        settings.setValue(rightSplitterKey, m_rightSplitter->saveState());
    }
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

    if (m_canvas != nullptr) {
        m_canvas->setPreferences(m_preferences);
    }

    refreshGroupUi();
    refreshImageUi();
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
    }

    return tr("Unknown preference warning.");
}

void MainWindow::markDirty()
{
    if (!m_isUpdatingUi) {
        setDirty(true);
    }
}

void MainWindow::setDirty(bool dirty)
{
    if (m_isDirty == dirty) {
        return;
    }

    m_isDirty = dirty;
    updateWindowTitle();
}

bool MainWindow::promptToSaveIfDirty()
{
    if (!m_isDirty) {
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
    if (!m_project.filePath().isEmpty()) {
        title += QStringLiteral(" - %1").arg(m_project.filePath());
    }
    if (m_isDirty) {
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
    const int index = static_cast<int>(m_project.groups().indexOf(group));
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

labelminus::core::ImageEntry* MainWindow::currentImage()
{
    if (m_currentImageIndex < 0 || m_currentImageIndex >= m_project.images().size()) {
        return nullptr;
    }
    return &m_project.images()[m_currentImageIndex];
}

const labelminus::core::ImageEntry* MainWindow::currentImage() const
{
    if (m_currentImageIndex < 0 || m_currentImageIndex >= m_project.images().size()) {
        return nullptr;
    }
    return &m_project.images().at(m_currentImageIndex);
}
