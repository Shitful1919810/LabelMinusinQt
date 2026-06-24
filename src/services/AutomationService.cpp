#include "services/AutomationService.h"

#include "services/SecretStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>

namespace labelminus::services {

namespace {
constexpr int automationApiVersion = 1;
constexpr int automationTimeoutMs = 300000;
constexpr int automationProcessStopWaitMs = 3000;
constexpr int automationLogMaxCharacters = 1024 * 1024;

QJsonObject labelToJson(const labelminus::core::Label& label, int labelIndex, int visibleIndex)
{
    const QPointF position = label.position();
    return {
        {QStringLiteral("labelIndex"), labelIndex}, {QStringLiteral("visibleIndex"), visibleIndex},
        {QStringLiteral("group"), label.group()},   {QStringLiteral("x"), position.x()},
        {QStringLiteral("y"), position.y()},        {QStringLiteral("text"), label.text()},
    };
}

QJsonObject projectToJson(const labelminus::core::Project& project, int currentImageIndex)
{
    QJsonArray groups;
    for (const QString& group : project.groups()) {
        groups.append(group);
    }

    QJsonArray imagePaths;
    QJsonArray pages;
    for (int imageIndex = 0; imageIndex < project.images().size(); ++imageIndex) {
        const labelminus::core::ImageEntry& image = project.images().at(imageIndex);
        imagePaths.append(image.path);
        QJsonArray labels;
        int visibleIndex = 0;
        for (int labelIndex = 0; labelIndex < image.labels.size(); ++labelIndex) {
            const labelminus::core::Label& label = image.labels.at(labelIndex);
            if (label.isDeleted()) {
                continue;
            }
            labels.append(labelToJson(label, labelIndex, visibleIndex));
            ++visibleIndex;
        }

        pages.append(QJsonObject{
            {QStringLiteral("index"), imageIndex},
            {QStringLiteral("name"), image.name},
            {QStringLiteral("imagePath"), image.path},
            {QStringLiteral("labels"), labels},
        });
    }

    const QString currentPage = currentImageIndex >= 0 && currentImageIndex < project.images().size()
                                    ? project.images().at(currentImageIndex).name
                                    : QString();
    return {
        {QStringLiteral("path"), project.filePath()}, {QStringLiteral("sourceName"), project.sourceName()},
        {QStringLiteral("groups"), groups},           {QStringLiteral("currentPage"), currentPage},
        {QStringLiteral("imagePaths"), imagePaths},   {QStringLiteral("pages"), pages},
    };
}

QJsonObject selectionToJson(const labelminus::core::Project& project, int currentImageIndex,
                            AutomationSelection selection)
{
    const bool hasSelection = selection.hasSelection && currentImageIndex >= 0 &&
                              currentImageIndex < project.images().size() && selection.normalizedRect.width() > 0.0 &&
                              selection.normalizedRect.height() > 0.0;
    if (!hasSelection) {
        return {{QStringLiteral("hasSelection"), false}};
    }

    QRectF rect = selection.normalizedRect.normalized();
    const double left = std::clamp(rect.left(), 0.0, 1.0);
    const double top = std::clamp(rect.top(), 0.0, 1.0);
    const double right = std::clamp(rect.left() + rect.width(), 0.0, 1.0);
    const double bottom = std::clamp(rect.top() + rect.height(), 0.0, 1.0);
    rect = QRectF(QPointF(left, top), QPointF(right, bottom)).normalized();

    const labelminus::core::ImageEntry& image = project.images().at(currentImageIndex);
    return {
        {QStringLiteral("hasSelection"), true},
        {QStringLiteral("imageIndex"), currentImageIndex},
        {QStringLiteral("page"), image.name},
        {QStringLiteral("imagePath"), image.path},
        {QStringLiteral("rect"),
         QJsonObject{
             {QStringLiteral("x"), rect.left()},
             {QStringLiteral("y"), rect.top()},
             {QStringLiteral("width"), rect.width()},
             {QStringLiteral("height"), rect.height()},
             {QStringLiteral("left"), rect.left()},
             {QStringLiteral("top"), rect.top()},
             {QStringLiteral("right"), rect.left() + rect.width()},
             {QStringLiteral("bottom"), rect.top() + rect.height()},
         }},
    };
}

QJsonObject contextToJson(const labelminus::core::Project& project, AutomationContext context)
{
    const int currentImageIndex = context.currentImageIndex;
    QJsonObject currentPage{{QStringLiteral("hasPage"), false}};
    if (currentImageIndex >= 0 && currentImageIndex < project.images().size()) {
        const labelminus::core::ImageEntry& image = project.images().at(currentImageIndex);
        currentPage = {
            {QStringLiteral("hasPage"), true},
            {QStringLiteral("index"), currentImageIndex},
            {QStringLiteral("number"), currentImageIndex + 1},
            {QStringLiteral("name"), image.name},
            {QStringLiteral("imagePath"), image.path},
        };
    }

    QJsonArray selectedLabelIndexes;
    QJsonArray selectedLabels;
    if (currentImageIndex >= 0 && currentImageIndex < project.images().size()) {
        const labelminus::core::ImageEntry& image = project.images().at(currentImageIndex);
        for (int labelIndex : context.selectedLabelIndexes) {
            if (labelIndex < 0 || labelIndex >= image.labels.size() || image.labels.at(labelIndex).isDeleted()) {
                continue;
            }
            selectedLabelIndexes.append(labelIndex);
            selectedLabels.append(
                labelToJson(image.labels.at(labelIndex), labelIndex, static_cast<int>(selectedLabels.size())));
        }
    }

    return {
        {QStringLiteral("currentPage"), currentPage},
        {QStringLiteral("selectedLabelIndexes"), selectedLabelIndexes},
        {QStringLiteral("selectedLabels"), selectedLabels},
    };
}

QJsonObject inputPayload(const labelminus::core::Project& project, int currentImageIndex, const QJsonObject& parameters,
                         AutomationSelection selection, AutomationContext context)
{
    return {
        {QStringLiteral("apiVersion"), automationApiVersion},
        {QStringLiteral("scope"), QStringLiteral("project")},
        {QStringLiteral("options"), QJsonObject{{QStringLiteral("includeDeletedLabels"), false}}},
        {QStringLiteral("parameters"), parameters},
        {QStringLiteral("context"), contextToJson(project, context)},
        {QStringLiteral("selection"), selectionToJson(project, currentImageIndex, selection)},
        {QStringLiteral("project"), projectToJson(project, currentImageIndex)},
    };
}

QString stringFromJsonValue(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    return {};
}

QStringList pythonProgramCandidates()
{
    QStringList programs;
    const QByteArray configuredPython = qgetenv("LABELMINUS_PYTHON");
    if (!configuredPython.trimmed().isEmpty()) {
        programs.append(QString::fromLocal8Bit(configuredPython));
    }
#ifdef Q_OS_WIN
    programs.append(QStringLiteral("python"));
    programs.append(QStringLiteral("py"));
#else
    programs.append(QStringLiteral("python3"));
    programs.append(QStringLiteral("python"));
#endif
    programs.removeDuplicates();
    return programs;
}

bool writeJsonFile(const QString& path, const QJsonObject& object, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error != nullptr) {
            *error = file.errorString();
        }
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return true;
}

bool readJsonFile(const QString& path, QJsonObject* object, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = file.errorString();
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error != nullptr) {
            *error = parseError.errorString();
        }
        return false;
    }
    *object = document.object();
    return true;
}

