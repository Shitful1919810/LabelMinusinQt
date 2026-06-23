#include "ui/AutomationParameterDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

namespace {
constexpr auto pathEditorObjectName = "automationParameterLineEdit";

QString tr(const char* sourceText)
{
    return QCoreApplication::translate("AutomationParameterDialog", sourceText);
}

void applyGroupStyles(QComboBox* comboBox, const QVector<labelminus::core::LabelGroupStyle>& groupStyles)
{
    for (int i = 0; i < comboBox->count() && i < groupStyles.size(); ++i) {
        const QColor color = groupStyles.at(i).groupColor;
        if (color.isValid()) {
            comboBox->setItemData(i, color, Qt::ForegroundRole);
        }
    }
}

bool defaultBoolValue(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("true") || normalized == QStringLiteral("1") ||
           normalized == QStringLiteral("yes") || normalized == QStringLiteral("on");
}

QWidget* createPathEditor(QWidget* parent, const QString& defaultValue, const QString& parameterType)
{
    auto* container = new QWidget(parent);
    auto* pathLayout = new QHBoxLayout(container);
    pathLayout->setContentsMargins(0, 0, 0, 0);

    auto* lineEdit = new QLineEdit(defaultValue, container);
    lineEdit->setObjectName(QString::fromLatin1(pathEditorObjectName));
    auto* browseButton = new QPushButton(tr("Browse..."), container);
    pathLayout->addWidget(lineEdit);
    pathLayout->addWidget(browseButton);

    QObject::connect(browseButton, &QPushButton::clicked, container, [parent, lineEdit, parameterType]() {
        const QString currentPath = lineEdit->text();
        QString path;
        if (parameterType == QStringLiteral("directory")) {
            path = QFileDialog::getExistingDirectory(parent, tr("Choose Directory"), currentPath);
        }
        else {
            path = QFileDialog::getOpenFileName(parent, tr("Choose File"), currentPath);
        }
        if (!path.isEmpty()) {
            lineEdit->setText(path);
        }
    });

    return container;
}

QWidget* createEditor(QWidget* parent, const labelminus::services::AutomationParameter& parameter,
                      const QStringList& groups, const QVector<labelminus::core::LabelGroupStyle>& groupStyles)
{
    const QString parameterType = parameter.type.toLower();
    if (parameterType == QStringLiteral("group")) {
        auto* comboBox = new QComboBox(parent);
        comboBox->addItems(groups);
        if (!parameter.defaultValue.isEmpty()) {
            const int index = comboBox->findText(parameter.defaultValue);
            if (index >= 0) {
                comboBox->setCurrentIndex(index);
            }
        }
        applyGroupStyles(comboBox, groupStyles);
        return comboBox;
    }

    if (parameterType == QStringLiteral("choice") || parameterType == QStringLiteral("select") ||
        parameterType == QStringLiteral("enum")) {
        auto* comboBox = new QComboBox(parent);
        comboBox->addItems(parameter.options);
        if (!parameter.defaultValue.isEmpty()) {
            const int index = comboBox->findText(parameter.defaultValue);
            if (index >= 0) {
                comboBox->setCurrentIndex(index);
            }
        }
        return comboBox;
    }

    if (parameterType == QStringLiteral("bool") || parameterType == QStringLiteral("boolean")) {
        auto* checkBox = new QCheckBox(parent);
        checkBox->setChecked(defaultBoolValue(parameter.defaultValue));
        return checkBox;
    }

    if (parameterType == QStringLiteral("file") || parameterType == QStringLiteral("directory") ||
        parameterType == QStringLiteral("path")) {
        return createPathEditor(parent, parameter.defaultValue, parameterType);
    }

    return new QLineEdit(parameter.defaultValue, parent);
}

QJsonObject collectParameters(const QVector<QPair<labelminus::services::AutomationParameter, QWidget*>>& editors)
{
    QJsonObject parameters;
    for (const auto& [parameter, editor] : editors) {
        if (auto* comboBox = qobject_cast<QComboBox*>(editor)) {
            parameters.insert(parameter.key, comboBox->currentText());
        }
        else if (auto* checkBox = qobject_cast<QCheckBox*>(editor)) {
            parameters.insert(parameter.key, checkBox->isChecked());
        }
        else if (auto* lineEdit = qobject_cast<QLineEdit*>(editor)) {
            parameters.insert(parameter.key, lineEdit->text());
        }
        else if (auto* pathLineEdit = editor->findChild<QLineEdit*>(QString::fromLatin1(pathEditorObjectName))) {
            parameters.insert(parameter.key, pathLineEdit->text());
        }
    }
    return parameters;
}
} // namespace

std::optional<QJsonObject>
AutomationParameterDialog::getParameters(QWidget* parent, const labelminus::services::AutomationScript& script,
                                         const QStringList& groups,
                                         const QVector<labelminus::core::LabelGroupStyle>& groupStyles)
{
    if (script.parameters.isEmpty()) {
        return QJsonObject{};
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(tr("Automation Parameters"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* formLayout = new QFormLayout;
    layout->addLayout(formLayout);

    QVector<QPair<labelminus::services::AutomationParameter, QWidget*>> editors;
    editors.reserve(script.parameters.size());
    for (const labelminus::services::AutomationParameter& parameter : script.parameters) {
        QWidget* editor = createEditor(&dialog, parameter, groups, groupStyles);
        formLayout->addRow(parameter.label, editor);
        editors.append({parameter, editor});
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    return collectParameters(editors);
}
