#include "services/SessionStateStore.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QSettings>
#include <QStringView>

namespace labelminus::services {

namespace {
constexpr QLatin1StringView layoutGroup{"layout"};
constexpr QLatin1StringView geometryKey{"geometry"};
constexpr QLatin1StringView windowStateKey{"windowState"};
constexpr QLatin1StringView rootSplitterKey{"rootSplitter"};
constexpr QLatin1StringView rightSplitterKey{"rightSplitter"};
constexpr QLatin1StringView projectSessionsGroup{"projectSessions"};
constexpr QLatin1StringView sessionFilePathKey{"filePath"};
constexpr QLatin1StringView sessionImageIndexKey{"imageIndex"};
constexpr QLatin1StringView sessionImageNameKey{"imageName"};
constexpr QLatin1StringView sessionZoomPercentKey{"zoomPercent"};
constexpr QLatin1StringView sessionViewCenterXKey{"viewCenterX"};
constexpr QLatin1StringView sessionViewCenterYKey{"viewCenterY"};
constexpr QLatin1StringView sessionSelectedLabelIndexKey{"selectedLabelIndex"};
} // namespace

WindowLayoutState SessionStateStore::loadWindowLayout() const
{
    QSettings settings;
    settings.beginGroup(layoutGroup);
    return {
        settings.value(geometryKey).toByteArray(),
        settings.value(windowStateKey).toByteArray(),
        settings.value(rootSplitterKey).toByteArray(),
        settings.value(rightSplitterKey).toByteArray(),
    };
}

void SessionStateStore::saveWindowLayout(const WindowLayoutState& state) const
{
    QSettings settings;
    settings.beginGroup(layoutGroup);
    settings.setValue(geometryKey, state.geometry);
    settings.setValue(windowStateKey, state.windowState);
    settings.setValue(rootSplitterKey, state.rootSplitterState);
    settings.setValue(rightSplitterKey, state.rightSplitterState);
}

ProjectSessionState SessionStateStore::loadProjectSession(const QString& projectPath) const
{
    QSettings settings;
    settings.beginGroup(projectSessionsGroup);
    settings.beginGroup(projectSessionGroupName(projectPath));

    if (settings.value(sessionFilePathKey).toString() != canonicalSessionPath(projectPath)) {
        return {};
    }

    ProjectSessionState state;
    state.isValid = true;
    state.imageIndex = settings.value(sessionImageIndexKey, 0).toInt();
    state.imageName = settings.value(sessionImageNameKey).toString();
    state.zoomPercent = settings.value(sessionZoomPercentKey, 100).toInt();
    state.viewCenter = QPointF(settings.value(sessionViewCenterXKey, 0.5).toDouble(),
                               settings.value(sessionViewCenterYKey, 0.5).toDouble());
    state.selectedLabelIndex = settings.value(sessionSelectedLabelIndexKey, -1).toInt();
    return state;
}

void SessionStateStore::saveProjectSession(const QString& projectPath, const ProjectSessionState& state) const
{
    if (projectPath.isEmpty()) {
        return;
    }

    QSettings settings;
    settings.beginGroup(projectSessionsGroup);
    settings.beginGroup(projectSessionGroupName(projectPath));
    settings.setValue(sessionFilePathKey, canonicalSessionPath(projectPath));
    settings.setValue(sessionImageIndexKey, state.imageIndex);
    settings.setValue(sessionImageNameKey, state.imageName);
    settings.setValue(sessionZoomPercentKey, state.zoomPercent);
    settings.setValue(sessionViewCenterXKey, state.viewCenter.x());
    settings.setValue(sessionViewCenterYKey, state.viewCenter.y());
    settings.setValue(sessionSelectedLabelIndexKey, state.selectedLabelIndex);
}

QString SessionStateStore::canonicalSessionPath(const QString& path)
{
    const QFileInfo fileInfo(path);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    return canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
}

QString SessionStateStore::projectSessionGroupName(const QString& path)
{
    const QByteArray hash =
        QCryptographicHash::hash(canonicalSessionPath(path).toUtf8(), QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(hash);
}

} // namespace labelminus::services