QString pythonUnavailableError(const QStringList& candidates, const QString& lastError)
{
    const QString triedPrograms = candidates.join(QStringLiteral(", "));
    // clang-format off
    return QCoreApplication::translate("AutomationService", "Python was not found. Install Python 3 or set LABELMINUS_PYTHON to the Python executable path.\n\nTried: %1\nLast error: %2")
        .arg(triedPrograms, lastError);
    // clang-format on
}

QVector<AutomationParameter> parametersFromManifest(const QJsonObject& manifest)
{
    QVector<AutomationParameter> parameters;
    const QJsonArray parameterArray = manifest.value(QStringLiteral("parameters")).toArray();
    for (const QJsonValue& value : parameterArray) {
        const QJsonObject object = value.toObject();
        const QString key = object.value(QStringLiteral("key")).toString().trimmed();
        if (key.isEmpty()) {
            continue;
        }

        AutomationParameter parameter;
        parameter.key = key;
        parameter.label = object.value(QStringLiteral("label")).toString(key);
        parameter.type = object.value(QStringLiteral("type")).toString(QStringLiteral("text"));
        parameter.defaultValue = stringFromJsonValue(object.value(QStringLiteral("default")));
        parameter.secretKey = object.value(QStringLiteral("secretKey")).toString(key);
        parameter.secretService = object.value(QStringLiteral("service")).toString(QStringLiteral("LabelMinus"));
        parameter.secretAccount = object.value(QStringLiteral("account")).toString(parameter.secretKey);
        parameter.secretEnvironment = object.value(QStringLiteral("environment")).toString();
        const QJsonArray options = object.value(QStringLiteral("options")).toArray();
        for (const QJsonValue& option : options) {
            const QString optionText = stringFromJsonValue(option).trimmed();
            if (!optionText.isEmpty()) {
                parameter.options.append(optionText);
            }
        }
        parameters.append(parameter);
    }
    return parameters;
}

