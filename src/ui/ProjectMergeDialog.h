#pragma once

#include "core/AppPreferences.h"
#include "core/Project.h"
#include "services/ProjectMergeService.h"

#include <QDialog>
#include <QPointF>
#include <QPointer>
#include <QVector>

class ImageCanvas;
class QLabel;
class QListWidget;
class QScrollArea;
class QStackedWidget;

class ProjectMergeDialog final : public QDialog {
    Q_OBJECT

public:
    ProjectMergeDialog(labelminus::services::ProjectMergePlan mergePlan, labelminus::core::AppPreferences preferences,
                       QWidget* parent = nullptr);

    labelminus::core::Project mergedProject() const;

public slots:
    void done(int result) override;

private:
    void buildUi();
    void populateConflictList();
    QWidget* createConflictPage(int conflictIndex);
    QWidget* createCandidateWidget(int conflictIndex, int candidateIndex);
    void updateSummary();
    void selectCandidate(int conflictIndex, int candidateIndex);
    void syncCandidateCanvasViews(int conflictIndex, ImageCanvas* sourceCanvas, int zoomPercent,
                                  QPointF normalizedCenter);
    void disconnectCandidateCanvases();

    labelminus::services::ProjectMergePlan m_mergePlan;
    labelminus::core::AppPreferences m_preferences;
    QVector<int> m_selectedCandidateIndexes;
    QVector<QVector<QPointer<ImageCanvas>>> m_candidateCanvases;
    QLabel* m_summaryLabel{nullptr};
    QListWidget* m_conflictList{nullptr};
    QStackedWidget* m_conflictStack{nullptr};
    bool m_isSyncingCanvasViews{false};
    bool m_isClosing{false};
};
