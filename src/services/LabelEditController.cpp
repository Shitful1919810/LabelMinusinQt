#include "services/LabelEditController.h"

#include <algorithm>
#include <utility>

namespace labelminus::services {

namespace {
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

LabelEditController::LabelEditController(labelminus::core::Project& project, labelminus::core::UndoStack& undoStack,
                                         LabelEditCommandTexts commandTexts)
    : m_project(project), m_undoStack(undoStack), m_commandTexts(std::move(commandTexts))
{
}

void LabelEditController::setCallbacks(LabelSelectedCallback labelSelected, LabelsSelectedCallback labelsSelected,
                                       ImageSelectionClearedCallback imageSelectionCleared, DirtyCallback dirty)
{
    m_labelSelected = std::move(labelSelected);
    m_labelsSelected = std::move(labelsSelected);
    m_imageSelectionCleared = std::move(imageSelectionCleared);
    m_dirty = std::move(dirty);
}

LabelEditResult LabelEditController::addLabel(int imageIndex, const labelminus::core::Label& label)
{
    labelminus::core::ImageEntry* image = imageAt(imageIndex);
    if (image == nullptr) {
        return {};
    }

    image->labels.append(label);
    const int labelIndex = static_cast<int>(image->labels.size()) - 1;
    const labelminus::core::Label addedLabel = image->labels.last();
    m_undoStack.push(
        m_commandTexts.addLabel,
        [this, imageIndex, labelIndex, addedLabel]() {
            labelminus::core::ImageEntry* targetImage = imageAt(imageIndex);
            if (targetImage == nullptr || labelIndex < 0 || labelIndex >= targetImage->labels.size()) {
                return;
            }

            const labelminus::core::Label& currentLabel = targetImage->labels.at(labelIndex);
            if (!labelEquals(currentLabel, addedLabel)) {
                return;
            }

            targetImage->labels.removeAt(labelIndex);
            if (m_imageSelectionCleared) {
                m_imageSelectionCleared(imageIndex);
            }
            markDirty();
        },
        [this, imageIndex, labelIndex, addedLabel]() {
            labelminus::core::ImageEntry* targetImage = imageAt(imageIndex);
            if (targetImage == nullptr || labelIndex < 0 || labelIndex > targetImage->labels.size()) {
                return;
            }

            targetImage->labels.insert(labelIndex, addedLabel);
            if (m_labelSelected) {
                m_labelSelected(imageIndex, labelIndex);
            }
            markDirty();
        });

    markDirty();
    return {true, labelIndex, {}, {labelIndex}};
}

LabelEditResult LabelEditController::deleteLabels(int imageIndex, const QVector<int>& labelIndexes)
{
    labelminus::core::ImageEntry* image = imageAt(imageIndex);
    if (image == nullptr || labelIndexes.isEmpty()) {
        return {};
    }

    QVector<int> changedIndexes;
    QVector<bool> oldDeleted;
    for (int labelIndex : labelIndexes) {
        if (labelIndex < 0 || labelIndex >= image->labels.size() || image->labels.at(labelIndex).isDeleted()) {
            continue;
        }
        changedIndexes.append(labelIndex);
        oldDeleted.append(false);
        image->labels[labelIndex].setDeleted(true);
    }

    if (changedIndexes.isEmpty()) {
        return {};
    }

    m_undoStack.push(
        m_commandTexts.deleteLabels,
        [this, imageIndex, labelIndexes = changedIndexes, oldDeleted]() {
            applyBatchLabelDeleted(imageIndex, labelIndexes, oldDeleted);
        },
        [this, imageIndex, labelIndexes = changedIndexes]() {
            applyBatchLabelDeleted(imageIndex, labelIndexes, QVector<bool>(labelIndexes.size(), true));
        });
    markDirty();
    return {true, -1, {}, changedIndexes};
}

LabelEditResult LabelEditController::changeLabelsGroup(int imageIndex, const QVector<int>& labelIndexes,
                                                       const QString& group)
{
    labelminus::core::ImageEntry* image = imageAt(imageIndex);
    if (image == nullptr || labelIndexes.isEmpty() || !hasGroup(group)) {
        return {};
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
        return {};
    }

    m_undoStack.push(
        m_commandTexts.changeLabelGroup,
        [this, imageIndex, labelIndexes = changedIndexes, oldGroups]() {
            applyBatchLabelGroups(imageIndex, labelIndexes, oldGroups);
        },
        [this, imageIndex, labelIndexes = changedIndexes, newGroups]() {
            applyBatchLabelGroups(imageIndex, labelIndexes, newGroups);
        });
    markDirty();
    return {true, changedIndexes.last(), {}, changedIndexes};
}

LabelEditResult LabelEditController::reorderLabels(int imageIndex, QVector<int> sourceIndexes,
                                                   int insertBeforeSourceIndex)
{
    labelminus::core::ImageEntry* image = imageAt(imageIndex);
    if (image == nullptr || sourceIndexes.isEmpty()) {
        return {};
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
        return {};
    }

    insertBeforeSourceIndex = std::clamp(insertBeforeSourceIndex, 0, static_cast<int>(image->labels.size()));

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
        return {};
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
        return {};
    }