QVector<AutomationSecret> secretsFromManifest(const QJsonObject& manifest)
{
    QVector<AutomationSecret> secrets;
    const QJsonArray secretArray = manifest.value(QStringLiteral("secrets")).toArray();
    for (const QJsonValue& value : secretArray) {
        const QJsonObject object = value.toObject();
        const QString key = object.value(QStringLiteral("key")).toString().trimmed();
        const QString environment = object.value(QStringLiteral("environment")).toString().trimmed();
        if (key.isEmpty() || environment.isEmpty()) {
            continue;
        }

        AutomationSecret secret;
        secret.key = key;
        secret.label = object.value(QStringLiteral("label")).toString(key);
        secret.service = object.value(QStringLiteral("service")).toString(QStringLiteral("LabelMinus"));
        secret.account = object.value(QStringLiteral("account")).toString(key);
        secret.environment = environment;
        secret.required = object.value(QStringLiteral("required")).toBool(true);
        secrets.append(secret);
    }
    return secrets;
}

QMap<QString, QString> environmentFromManifest(const QJsonObject& manifest)
{
    QMap<QString, QString> environment;
    const QJsonObject object = manifest.value(QStringLiteral("environment")).toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!it.value().isString() || it.key().trimmed().isEmpty()) {
            continue;
        }
        environment.insert(it.key(), it.value().toString());
    }
    return environment;
}

QVector<AutomationOperation> operationsFromOutput(const QJsonObject& output)
{
    QVector<AutomationOperation> operations;
    const QJsonArray operationArray = output.value(QStringLiteral("operations")).toArray();
    for (const QJsonValue& value : operationArray) {
        const QJsonObject object = value.toObject();
        const QString type = object.value(QStringLiteral("type")).toString().trimmed();
        if (type.isEmpty()) {
            continue;
        }

        AutomationOperation operation;
        operation.type = type;
        operation.page = object.value(QStringLiteral("page")).toString();
        operation.labelIndex = object.value(QStringLiteral("labelIndex")).toInt(-1);
        operation.group = object.value(QStringLiteral("group")).toString();
        operation.text = object.value(QStringLiteral("text")).toString();
        operation.x = object.value(QStringLiteral("x")).toDouble(0.0);
        operation.y = object.value(QStringLiteral("y")).toDouble(0.0);
        operations.append(operation);
    }
    return operations;
}

