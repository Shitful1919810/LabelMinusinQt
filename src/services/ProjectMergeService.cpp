#include "services/ProjectMergeService.h"

#include "core/LabelPlusDocument.h"

#include <QFileInfo>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace labelminus::services {
namespace {
struct LoadedProject {
    QString path;
    labelminus::core::Project project;
};

struct PageMergeData {
    labelminus::core::ImageEntry firstImage;
    QVector<ProjectMergeCandidate> candidates;
};

void appendUniqueGroup(QStringList& groups, const QString& group)
{
    if (!group.isEmpty() && !groups.contains(group)) {
        groups.append(group);
    }
}

QString displayPath(const QString& path)
{
    return QFileInfo(path).absoluteFilePath();
}

labelminus::core::ImageEntry withoutDeletedLabels(labelminus::core::ImageEntry image)
{
    image.labels.erase(std::remove_if(image.labels.begin(), image.labels.end(),
                                      [](const labelminus::core::Label& label) { return label.isDeleted(); }),
                       image.labels.end());
    return image;
}
} // namespace

ProjectMergePlan ProjectMergeService::createPlan(const QStringList& projectPaths)
{
    if (projectPaths.isEmpty()) {
        return {};
    }

    QVector<LoadedProject> loadedProjects;
    loadedProjects.reserve(projectPaths.size());
    for (const QString& path : projectPaths) {
        LoadedProject loaded;
        loaded.path = displayPath(path);
        loaded.project = labelminus::core::LabelPlusDocument::loadFromFile(path);
        loadedProjects.append(std::move(loaded));
    }

    ProjectMergePlan plan;
    QStringList imageOrder;
    QHash<QString, PageMergeData> pageDataByName;
    QStringList mergedGroups;

    for (int projectIndex = 0; projectIndex < loadedProjects.size(); ++projectIndex) {
        const LoadedProject& loaded = loadedProjects.at(projectIndex);
        for (const QString& group : loaded.project.groups()) {
            appendUniqueGroup(mergedGroups, group);
        }
        if (plan.mergedProject.sourceName().isEmpty()) {
            plan.mergedProject.setSourceName(loaded.project.sourceName());
        }

        for (const labelminus::core::ImageEntry& image : loaded.project.images()) {
            const QString imageName = image.name.isEmpty() ? QFileInfo(image.path).fileName() : image.name;
            if (imageName.isEmpty()) {
                continue;
            }

            if (!pageDataByName.contains(imageName)) {
                PageMergeData pageData;
                pageData.firstImage = image;
                pageData.firstImage.name = imageName;
                pageDataByName.insert(imageName, std::move(pageData));
                imageOrder.append(imageName);
            }

            const int labelCount = visibleLabelCount(image);
            if (labelCount <= 0) {
                continue;
            }

            ProjectMergeCandidate candidate;
            candidate.projectIndex = projectIndex;
            candidate.projectPath = loaded.path;
            candidate.image = withoutDeletedLabels(image);
            candidate.image.name = imageName;
            candidate.labelCount = labelCount;
            pageDataByName[imageName].candidates.append(std::move(candidate));
        }
    }

    if (mergedGroups.isEmpty()) {
        mergedGroups = {QStringLiteral("框内"), QStringLiteral("框外")};
    }
    plan.mergedProject.setGroups(mergedGroups);

    std::sort(imageOrder.begin(), imageOrder.end(), [](const QString& lhs, const QString& rhs) {
        return QString::compare(lhs, rhs, Qt::CaseInsensitive) < 0;
    });

    for (const QString& imageName : imageOrder) {
        PageMergeData& pageData = pageDataByName[imageName];
        labelminus::core::ImageEntry mergedImage = pageData.firstImage;
        mergedImage.name = imageName;
        mergedImage.labels.clear();

        if (pageData.candidates.size() == 1) {
            mergedImage = pageData.candidates.first().image;
        }
        else if (pageData.candidates.size() > 1) {
            ProjectMergeConflict conflict;
            conflict.imageName = imageName;
            conflict.candidates = pageData.candidates;
            conflict.selectedCandidateIndex = 0;
            plan.conflicts.append(std::move(conflict));
            mergedImage = pageData.candidates.first().image;
        }

        plan.mergedProject.images().append(std::move(mergedImage));
    }

    return plan;
}

labelminus::core::Project ProjectMergeService::mergedProjectWithSelections(ProjectMergePlan plan,
                                                                           const QVector<int>& selectedCandidateIndexes)
{
    QHash<QString, labelminus::core::ImageEntry> selectedImagesByName;
    for (int conflictIndex = 0; conflictIndex < plan.conflicts.size(); ++conflictIndex) {
        ProjectMergeConflict& conflict = plan.conflicts[conflictIndex];
        int selectedCandidateIndex = conflict.selectedCandidateIndex;
        if (conflictIndex < selectedCandidateIndexes.size()) {
            selectedCandidateIndex = selectedCandidateIndexes.at(conflictIndex);
        }
        if (selectedCandidateIndex < 0 || selectedCandidateIndex >= conflict.candidates.size()) {
            selectedCandidateIndex = 0;
        }

        labelminus::core::ImageEntry selectedImage = conflict.candidates.at(selectedCandidateIndex).image;
        selectedImage.name = conflict.imageName;
        selectedImagesByName.insert(conflict.imageName, std::move(selectedImage));
    }

    for (labelminus::core::ImageEntry& image : plan.mergedProject.images()) {
        const QString imageName = image.name.isEmpty() ? QFileInfo(image.path).fileName() : image.name;
        if (selectedImagesByName.contains(imageName)) {
            image = selectedImagesByName.value(imageName);
        }
    }

    return std::move(plan.mergedProject);
}

int ProjectMergeService::visibleLabelCount(const labelminus::core::ImageEntry& image)
{
    int count = 0;
    for (const labelminus::core::Label& label : image.labels) {
        if (!label.isDeleted()) {
            ++count;
        }
    }
    return count;
}

} // namespace labelminus::services
