#pragma once

#include "core/Label.h"
#include "core/Project.h"
#include "core/UndoStack.h"

#include <QPointF>
#include <QString>
#include <QVector>

#include <functional>

namespace labelminus::services {

struct LabelEditResult {
    bool changed{false};
    int selectedLabelIndex{-1};
    QVector<int> selectedLabelIndexes;
    QVector<int> changedLabelIndexes;
};

struct LabelEditCommandTexts {
    QString addLabel;
    QString editLabelText;
    QString changeLabelGroup;
    QString moveLabel;
    QString deleteLabels;
    QString reorderLabels;
    QString addGroup;
    QString removeGroup;
};

class LabelEditController {
public:
    using LabelSelectedCallback = std::function<void(int imageIndex, int labelIndex)>;
    using LabelsSelectedCallback = std::function<void(int imageIndex, QVector<int> labelIndexes)>;
    using ImageSelectionClearedCallback = std::function<void(int imageIndex)>;
    using ProjectChangedCallback = std::function<void()>;
    using DirtyCallback = std::function<void()>;

    LabelEditController(labelminus::core::Project& project, labelminus::core::UndoStack& undoStack,
                        LabelEditCommandTexts commandTexts);

    void setCallbacks(LabelSelectedCallback labelSelected, LabelsSelectedCallback labelsSelected,
                      ImageSelectionClearedCallback imageSelectionCleared, ProjectChangedCallback projectChanged,
                      DirtyCallback dirty);

    LabelEditResult addGroup(const QString& group);
    LabelEditResult removeGroup(const QString& group, const QString& fallbackGroup);
    LabelEditResult addLabel(int imageIndex, const labelminus::core::Label& label);
    LabelEditResult deleteLabels(int imageIndex, const QVector<int>& labelIndexes);
    LabelEditResult changeLabelsGroup(int imageIndex, const QVector<int>& labelIndexes, const QString& group);
    LabelEditResult reorderLabels(int imageIndex, QVector<int> sourceIndexes, int insertBeforeSourceIndex);
    LabelEditResult setLabelText(int imageIndex, int labelIndex, const QString& text, bool registerUndo = true);
    LabelEditResult setLabelGroup(int imageIndex, int labelIndex, const QString& group, bool registerUndo = true);
    LabelEditResult setLabelPosition(int imageIndex, int labelIndex, QPointF normalizedPosition,
                                     bool registerUndo = true);
    void registerLabelTextUndo(int imageIndex, int labelIndex, const QString& oldText, const QString& newText);
    void registerLabelGroupUndo(int imageIndex, int labelIndex, const QString& oldGroup, const QString& newGroup);

private:
    void applyLabelText(int imageIndex, int labelIndex, const QString& text);
    void applyLabelGroup(int imageIndex, int labelIndex, const QString& group);
    void applyLabelPosition(int imageIndex, int labelIndex, QPointF normalizedPosition);
    void applyLabelOrder(int imageIndex, QVector<labelminus::core::Label> labels, QVector<int> selectedIndexes);
    void applyBatchLabelGroups(int imageIndex, QVector<int> labelIndexes, QVector<QString> groups);
    void applyBatchLabelDeleted(int imageIndex, QVector<int> labelIndexes, QVector<bool> deleted);
    void applyGroupsAndLabelGroups(QStringList groups, QVector<QVector<QString>> labelGroups);
    QVector<QVector<QString>> currentLabelGroups() const;
    labelminus::core::ImageEntry* imageAt(int imageIndex);
    const labelminus::core::ImageEntry* imageAt(int imageIndex) const;
    bool hasGroup(const QString& group) const;
    void markDirty();

    labelminus::core::Project& m_project;
    labelminus::core::UndoStack& m_undoStack;
    LabelEditCommandTexts m_commandTexts;
    LabelSelectedCallback m_labelSelected;
    LabelsSelectedCallback m_labelsSelected;
    ImageSelectionClearedCallback m_imageSelectionCleared;
    ProjectChangedCallback m_projectChanged;
    DirtyCallback m_dirty;
};

} // namespace labelminus::services