AutomationRunResult resultFromOutput(const QJsonObject& output)
{
    AutomationRunResult result;
    result.success = true;
    result.summary = output.value(QStringLiteral("summary")).toString();
    result.operations = operationsFromOutput(output);
    result.quiet = output.value(QStringLiteral("quiet")).toBool(false);

    const QJsonObject resultObject = output.value(QStringLiteral("result")).toObject();
    result.resultTitle = resultObject.value(QStringLiteral("title")).toString();
    result.resultText = resultObject.value(QStringLiteral("text")).toString();
    if (result.resultText.isEmpty() && output.contains(QStringLiteral("message"))) {
        result.resultText = output.value(QStringLiteral("message")).toString();
    }
    return result;
}

QString scriptIdForDirectory(const QFileInfo& scriptDirectory, bool official)
{
    return QStringLiteral("%1:%2").arg(official ? QStringLiteral("official") : QStringLiteral("custom"),
                                       scriptDirectory.fileName());
}

QString scriptLocation(const QFileInfo& scriptDirectory, const QString& scriptName = {})
{
    if (scriptName.trimmed().isEmpty()) {
        return scriptDirectory.fileName();
    }
    return QStringLiteral("%1/%2").arg(scriptDirectory.fileName(), scriptName);
}

void appendLog(QString* log, const QString& text)
{
    if (log == nullptr || text.isEmpty()) {
        return;
    }
    log->append(text);
    if (log->size() > automationLogMaxCharacters) {
        const int keepCharacters = automationLogMaxCharacters / 2;
        *log = QCoreApplication::translate("AutomationService", "[Earlier automation log output was truncated.]\n") +
               log->right(keepCharacters);
    }
}

bool appendScriptFromManifest(QVector<AutomationScript>* scripts, const QFileInfo& scriptDirectory,
                              const QJsonObject& directoryManifest, const QJsonObject& scriptManifest, bool official,
                              int directoryIndex, int scriptIndex, QStringList* warnings)
{
    const QString entry = scriptManifest.value(QStringLiteral("entry")).toString();
    if (entry.trimmed().isEmpty()) {
        if (warnings != nullptr) {
            warnings->append(
                QCoreApplication::translate("AutomationService", "Skipped automation script %1: missing entry.")
                    .arg(scriptLocation(scriptDirectory, scriptManifest.value(QStringLiteral("name")).toString())));
        }
        return false;
    }

    const QString entryPath = QDir(scriptDirectory.absoluteFilePath()).filePath(entry);
    if (!QFileInfo::exists(entryPath)) {
        if (warnings != nullptr) {
            warnings->append(QCoreApplication::translate("AutomationService",
                                                         "Skipped automation script %1: entry file does not exist.")
                                 .arg(scriptLocation(scriptDirectory, entry)));
        }
        return false;
    }

    AutomationScript script;
    const QString scriptId = scriptManifest.value(QStringLiteral("id")).toString(QString::number(scriptIndex));
    script.id = QStringLiteral("%1:%2").arg(
        scriptIdForDirectory(scriptDirectory, official),
        directoryManifest.contains(QStringLiteral("scripts")) ? scriptId : QStringLiteral("single"));
    script.name = scriptManifest.value(QStringLiteral("name")).toString(scriptDirectory.fileName());
    script.description = scriptManifest.value(QStringLiteral("description"))
                             .toString(directoryManifest.value(QStringLiteral("description")).toString());
    script.directoryName = directoryManifest.value(QStringLiteral("name")).toString(scriptDirectory.fileName());
    script.directoryPath = scriptDirectory.absoluteFilePath();
    script.entryPath = entryPath;
    script.parameters = parametersFromManifest(scriptManifest);
    script.secrets = secretsFromManifest(scriptManifest);
    script.environment = environmentFromManifest(scriptManifest);
    script.official = official;
    script.directoryOrder = directoryIndex;
    script.scriptOrder = scriptIndex;
    scripts->append(script);
    return true;
}

} // namespace

