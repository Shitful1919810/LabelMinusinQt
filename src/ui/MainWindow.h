#pragma once

#include "core/AppPreferences.h"
#include "core/UndoStack.h"
#include "services/AutomationService.h"
#include "services/LabelEditController.h"
#include "services/ProjectController.h"
#include "services/ProjectWorkflowController.h"
#include "services/SessionStateStore.h"
#include "ui/GroupFilterComboBox.h"
#include "ui/ImageCanvas.h"
#include "ui/LabelTableModel.h"

#include <QColor>
#include <QFont>
#include <QMainWindow>
#include <QPersistentModelIndex>
#include <QPointF>
#include <QPointer>
#include <QVariant>
#include <QVector>

#include <memory>

class QCloseEvent;
class QAction;
class QComboBox;
class QLabel;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QSplitter;
class QTableView;
class QTimer;
class QToolButton;
class AutomationController;
class AutomationShortcutController;
class CanvasLabelTextEditController;
class LabelGroupDelegate;
class LabelTextDelegate;
class MainWindowShortcutController;
class ViewportFittedTableColumns;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool openProjectFile(const QString& path);
    bool openMostRecentProject();

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    enum class ActiveTextInputMode {
        None,
        BottomEditor,
        TableTextEditor,
        CanvasTextEditor,
    };

    void createActions();
    void createMenus();
    void createCentralWidget();
    void newProject();
    void openProject();
    void mergeProjects();
    void reorderPages();
    void showAutomationDiscoveryWarnings(const QStringList& warnings);
    void applyAutomationOperations(const QString& scriptName,
                                   const QVector<labelminus::services::AutomationOperation>& operations);
    void openRecentProjectFromAction();
    void openPreferences();
    bool saveProject();
    bool saveProjectAs();
    void addGroup();
    void removeGroup();
    void updateGroupFilter(const QStringList& groups);
    void selectImage(int index);
    void selectLabel(int index);
    void selectLabelFromCanvas(int index, Qt::KeyboardModifiers modifiers);
    void addLabel(QPointF normalizedPosition);
    void deleteSelectedLabels();
    void changeSelectedLabelsGroup(const QString& group);
    void showLabelContextMenu(const QPoint& position);
    void reorderLabels(QVector<int> sourceIndexes, int visibleDropRow);
    void updateCurrentLabelText();
    bool updateCurrentLabelDetails(int index);
    ActiveTextInputMode activeTextInputMode() const;
    void commitActiveTextInput();
    void restoreTextInputModeAfterLabelNavigation(ActiveTextInputMode mode);
    void commitPendingTextEdit();
    void resetPendingTextEdit();
    void refreshLabelTableView();
    void refreshLabelViews();
    void clearCurrentLabelSelection();
    void updateCurrentLabelGroup(int index);
    void updateLabelFromTable(int sourceIndex, int column, QVariant newValue);
    void previewLabelTextFromTableEditor(QPersistentModelIndex index, const QString& text);
    void clearLabelTextPreviewFromTableEditor(QPersistentModelIndex index);
    void openCanvasLabelTextEditor(int index, QPoint globalPosition);
    void commitCanvasLabelTextEditor();
    void cancelCanvasLabelTextEditor();
    void closeCanvasLabelTextEditor();
    void moveLabel(int index, QPointF normalizedPosition);
    void undoLastOperation();
    void redoLastOperation();
    void updateEditShortcuts();
    void updateUndoRedoActions();
    void showUndoRedoMessage(const QString& prefix, const QString& message);
    QString richUndoRedoMessage(QString message) const;
    void selectPreviousPage();
    void selectNextPage();
    void selectNextVisibleLabel();
    void selectPreviousVisibleLabel();
    void selectNextVisibleLabelFrom(int imageIndex, int labelIndex);
    void selectPreviousVisibleLabelFrom(int imageIndex, int labelIndex);
    void selectAdjacentVisibleLabelFromShortcut(bool previous);
    void editCurrentLabelText();
    void openCanvasLabelTextEditorForCurrentLabel();
    void selectLabelAndCenter(int imageIndex, int labelIndex);
    bool isLabelVisibleByGroupFilter(const labelminus::core::Label& label) const;
    QVector<int> selectedLabelIndexes() const;
    void selectLabelIndexes(const QVector<int>& sourceIndexes, int primarySourceIndex = -1);
    void refreshProjectUi();
    void detachProjectViewsFromProjectData();
    void replaceProjectImages(QVector<labelminus::core::ImageEntry> images, const QString& preferredImageName,
                              int fallbackImageIndex, int zoomPercent, QPointF normalizedCenter);
    void refreshImageUi();
    void refreshCanvasLabels();
    void refreshCurrentLabelUi();
    void refreshLabelEditSelection(int imageIndex, int labelIndex);
    void refreshLabelEditSelection(int imageIndex, QVector<int> labelIndexes);
    void clearLabelEditSelection(int imageIndex);
    void refreshGroupUi();
    void resizeLabelRowsToContents();
    void capLabelRowHeight(int row);
    void restoreLayoutState();
    void saveLayoutState() const;
    void restoreLabelTableColumnWidths();
    void saveLabelTableColumnWidths() const;
    void restoreProjectSessionState();
    void saveProjectSessionState() const;
    void configureBackupTimer();
    void performAutoBackup();
    void applyLabelTableFont();
    void applyTextEditorFont();
    void showPreferenceWarnings();
    void applyPreferences(labelminus::core::AppPreferencesLoadResult result);
    QString preferenceWarningText(const labelminus::core::AppPreferenceWarning& warning) const;
    QStringList defaultProjectGroups() const;
    void markDirty();
    void setDirty(bool dirty);
    bool promptToSaveIfDirty();
    void updateWindowTitle();
    bool loadProjectFile(const QString& path, bool showErrors, const QString& successMessage);
    void updateRecentProjectsMenu();
    void applyGroupStylesToCombo(QComboBox* comboBox);
    void updateInsertGroupTextColor();
    QColor colorForGroup(const QString& group) const;
    int labelCountForGroup(const QString& group) const;
    void setEditorEnabled(bool enabled);
    void setAutomationRunning(bool running);
    labelminus::core::Project& project() noexcept;
    const labelminus::core::Project& project() const noexcept;
    labelminus::core::ImageEntry* currentImage();
    const labelminus::core::ImageEntry* currentImage() const;

    ImageCanvas* m_canvas{nullptr};
    LabelTableModel* m_labelModel{nullptr};
    LabelTextDelegate* m_labelTextDelegate{nullptr};
    LabelGroupDelegate* m_labelGroupDelegate{nullptr};
    QTableView* m_labelView{nullptr};
    ViewportFittedTableColumns* m_labelTableColumns{nullptr};
    QPlainTextEdit* m_textEdit{nullptr};
    CanvasLabelTextEditController* m_canvasTextEditController{nullptr};
    MainWindowShortcutController* m_shortcutController{nullptr};
    AutomationController* m_automationController{nullptr};
    AutomationShortcutController* m_automationShortcutController{nullptr};
    QComboBox* m_imageComboBox{nullptr};
    QComboBox* m_insertGroupComboBox{nullptr};
    GroupFilterComboBox* m_groupFilterComboBox{nullptr};
    QComboBox* m_labelGroupComboBox{nullptr};
    QLabel* m_warningLabel{nullptr};
    QLabel* m_operationMessageLabel{nullptr};
    QSlider* m_zoomSlider{nullptr};
    QSplitter* m_rootSplitter{nullptr};
    QSplitter* m_rightSplitter{nullptr};
    QTimer* m_backupTimer{nullptr};
    QAction* m_openProjectAction{nullptr};
    QAction* m_newProjectAction{nullptr};
    QAction* m_mergeProjectsAction{nullptr};
    QAction* m_reorderPagesAction{nullptr};
    QAction* m_saveProjectAction{nullptr};
    QAction* m_saveProjectAsAction{nullptr};
    QMenu* m_recentProjectsMenu{nullptr};
    QAction* m_undoAction{nullptr};
    QAction* m_redoAction{nullptr};
    QAction* m_previousPageAction{nullptr};
    QAction* m_nextPageAction{nullptr};
    QAction* m_preferencesAction{nullptr};
    QAction* m_quitAction{nullptr};
    QToolButton* m_labelModeButton{nullptr};
    QToolButton* m_selectionModeButton{nullptr};
    QPushButton* m_previousButton{nullptr};
    QPushButton* m_nextButton{nullptr};
    labelminus::services::ProjectController m_projectController;
    std::unique_ptr<labelminus::services::ProjectWorkflowController> m_projectWorkflowController;
    std::unique_ptr<labelminus::services::LabelEditController> m_labelEditController;
    labelminus::core::AppPreferences m_preferences;
    QVector<labelminus::core::AppPreferenceWarning> m_preferenceWarnings;
    int m_currentImageIndex{-1};
    int m_currentLabelIndex{-1};
    bool m_isUpdatingUi{false};
    bool m_suppressNextTableCommitSelection{false};
    int m_pendingTextEditImageIndex{-1};
    int m_pendingTextEditLabelIndex{-1};
    QString m_pendingTextEditOldText;
    int m_labelTableMaxTextRows{3};
    QFont m_defaultLabelTableFont;
    QFont m_defaultTextEditFont;
    labelminus::services::SessionStateStore m_sessionStateStore;
    labelminus::core::UndoStack m_undoStack;
    int m_operationMessageSerial{0};
    bool m_isAutomationRunning{false};
};
