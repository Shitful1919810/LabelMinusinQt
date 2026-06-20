#pragma once

#include <QByteArray>
#include <QPointF>
#include <QString>

namespace labelminus::services {

struct WindowLayoutState {
    QByteArray geometry;
    QByteArray windowState;
    QByteArray rootSplitterState;
    QByteArray rightSplitterState;
};

struct ProjectSessionState {
    bool isValid{false};
    QString imageName;
    int imageIndex{0};
    int zoomPercent{100};
    QPointF viewCenter{0.5, 0.5};
    int selectedLabelIndex{-1};
};

class SessionStateStore {
public:
    WindowLayoutState loadWindowLayout() const;
    void saveWindowLayout(const WindowLayoutState& state) const;

    ProjectSessionState loadProjectSession(const QString& projectPath) const;
    void saveProjectSession(const QString& projectPath, const ProjectSessionState& state) const;

private:
    static QString canonicalSessionPath(const QString& path);
    static QString projectSessionGroupName(const QString& path);
};

} // namespace labelminus::services