QVector<AutomationScript> AutomationService::discoverScripts(QStringList* warnings)
{
    QVector<AutomationScript> scripts;
    const QStringList roots = scriptsRootCandidates();
    for (const QString& rootPath : roots) {
        const QDir root(rootPath);
        if (!root.exists()) {
            continue;
        }

        const bool official = root.dirName() == QStringLiteral("official");
        const QFileInfoList scriptDirectories = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        int directoryIndex = 0;
        for (const QFileInfo& scriptDirectory : scriptDirectories) {
            const QString manifestPath =
                QDir(scriptDirectory.absoluteFilePath()).filePath(QStringLiteral("script.json"));
            QJsonObject manifest;
            QString error;
            if (!readJsonFile(manifestPath, &manifest, &error)) {
                if (warnings != nullptr) {
                    warnings->append(
                        QCoreApplication::translate("AutomationService", "Skipped automation script directory %1: %2")
                            .arg(scriptDirectory.fileName(), error));
                }
                ++directoryIndex;
                continue;
            }

            const QJsonArray scriptArray = manifest.value(QStringLiteral("scripts")).toArray();
            if (!scriptArray.isEmpty()) {
                int scriptIndex = 0;
                for (const QJsonValue& scriptValue : scriptArray) {
                    appendScriptFromManifest(&scripts, scriptDirectory, manifest, scriptValue.toObject(), official,
                                             directoryIndex, scriptIndex, warnings);
                    ++scriptIndex;
                }
            }
            else {
                appendScriptFromManifest(&scripts, scriptDirectory, manifest, manifest, official, directoryIndex, 0,
                                         warnings);
            }
            ++directoryIndex;
        }
    }

    std::sort(scripts.begin(), scripts.end(), [](const AutomationScript& lhs, const AutomationScript& rhs) {
        if (lhs.official != rhs.official) {
            return lhs.official;
        }
        const int directoryCompare = QString::localeAwareCompare(lhs.directoryName, rhs.directoryName);
        if (directoryCompare != 0) {
            return directoryCompare < 0;
        }
        return lhs.scriptOrder < rhs.scriptOrder;
    });
    return scripts;
}

bool AutomationService::storeParameterSecrets(const AutomationScript& script, const QMap<QString, QString>& secrets,
                                              QString* error)
{
    for (const AutomationParameter& parameter : script.parameters) {
        if (parameter.type.compare(QStringLiteral("secret"), Qt::CaseInsensitive) != 0 ||
            !secrets.contains(parameter.secretKey)) {
            continue;
        }

        const SecretStoreWriteResult result = SecretStore::writeText(parameter.secretService, parameter.secretAccount,
                                                                     secrets.value(parameter.secretKey));
        if (!result.success) {
            if (error != nullptr) {
                *error = QCoreApplication::translate("AutomationService", "Failed to store automation secret %1: %2")
                             .arg(parameter.label, result.error);
            }
            return false;
        }
    }
    return true;
}

bool AutomationService::secretEnvironment(const AutomationScript& script, QMap<QString, QString>* environment,
                                          QString* error)
{
    if (environment != nullptr) {
        environment->clear();
    }
    for (const AutomationSecret& secret : script.secrets) {
        const SecretStoreReadResult result = SecretStore::readText(secret.service, secret.account);
        if (!result.error.isEmpty()) {
            if (error != nullptr) {
                *error = QCoreApplication::translate("AutomationService", "Failed to read automation secret %1: %2")
                             .arg(secret.label, result.error);
            }
            return false;
        }
        if (!result.found || result.value.isEmpty()) {
            if (secret.required) {
                if (error != nullptr) {
                    *error = QCoreApplication::translate(
                                 "AutomationService",
                                 "Automation secret %1 is not configured. Run the script's configuration first.")
                                 .arg(secret.label);
                }
                return false;
            }
            continue;
        }
        if (environment != nullptr) {
            environment->insert(secret.environment, result.value);
        }
    }
    return true;
}