    image->labels = newLabels;
    m_undoStack.push(
        m_commandTexts.reorderLabels,
        [this, imageIndex, oldLabels, sourceIndexes]() { applyLabelOrder(imageIndex, oldLabels, sourceIndexes); },
        [this, imageIndex, newLabels, newSelectedIndexes]() {
            applyLabelOrder(imageIndex, newLabels, newSelectedIndexes);
        });
    markDirty();
    return {true, -1, newSelectedIndexes, newSelectedIndexes};
}

LabelEditResult LabelEditController::setLabelText(int imageIndex, int labelIndex, const QString& text,
                                                  bool registerUndo)
{
    labelminus::core::ImageEntry* image = imageAt(imageIndex);
    if (image == nullptr || labelIndex < 0 || labelIndex >= image->labels.size()) {
        return {};
    }

    const QString oldText = image->labels.at(labelIndex).text();
    if (oldText == text) {
        return {};
    }

    if (registerUndo) {
        registerLabelTextUndo(imageIndex, labelIndex, oldText, text);
    }
    image->labels[labelIndex].setText(text);
    markDirty();
    return {true, labelIndex, {}, {labelIndex}};
}

LabelEditResult LabelEditController::setLabelGroup(int imageIndex, int labelIndex, const QString& group,
                                                   bool registerUndo)
{
    labelminus::core::ImageEntry* image = imageAt(imageIndex);
    if (image == nullptr || labelIndex < 0 || labelIndex >= image->labels.size() || !hasGroup(group)) {
        return {};
    }

    const QString oldGroup = image->labels.at(labelIndex).group();
    if (oldGroup == group) {
        return {};
    }

    if (registerUndo) {
        registerLabelGroupUndo(imageIndex, labelIndex, oldGroup, group);
    }
    image->labels[labelIndex].setGroup(group);
    markDirty();
    return {true, labelIndex, {}, {labelIndex}};
}

LabelEditResult LabelEditController::setLabelPosition(int imageIndex, int labelIndex, QPointF normalizedPosition,
                                                      bool registerUndo)
{
    labelminus::core::ImageEntry* image = imageAt(imageIndex);
    if (image == nullptr || labelIndex < 0 || labelIndex >= image->labels.size()) {
        return {};
    }

    const QPointF oldPosition = image->labels.at(labelIndex).position();
    image->labels[labelIndex].setPosition(normalizedPosition);
    const QPointF newPosition = image->labels.at(labelIndex).position();
    if (oldPosition == newPosition) {
        return {};
    }

    if (registerUndo) {
        m_undoStack.push(
            m_commandTexts.moveLabel,
            [this, imageIndex, labelIndex, oldPosition]() { applyLabelPosition(imageIndex, labelIndex, oldPosition); },
            [this, imageIndex, labelIndex, newPosition]() { applyLabelPosition(imageIndex, labelIndex, newPosition); });
    }
    markDirty();
    return {true, labelIndex, {}, {labelIndex}};
}

void LabelEditController::registerLabelTextUndo(int imageIndex, int labelIndex, const QString& oldText,
                                                const QString& newText)
{
    if (oldText == newText) {
        return;
    }

    m_undoStack.push(
        m_commandTexts.editLabelText,
        [this, imageIndex, labelIndex, oldText]() { applyLabelText(imageIndex, labelIndex, oldText); },
        [this, imageIndex, labelIndex, newText]() { applyLabelText(imageIndex, labelIndex, newText); });
}

void LabelEditController::registerLabelGroupUndo(int imageIndex, int labelIndex, const QString& oldGroup,
                                                 const QString& newGroup)
{
    if (oldGroup == newGroup) {
        return;
    }

    m_undoStack.push(
        m_commandTexts.changeLabelGroup,
        [this, imageIndex, labelIndex, oldGroup]() { applyLabelGroup(imageIndex, labelIndex, oldGroup); },
        [this, imageIndex, labelIndex, newGroup]() { applyLabelGroup(imageIndex, labelIndex, newGroup); });
}

void LabelEditController::applyLabelText(int imageIndex, int labelIndex, const QString& text)
{
    labelminus::core::ImageEntry* image = imageAt(imageIndex);
    if (image == nullptr || labelIndex < 0 || labelIndex >= image->labels.size()) {
        return;
    }

    image->labels[labelIndex].setText(text);
    if (m_labelSelected) {
        m_labelSelected(imageIndex, labelIndex);
    }
    markDirty();
}

void LabelEditController::applyLabelGroup(int imageIndex, int labelIndex, const QString& group)
{
    labelminus::core::ImageEntry* image = imageAt(imageIndex);
    if (image == nullptr || labelIndex < 0 || labelIndex >= image->labels.size() || !hasGroup(group)) {
        return;
    }

    image->labels[labelIndex].setGroup(group);
    if (m_labelSelected) {
        m_labelSelected(imageIndex, labelIndex);
    }
    markDirty();
}

void LabelEditController::applyLabelPosition(int imageIndex, int labelIndex, QPointF normalizedPosition)
{
    labelminus::core::ImageEntry* image = imageAt(imageIndex);
    if (image == nullptr || labelIndex < 0 || labelIndex >= image->labels.size()) {
        return;
    }

    image->labels[labelIndex].setPosition(normalizedPosition);
    if (m_labelSelected) {
        m_labelSelected(imageIndex, labelIndex);
    }
    markDirty();
}

void LabelEditController::applyLabelOrder(int imageIndex, QVector<labelminus::core::Label> labels,
                                          QVector<int> selectedIndexes)
{
    labelminus::core::ImageEntry* image = imageAt(imageIndex);
    if (image == nullptr) {
        return;
    }

    image->labels = std::move(labels);
    if (m_labelsSelected) {
        m_labelsSelected(imageIndex, std::move(selectedIndexes));
    }
    markDirty();
}

void LabelEditController::applyBatchLabelGroups(int imageIndex, QVector<int> labelIndexes, QVector<QString> groups)
{
    labelminus::core::ImageEntry* image = imageAt(imageIndex);
    if (image == nullptr || labelIndexes.size() != groups.size()) {
        return;
    }

    int lastValidIndex = -1;
    for (int i = 0; i < labelIndexes.size(); ++i) {
        const int labelIndex = labelIndexes.at(i);
        if (labelIndex < 0 || labelIndex >= image->labels.size() || !hasGroup(groups.at(i))) {
            continue;
        }
        image->labels[labelIndex].setGroup(groups.at(i));
        lastValidIndex = labelIndex;
    }

    if (lastValidIndex >= 0 && m_labelSelected) {
        m_labelSelected(imageIndex, lastValidIndex);
    }
    else if (m_imageSelectionCleared) {
        m_imageSelectionCleared(imageIndex);
    }
    markDirty();
}

void LabelEditController::applyBatchLabelDeleted(int imageIndex, QVector<int> labelIndexes, QVector<bool> deleted)
{
    labelminus::core::ImageEntry* image = imageAt(imageIndex);
    if (image == nullptr || labelIndexes.size() != deleted.size()) {
        return;
    }

    int lastRestoredIndex = -1;
    for (int i = 0; i < labelIndexes.size(); ++i) {
        const int labelIndex = labelIndexes.at(i);
        if (labelIndex < 0 || labelIndex >= image->labels.size()) {
            continue;
        }
        image->labels[labelIndex].setDeleted(deleted.at(i));
        if (!deleted.at(i)) {
            lastRestoredIndex = labelIndex;
        }
    }

    if (lastRestoredIndex >= 0 && m_labelSelected) {
        m_labelSelected(imageIndex, lastRestoredIndex);
    }
    else if (m_imageSelectionCleared) {
        m_imageSelectionCleared(imageIndex);
    }
    markDirty();
}

labelminus::core::ImageEntry* LabelEditController::imageAt(int imageIndex)
{
    if (imageIndex < 0 || imageIndex >= m_project.images().size()) {
        return nullptr;
    }
    return &m_project.images()[imageIndex];
}

const labelminus::core::ImageEntry* LabelEditController::imageAt(int imageIndex) const
{
    if (imageIndex < 0 || imageIndex >= m_project.images().size()) {
        return nullptr;
    }
    return &m_project.images().at(imageIndex);
}

bool LabelEditController::hasGroup(const QString& group) const
{
    return m_project.groups().contains(group);
}

void LabelEditController::markDirty()
{
    if (m_dirty) {
        m_dirty();
    }
}

} // namespace labelminus::services
