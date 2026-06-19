#pragma once

#include "core/AppPreferences.h"
#include "core/Project.h"
#include "core/UndoStack.h"
#include "ui/GroupFilterComboBox.h"
#include "ui/ImageCanvas.h"
#include "ui/LabelTableModel.h"

#include <QColor>
#include <QFont>
#include <QMainWindow>
#include <QPointF>
#include <QVariant>
#include <QVector>

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
class LabelGroupDelegate;
class LabelTextDelegate;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    bool openProjectFile(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createActions();
    void createMenus();
    void createCentralWidget();
    void newProject();
    void openProject();
    void openPreferences();
    bool saveProject();
    bool saveProjectAs();
    void addGroup();
    void removeGroup();
    void updateGroupFilter(const QStringList& groups);
    void selectImage(int index);
    void selectLabel(int index);
    void addLabel(QPointF normalizedPosition);
    void deleteSelectedLabels();
    void changeSelectedLabelsGroup(const QString& group);
    void showLabelContextMenu(const QPoint& position);
    void reorderLabels(QVector<int> sourceIndexes, int visibleDropRow);
    void updateCurrentLabelText();
    void updateCurrentLabelGroup(int index);
    void updateLabelFromTable(int sourceIndex, int column, QVariant oldValue, QVariant newValue);
    void moveLabel(int index, QPointF normalizedPosition);
    void undoLastOperation();
    void applyLabelText(int imageIndex, int labelIndex, const QString& text);
    void applyLabelGroup(int imageIndex, int labelIndex, const QString& group);
    void applyLabelPosition(int imageIndex, int labelIndex, QPointF normalizedPosition);
    void applyLabelDeleted(int imageIndex, int labelIndex, bool deleted);
    void applyLabelOrder(int imageIndex, QVector<labelminus::core::Label> labels, QVector<int> selectedIndexes);
    void applyBatchLabelGroups(int imageIndex, QVector<int> labelIndexes, QVector<QString> groups);
    void applyBatchLabelDeleted(int imageIndex, QVector<int> labelIndexes, QVector<bool> deleted);
    void pushLabelTextUndo(int imageIndex, int labelIndex, const QString& oldText, const QString& newText);
    void pushLabelGroupUndo(int imageIndex, int labelIndex, const QString& oldGroup, const QString& newGroup);
    void pushLabelPositionUndo(int imageIndex, int labelIndex, QPointF oldPosition, QPointF newPosition);
    void pushLabelOrderUndo(int imageIndex, QVector<labelminus::core::Label> oldLabels,
                            QVector<int> oldSelectedIndexes);
    void pushBatchLabelGroupUndo(int imageIndex, QVector<int> labelIndexes, QVector<QString> oldGroups,
                                 QVector<QString> newGroups);
    void pushBatchLabelDeletedUndo(int imageIndex, QVector<int> labelIndexes, QVector<bool> oldDeleted,
                                   QVector<bool> newDeleted);
    QVector<int> selectedLabelIndexes() const;
    void selectLabelIndexes(const QVector<int>& sourceIndexes);
    void refreshProjectUi();
    void refreshImageUi();
    void refreshGroupUi();
    void resizeLabelRowsToContents();
    void capLabelRowHeight(int row);
    void restoreLayoutState();
    void saveLayoutState() const;
    void configureBackupTimer();
    void performAutoBackup();
    void applyLabelTableFont();
    void applyTextEditorFont();
    void showPreferenceWarnings();
    void applyPreferences(labelminus::core::AppPreferencesLoadResult result);
    QString preferenceWarningText(const labelminus::core::AppPreferenceWarning& warning) const;
    void markDirty();
    void setDirty(bool dirty);
    bool promptToSaveIfDirty();
    void updateWindowTitle();
    void applyGroupStylesToCombo(QComboBox* comboBox);
    void updateInsertGroupTextColor();
    QColor colorForGroup(const QString& group) const;
    void setEditorEnabled(bool enabled);
    labelminus::core::ImageEntry* currentImage();
    const labelminus::core::ImageEntry* currentImage() const;

    ImageCanvas* m_canvas{nullptr};
    LabelTableModel* m_labelModel{nullptr};
    LabelTextDelegate* m_labelTextDelegate{nullptr};
    LabelGroupDelegate* m_labelGroupDelegate{nullptr};
    QTableView* m_labelView{nullptr};
    QPlainTextEdit* m_textEdit{nullptr};
    QComboBox* m_imageComboBox{nullptr};
    QComboBox* m_insertGroupComboBox{nullptr};
    GroupFilterComboBox* m_groupFilterComboBox{nullptr};
    QComboBox* m_labelGroupComboBox{nullptr};
    QLabel* m_warningLabel{nullptr};
    QSlider* m_zoomSlider{nullptr};
    QSplitter* m_rootSplitter{nullptr};
    QSplitter* m_rightSplitter{nullptr};
    QTimer* m_backupTimer{nullptr};
    QAction* m_openProjectAction{nullptr};
    QAction* m_newProjectAction{nullptr};
    QAction* m_saveProjectAction{nullptr};
    QAction* m_saveProjectAsAction{nullptr};
    QAction* m_preferencesAction{nullptr};
    QAction* m_quitAction{nullptr};
    QPushButton* m_previousButton{nullptr};
    QPushButton* m_nextButton{nullptr};
    labelminus::core::Project m_project;
    labelminus::core::AppPreferences m_preferences;
    QVector<labelminus::core::AppPreferenceWarning> m_preferenceWarnings;
    int m_currentImageIndex{-1};
    int m_currentLabelIndex{-1};
    bool m_isUpdatingUi{false};
    bool m_isDirty{false};
    bool m_hasPendingBackup{false};
    int m_textEditUndoImageIndex{-1};
    int m_textEditUndoLabelIndex{-1};
    QString m_textEditUndoOriginalText;
    int m_labelTableMaxTextRows{3};
    QFont m_defaultLabelTableFont;
    QFont m_defaultTextEditFont;
    labelminus::core::UndoStack m_undoStack;
};