AutomationRunner::AutomationRunner(QObject* parent) : QObject(parent)
{
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &AutomationRunner::handleTimeout);
}

AutomationRunner::~AutomationRunner()
{
    if (m_process != nullptr) {
        m_process->disconnect(this);
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(automationProcessStopWaitMs);
        }
    }
}

void AutomationRunner::start(const AutomationScript& script, const labelminus::core::Project& project,
                             int currentImageIndex, const QJsonObject& parameters, AutomationSelection selection,
                             AutomationContext context, const QMap<QString, QString>& environmentOverrides)
{
    if (m_running) {
        AutomationRunResult result;
        result.error = QStringLiteral("Automation script is already running.");
        finishWithResult(result);
        return;
    }

    m_script = script;
    m_environmentOverrides = environmentOverrides;
    m_pythonCandidates = pythonProgramCandidates();
    m_candidateIndex = 0;
    m_lastFailure = {};
    m_cancelRequested = false;
    m_running = true;

    m_temporaryDirectory = std::make_unique<QTemporaryDir>();
    if (!m_temporaryDirectory->isValid()) {
        AutomationRunResult result;
        result.error = QStringLiteral("Failed to create a temporary directory.");
        finishWithResult(result);
        return;
    }

    const QString inputPath = m_temporaryDirectory->filePath(QStringLiteral("input.json"));
    m_outputPath = m_temporaryDirectory->filePath(QStringLiteral("output.json"));
    QString error;
    if (!writeJsonFile(inputPath, inputPayload(project, currentImageIndex, parameters, selection, context), &error)) {
        AutomationRunResult result;
        result.error = QStringLiteral("Failed to write automation input: %1").arg(error);
        finishWithResult(result);
        return;
    }

    m_arguments = {script.entryPath, QStringLiteral("--input"), inputPath, QStringLiteral("--output"), m_outputPath};
    startNextCandidate();
}

void AutomationRunner::cancel()
{
    m_cancelRequested = true;
    if (m_process != nullptr && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
}

bool AutomationRunner::isRunning() const noexcept
{
    return m_running;
}

void AutomationRunner::startNextCandidate()
{
    while (m_candidateIndex < m_pythonCandidates.size()) {
        const QString program = m_pythonCandidates.at(m_candidateIndex);
        ++m_candidateIndex;

        if (m_process != nullptr) {
            m_process->deleteLater();
            m_process = nullptr;
        }

        m_process = new QProcess(this);
        m_process->setWorkingDirectory(m_script.directoryPath);
        QProcessEnvironment processEnvironment = QProcessEnvironment::systemEnvironment();
        for (auto it = m_script.environment.constBegin(); it != m_script.environment.constEnd(); ++it) {
            processEnvironment.insert(it.key(), it.value());
        }
        for (auto it = m_environmentOverrides.constBegin(); it != m_environmentOverrides.constEnd(); ++it) {
            processEnvironment.insert(it.key(), it.value());
        }
        m_process->setProcessEnvironment(processEnvironment);

        connect(m_process, &QProcess::readyReadStandardOutput, this, &AutomationRunner::appendProcessOutput);
        connect(m_process, &QProcess::readyReadStandardError, this, &AutomationRunner::appendProcessOutput);
        connect(m_process, &QProcess::started, this, &AutomationRunner::handleStarted);
        connect(m_process, &QProcess::errorOccurred, this, &AutomationRunner::handleError);
        connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                &AutomationRunner::handleFinished);

        m_process->start(program, m_arguments);
        if (m_process->waitForStarted(1000)) {
            m_timeoutTimer->start(automationTimeoutMs);
            return;
        }

        m_lastFailure.error = QStringLiteral("Failed to start %1: %2").arg(program, m_process->errorString());
        emit standardErrorReceived(m_lastFailure.error + QLatin1Char('\n'));
    }

    m_lastFailure.error = pythonUnavailableError(m_pythonCandidates, m_lastFailure.error);
    finishWithResult(m_lastFailure);
}

