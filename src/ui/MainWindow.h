#pragma once

#include "core/AppPreferences.h"
#include "core/Project.h"
#include "core/UndoStack.h"
#include "ui/GroupFilterComboBox.h"
#include "ui/ImageCanvas.h"
#include "ui/LabelTableModel.h"

#include <QColor>
#include <QMainWindow>

class QCloseEvent;
class QAction;
class QComboBox;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QTableView;
class QToolButton;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createActions();
    void createMenus();
    void createCentralWidget();
    void openProject();
    bool saveProject();
    bool saveProjectAs();
    void addGroup();
    void removeGroup();
    void updateGroupFilter(const QStringList& groups);
    void selectImage(int index);
    void selectLabel(int index);
    void addLabel(QPointF normalizedPosition);
    void updateCurrentLabelText();
    void updateCurrentLabelGroup(int index);
    void undoLastOperation();
    void refreshProjectUi();
    void refreshImageUi();
    void refreshGroupUi();
    void markDirty();
    void setDirty(bool dirty);
    bool promptToSaveIfDirty();
    void updateWindowTitle();
    void applyGroupColorsToCombo(QComboBox* comboBox);
    void updateInsertGroupTextColor();
    QColor colorForGroup(const QString& group) const;
    void setEditorEnabled(bool enabled);
    labelminus::core::ImageEntry* currentImage();
    const labelminus::core::ImageEntry* currentImage() const;

    ImageCanvas* m_canvas{nullptr};
    LabelTableModel* m_labelModel{nullptr};
    QTableView* m_labelView{nullptr};
    QPlainTextEdit* m_textEdit{nullptr};
    QComboBox* m_imageComboBox{nullptr};
    QComboBox* m_insertGroupComboBox{nullptr};
    GroupFilterComboBox* m_groupFilterComboBox{nullptr};
    QComboBox* m_labelGroupComboBox{nullptr};
    QSlider* m_zoomSlider{nullptr};
    QAction* m_openProjectAction{nullptr};
    QAction* m_saveProjectAction{nullptr};
    QAction* m_saveProjectAsAction{nullptr};
    QAction* m_quitAction{nullptr};
    QPushButton* m_previousButton{nullptr};
    QPushButton* m_nextButton{nullptr};
    labelminus::core::Project m_project;
    labelminus::core::AppPreferences m_preferences;
    int m_currentImageIndex{-1};
    int m_currentLabelIndex{-1};
    bool m_isUpdatingUi{false};
    bool m_isDirty{false};
    labelminus::core::UndoStack m_undoStack;
};
