#pragma once

#include "core/Project.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace labelminus::services {

struct ProjectMergeCandidate {
    int projectIndex{-1};
    QString projectPath;
    labelminus::core::ImageEntry image;
    int labelCount{0};
};

struct ProjectMergeConflict {
    QString imageName;
    QVector<ProjectMergeCandidate> candidates;
    int selectedCandidateIndex{0};
};

struct ProjectMergePageSource {
    QString imageName;
    int projectIndex{-1};
    QString projectPath;
    int labelCount{0};
};

struct ProjectMergePlan {
    labelminus::core::Project mergedProject;
    QVector<ProjectMergeConflict> conflicts;
    QVector<ProjectMergePageSource> pageSources;
    QStringList warnings;
};

class ProjectMergeService final {
public:
    static ProjectMergePlan createPlan(const QStringList& projectPaths);
    static labelminus::core::Project mergedProjectWithSelections(ProjectMergePlan plan,
                                                                 const QVector<int>& selectedCandidateIndexes,
                                                                 const QString& outputProjectPath = {},
                                                                 const QVector<int>& imageOrder = {});

private:
    static int visibleLabelCount(const labelminus::core::ImageEntry& image);
};

} // namespace labelminus::services
