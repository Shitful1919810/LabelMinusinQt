#pragma once

#include "core/AppPreferences.h"
#include "core/Project.h"
#include "services/AutomationService.h"

#include <QMap>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class AutomationRunDialog;
class QAction;
class QMenu;
class QWidget;

class AutomationController final : public QObject {
    Q_OBJECT

public:
    struct Callbacks {
        std::function<bool()> isProjectEmpty;
        std::function<void()> commitActiveTextInput;
        std::function<const labelminus::core::Project&()> project;
        std::function<QStringList()> groups;
        std::function<QVector<labelminus::core::LabelGroupStyle>()> groupStyles;
        std::function<int()> currentImageIndex;
        std::function<labelminus::services::AutomationSelection()> selection;
        std::function<labelminus::services::AutomationContext()> context;
    };

    explicit AutomationController(QWidget* window, QObject* parent = nullptr);

    void setMenu(QMenu* menu);
    void setPreferences(labelminus::core::AppPreferences preferences);
    void setCallbacks(Callbacks callbacks);
    void refreshScripts();
    const QVector<labelminus::services::AutomationScript>& scripts() const noexcept;
    bool isRunning() const noexcept;

public slots:
    void runScriptById(const QString& scriptId);
    void showMissingScriptMessage(const QString& scriptId);
    void cancelRunningScript();

signals:
    void scriptsChanged(const QVector<labelminus::services::AutomationScript>& scripts);
    void discoveryWarningsFound(const QStringList& warnings);
    void runningChanged(bool running);
    void operationsReady(const QString& scriptName,
                         const QVector<labelminus::services::AutomationOperation>& operations);
    void statusMessageRequested(const QString& message, int timeoutMs);

private:
    void rebuildMenu();
    void updateMenuEnabledState();
    void runScript(const labelminus::services::AutomationScript& script);
    void finishScript(labelminus::services::AutomationRunner* runner, AutomationRunDialog* dialog,
                      labelminus::services::AutomationScript script,
                      const labelminus::services::AutomationRunResult& result);
    void setRunning(bool running);

    QWidget* m_window{nullptr};
    QMenu* m_menu{nullptr};
    QAction* m_cancelAction{nullptr};
    Callbacks m_callbacks;
    labelminus::core::AppPreferences m_preferences;
    QVector<labelminus::services::AutomationScript> m_scripts;
    QPointer<labelminus::services::AutomationRunner> m_runner;
    QPointer<AutomationRunDialog> m_runDialog;
    bool m_running{false};
};