void AutomationRunner::handleStarted()
{
    if (m_process != nullptr) {
        emit standardOutputReceived(QStringLiteral("Started %1\n").arg(m_process->program()));
    }
}

void AutomationRunner::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    appendProcessOutput();
    if (m_timeoutTimer != nullptr) {
        m_timeoutTimer->stop();
    }

    if (m_cancelRequested) {
        AutomationRunResult result;
        result.error = QStringLiteral("Automation script was canceled.");
        if (m_process != nullptr) {
            appendLog(&result.standardOutput, QString::fromUtf8(m_process->readAllStandardOutput()));
            appendLog(&result.standardError, QString::fromUtf8(m_process->readAllStandardError()));
        }
        finishWithResult(result);
        return;
    }

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        m_lastFailure.error = QStringLiteral("Automation script failed with exit code %1.").arg(exitCode);
        finishWithResult(m_lastFailure);
        return;
    }

    QJsonObject output;
    QString error;
    if (!readJsonFile(m_outputPath, &output, &error)) {
        m_lastFailure.error = QStringLiteral("Failed to read automation output: %1").arg(error);
        finishWithResult(m_lastFailure);
        return;
    }

    AutomationRunResult result = resultFromOutput(output);
    result.standardOutput = m_lastFailure.standardOutput;
    result.standardError = m_lastFailure.standardError;
    finishWithResult(result);
}

void AutomationRunner::handleError()
{
    if (m_process == nullptr || m_process->state() != QProcess::NotRunning) {
        return;
    }
    m_lastFailure.error = m_process->errorString();
}

void AutomationRunner::handleTimeout()
{
    if (m_process != nullptr && m_process->state() != QProcess::NotRunning) {
        m_lastFailure.error = QStringLiteral("Automation script timed out.");
        emit standardErrorReceived(m_lastFailure.error + QLatin1Char('\n'));
        m_process->kill();
    }
}

void AutomationRunner::appendProcessOutput()
{
    if (m_process == nullptr) {
        return;
    }

    const QString standardOutput = QString::fromUtf8(m_process->readAllStandardOutput());
    if (!standardOutput.isEmpty()) {
        appendLog(&m_lastFailure.standardOutput, standardOutput);
        emit standardOutputReceived(standardOutput);
    }

    const QString standardError = QString::fromUtf8(m_process->readAllStandardError());
    if (!standardError.isEmpty()) {
        appendLog(&m_lastFailure.standardError, standardError);
        emit standardErrorReceived(standardError);
    }
}

void AutomationRunner::finishWithResult(AutomationRunResult result)
{
    if (!m_running && !result.success && result.error.isEmpty()) {
        return;
    }

    if (m_timeoutTimer != nullptr) {
        m_timeoutTimer->stop();
    }
    if (m_process != nullptr) {
        m_process->disconnect(this);
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        }
        m_process->deleteLater();
        m_process = nullptr;
    }

    m_running = false;
    emit finished(result);
    m_temporaryDirectory.reset();
}

QStringList AutomationService::scriptsRootCandidates()
{
    const QString applicationScriptsPath =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("scripts"));
    const QString scriptsPath = QDir(applicationScriptsPath).exists()
                                    ? applicationScriptsPath
                                    : QDir::current().filePath(QStringLiteral("scripts"));
    return {
        QDir(scriptsPath).filePath(QStringLiteral("official")),
        QDir(scriptsPath).filePath(QStringLiteral("custom")),
    };
}

} // namespace labelminus::services
