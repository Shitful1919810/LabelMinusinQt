#include "ui/MainWindow.h"

#include "core/LabelPlusDocument.h"

#include <QAbstractItemView>
#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_labelModel(new LabelTableModel(this)),
      m_preferences(labelminus::core::AppPreferences::load())
{
    setWindowTitle(QStringLiteral("LabelMinus"));
    resize(1200, 800);

    createActions();
    createMenus();
    createCentralWidget();
    setEditorEnabled(false);
    statusBar()->showMessage(QStringLiteral("Ready"));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!promptToSaveIfDirty()) {
        event->ignore();
        return;
    }

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
    fileMenu->addAction(m_quitAction);
}

void MainWindow::createCentralWidget()
{
    auto* rootSplitter = new QSplitter(Qt::Horizontal, this);
    rootSplitter->setChildrenCollapsible(false);

    auto* leftPanel = new QWidget(rootSplitter);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(8, 8, 8, 8);
    leftLayout->setSpacing(6);

    m_canvas = new ImageCanvas(leftPanel);
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
    addGroupButton->setText(tr("+"));
    removeGroupButton->setText(tr("-"));
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

    auto* rightPanel = new QWidget(rootSplitter);
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

    auto* rightSplitter = new QSplitter(Qt::Vertical, rightPanel);
    rightSplitter->setChildrenCollapsible(false);

    m_labelView = new QTableView(rightSplitter);
    m_labelView->setModel(m_labelModel);
    m_labelView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_labelView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_labelView->setWordWrap(true);
    m_labelView->verticalHeader()->setVisible(false);
    m_labelView->horizontalHeader()->setStretchLastSection(false);
    m_labelView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_labelView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_labelView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_labelView->setAlternatingRowColors(true);

    auto* editorPanel = new QWidget(rightSplitter);
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
    rightSplitter->setStretchFactor(0, 3);
    rightSplitter->setStretchFactor(1, 1);
    rightLayout->addWidget(rightSplitter, 1);

    rootSplitter->addWidget(leftPanel);
    rootSplitter->addWidget(rightPanel);
    rootSplitter->setStretchFactor(0, 3);
    rootSplitter->setStretchFactor(1, 2);
    setCentralWidget(rootSplitter);

    connect(m_canvas, &ImageCanvas::labelCreateRequested, this, &MainWindow::addLabel);
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
    connect(m_textEdit, &QPlainTextEdit::textChanged, this, &MainWindow::updateCurrentLabelText);
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

    try {
        m_project = labelminus::core::LabelPlusDocument::loadFromFile(path);
        m_undoStack.clear();
        setDirty(false);
        refreshProjectUi();
        statusBar()->showMessage(tr("Loaded %1").arg(path), 4000);
    }
    catch (const std::exception& error) {
        QMessageBox::critical(this, tr("Open failed"), QString::fromUtf8(error.what()));
    }
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
    m_labelView->resizeRowsToContents();

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
    m_labelGroupComboBox->setCurrentText(image->labels.at(index).group());
    const int visibleRow = m_labelModel->rowForSourceIndex(index);
    if (visibleRow >= 0) {
        m_labelView->selectRow(visibleRow);
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

void MainWindow::updateCurrentLabelText()
{
    if (m_isUpdatingUi) {
        return;
    }

    labelminus::core::ImageEntry* image = currentImage();
    if (image == nullptr || m_currentLabelIndex < 0 || m_currentLabelIndex >= image->labels.size()) {
        return;
    }

    image->labels[m_currentLabelIndex].setText(m_textEdit->toPlainText());
    m_labelModel->labelChanged(m_currentLabelIndex);
    m_labelView->resizeRowToContents(m_currentLabelIndex);
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

    image->labels[m_currentLabelIndex].setGroup(m_labelGroupComboBox->itemText(index));
    m_labelModel->refresh();
    refreshImageUi();
    selectLabel(m_currentLabelIndex);
    markDirty();
}

void MainWindow::undoLastOperation()
{
    m_undoStack.undo();
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
    m_labelView->resizeRowsToContents();
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
    m_groupFilterComboBox->setGroups(m_project.groups(), m_preferences.groupColors());
    m_canvas->setGroups(m_project.groups());
    m_labelModel->setGroups(m_project.groups(), m_preferences.groupColors());
    m_labelGroupComboBox->addItems(m_project.groups());
    m_insertGroupComboBox->addItems(m_project.groups());
    applyGroupColorsToCombo(m_insertGroupComboBox);
    if (!previousInsertGroup.isEmpty()) {
        m_insertGroupComboBox->setCurrentText(previousInsertGroup);
    }
    updateInsertGroupTextColor();
    m_isUpdatingUi = false;
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

void MainWindow::applyGroupColorsToCombo(QComboBox* comboBox)
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
    if (index < 0 || index >= static_cast<int>(m_preferences.groupColors().size())) {
        return {};
    }
    return m_preferences.groupColors().at(index);
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
