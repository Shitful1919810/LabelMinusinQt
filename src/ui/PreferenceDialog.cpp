#include "ui/PreferenceDialog.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {
constexpr int colorColumn = 0;
constexpr int diameterColumn = 1;
constexpr int fontColumn = 2;
constexpr int shapeColumn = 3;

QColor colorFromStyleObject(const QJsonObject& style)
{
    const QJsonValue value = style.value(QStringLiteral("groupColor"));
    if (value.isString()) {
        const QColor color(value.toString());
        return color.isValid() ? color : QColor(QStringLiteral("#000000"));
    }
    return QColor(QStringLiteral("#000000"));
}

QString markerStyleFromObject(const QJsonObject& style)
{
    const QString markerStyle = style.value(QStringLiteral("markerStyle")).toString(QStringLiteral("circle"));
    return markerStyle == QStringLiteral("square") ? QStringLiteral("square") : QStringLiteral("circle");
}

QDoubleSpinBox* makePositiveDoubleSpinBox(double value, double minimum, double maximum)
{
    auto* spinBox = new QDoubleSpinBox;
    spinBox->setRange(minimum, maximum);
    spinBox->setDecimals(2);
    spinBox->setSingleStep(1.0);
    spinBox->setValue(value);
    return spinBox;
}
} // namespace

PreferenceDialog::PreferenceDialog(QString preferencePath, QWidget* parent)
    : QDialog(parent), m_preferencePath(std::move(preferencePath))
{
    createUi();
    loadFromDisk();
}

void PreferenceDialog::createUi()
{
    setWindowTitle(tr("Preferences"));
    resize(760, 620);

    auto* rootLayout = new QVBoxLayout(this);

    auto* pathLabel = new QLabel(tr("Preference file: %1").arg(m_preferencePath), this);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rootLayout->addWidget(pathLabel);

    auto* tabWidget = new QTabWidget(this);

    auto* generalPage = new QWidget(tabWidget);
    auto* generalLayout = new QFormLayout(generalPage);

    m_markerDiameterSpinBox = makePositiveDoubleSpinBox(20.0, 1.0, 256.0);
    m_markerFontSpinBox = makePositiveDoubleSpinBox(10.0, 0.1, 256.0);
    m_tableMaxRowsSpinBox = new QSpinBox(generalPage);
    m_tableMaxRowsSpinBox->setRange(1, 50);
    m_tableMaxRowsSpinBox->setValue(4);

    m_moveModifierComboBox = new QComboBox(generalPage);
    m_moveModifierComboBox->setEditable(true);
    m_moveModifierComboBox->addItems({QStringLiteral("ctrl"), QStringLiteral("shift"), QStringLiteral("alt"),
                                      QStringLiteral("meta"), QStringLiteral("ctrl+shift"), QStringLiteral("none")});

    generalLayout->addRow(tr("Default marker diameter"), m_markerDiameterSpinBox);
    generalLayout->addRow(tr("Default marker font size"), m_markerFontSpinBox);
    generalLayout->addRow(tr("Maximum label table text rows"), m_tableMaxRowsSpinBox);
    generalLayout->addRow(tr("Move-label modifier"), m_moveModifierComboBox);
    tabWidget->addTab(generalPage, tr("General"));

    auto* groupPage = new QWidget(tabWidget);
    auto* groupLayout = new QVBoxLayout(groupPage);

    m_groupStyleTable = new QTableWidget(groupPage);
    m_groupStyleTable->setColumnCount(4);
    m_groupStyleTable->setHorizontalHeaderLabels(
        {tr("Color"), tr("Marker diameter"), tr("Font size"), tr("Marker style")});
    m_groupStyleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_groupStyleTable->verticalHeader()->setVisible(false);
    m_groupStyleTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_groupStyleTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    groupLayout->addWidget(m_groupStyleTable, 1);

    auto* groupButtonLayout = new QHBoxLayout;
    auto* addStyleButton = new QPushButton(tr("Add group style"), groupPage);
    auto* removeStyleButton = new QPushButton(tr("Remove selected styles"), groupPage);
    groupButtonLayout->addWidget(addStyleButton);
    groupButtonLayout->addWidget(removeStyleButton);
    groupButtonLayout->addStretch();
    groupLayout->addLayout(groupButtonLayout);
    tabWidget->addTab(groupPage, tr("Group styles"));

    auto* jsonPage = new QWidget(tabWidget);
    auto* jsonLayout = new QVBoxLayout(jsonPage);
    m_jsonPreview = new QPlainTextEdit(jsonPage);
    m_jsonPreview->setReadOnly(true);
    jsonLayout->addWidget(m_jsonPreview);
    tabWidget->addTab(jsonPage, tr("JSON preview"));

    rootLayout->addWidget(tabWidget, 1);

    m_messageLabel = new QLabel(this);
    m_messageLabel->setTextFormat(Qt::PlainText);
    m_messageLabel->setVisible(false);
    rootLayout->addWidget(m_messageLabel);

    auto* buttonBox = new QDialogButtonBox(this);
    auto* reloadButton = buttonBox->addButton(tr("Reload"), QDialogButtonBox::ResetRole);
    auto* openButton = buttonBox->addButton(tr("Open in Text Editor"), QDialogButtonBox::ActionRole);
    m_applyButton = buttonBox->addButton(tr("Apply"), QDialogButtonBox::ApplyRole);
    m_saveButton = buttonBox->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    auto* closeButton = buttonBox->addButton(tr("Close"), QDialogButtonBox::RejectRole);
    rootLayout->addWidget(buttonBox);

    connect(addStyleButton, &QPushButton::clicked, this, [this]() {
        addGroupStyleRow();
        updateJsonPreview();
    });
    connect(removeStyleButton, &QPushButton::clicked, this, [this]() {
        removeSelectedGroupStyleRows();
        updateJsonPreview();
    });
    connect(reloadButton, &QPushButton::clicked, this, &PreferenceDialog::loadFromDisk);
    connect(openButton, &QPushButton::clicked, this, &PreferenceDialog::openPreferenceFile);
    connect(m_applyButton, &QPushButton::clicked, this, &PreferenceDialog::applyPreferences);
    connect(m_saveButton, &QPushButton::clicked, this, &PreferenceDialog::savePreferences);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);

    connect(m_markerDiameterSpinBox, &QDoubleSpinBox::valueChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_markerFontSpinBox, &QDoubleSpinBox::valueChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_tableMaxRowsSpinBox, &QSpinBox::valueChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_moveModifierComboBox, &QComboBox::currentTextChanged, this, &PreferenceDialog::updateJsonPreview);
}

void PreferenceDialog::loadFromDisk()
{
    QFile file(m_preferencePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setMessage(tr("Could not read preference file; showing defaults."), true);
        loadDocument(QJsonDocument(QJsonObject{}));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setMessage(tr("Preference file is invalid; showing defaults."), true);
        loadDocument(QJsonDocument(QJsonObject{}));
        return;
    }

    loadDocument(document);
    setMessage(tr("Loaded preference file."));
}

void PreferenceDialog::loadDocument(const QJsonDocument& document)
{
    const QJsonObject root = document.object();
    const QJsonObject labelMarker = root.value(QStringLiteral("labelMarker")).toObject();
    const QJsonObject labelTable = root.value(QStringLiteral("labelTable")).toObject();
    const QJsonObject input = root.value(QStringLiteral("input")).toObject();

    m_markerDiameterSpinBox->setValue(labelMarker.value(QStringLiteral("diameter")).toDouble(20.0));
    m_markerFontSpinBox->setValue(labelMarker.value(QStringLiteral("fontPointSize")).toDouble(10.0));
    m_tableMaxRowsSpinBox->setValue(labelTable.value(QStringLiteral("maxTextRows")).toInt(4));
    m_moveModifierComboBox->setCurrentText(
        input.value(QStringLiteral("moveLabelModifier")).toString(QStringLiteral("ctrl")));

    m_groupStyleTable->setRowCount(0);
    const QJsonArray groupStyles = root.value(QStringLiteral("groupStyles")).toArray();
    for (const QJsonValue& value : groupStyles) {
        addGroupStyleRow(value.toObject());
    }

    updateJsonPreview();
}

QJsonDocument PreferenceDialog::documentFromUi() const
{
    QJsonObject labelMarker;
    labelMarker.insert(QStringLiteral("diameter"), m_markerDiameterSpinBox->value());
    labelMarker.insert(QStringLiteral("fontPointSize"), m_markerFontSpinBox->value());

    QJsonObject labelTable;
    labelTable.insert(QStringLiteral("maxTextRows"), m_tableMaxRowsSpinBox->value());

    QJsonObject input;
    input.insert(QStringLiteral("moveLabelModifier"), m_moveModifierComboBox->currentText().trimmed());

    QJsonArray groupStyles;
    for (int row = 0; row < m_groupStyleTable->rowCount(); ++row) {
        auto* colorButton = qobject_cast<QPushButton*>(m_groupStyleTable->cellWidget(row, colorColumn));
        auto* diameterSpinBox = qobject_cast<QDoubleSpinBox*>(m_groupStyleTable->cellWidget(row, diameterColumn));
        auto* fontSpinBox = qobject_cast<QDoubleSpinBox*>(m_groupStyleTable->cellWidget(row, fontColumn));
        auto* shapeComboBox = qobject_cast<QComboBox*>(m_groupStyleTable->cellWidget(row, shapeColumn));
        if (colorButton == nullptr || diameterSpinBox == nullptr || fontSpinBox == nullptr ||
            shapeComboBox == nullptr) {
            continue;
        }

        QJsonObject style;
        style.insert(QStringLiteral("groupColor"), colorButton->property("color").toString());
        style.insert(QStringLiteral("markerDiameter"), diameterSpinBox->value());
        style.insert(QStringLiteral("fontPointSize"), fontSpinBox->value());
        style.insert(QStringLiteral("markerStyle"), shapeComboBox->currentData().toString());
        groupStyles.append(style);
    }

    QJsonObject root;
    root.insert(QStringLiteral("labelMarker"), labelMarker);
    root.insert(QStringLiteral("labelTable"), labelTable);
    root.insert(QStringLiteral("input"), input);
    root.insert(QStringLiteral("groupStyles"), groupStyles);
    return QJsonDocument(root);
}

void PreferenceDialog::updateJsonPreview()
{
    m_jsonPreview->setPlainText(QString::fromUtf8(documentFromUi().toJson(QJsonDocument::Indented)));
}

void PreferenceDialog::setMessage(const QString& message, bool warning)
{
    m_messageLabel->setStyleSheet(warning ? QStringLiteral("QLabel { color: #b26a00; font-weight: 600; }")
                                          : QStringLiteral("QLabel { color: palette(text); }"));
    m_messageLabel->setText(message);
    m_messageLabel->setVisible(!message.isEmpty());
}

void PreferenceDialog::addGroupStyleRow(const QJsonObject& style)
{
    const int row = m_groupStyleTable->rowCount();
    m_groupStyleTable->insertRow(row);
    m_groupStyleTable->setVerticalHeaderItem(row, new QTableWidgetItem(QString::number(row + 1)));

    const QColor color = colorFromStyleObject(style);
    auto* colorButton = new QPushButton(color.name(), m_groupStyleTable);
    colorButton->setProperty("color", color.name());
    colorButton->setStyleSheet(QStringLiteral("QPushButton { color: %1; }").arg(color.name()));
    m_groupStyleTable->setCellWidget(row, colorColumn, colorButton);

    auto* diameterSpinBox = makePositiveDoubleSpinBox(
        style.value(QStringLiteral("markerDiameter")).toDouble(m_markerDiameterSpinBox->value()), 1.0, 256.0);
    m_groupStyleTable->setCellWidget(row, diameterColumn, diameterSpinBox);

    auto* fontSpinBox = makePositiveDoubleSpinBox(
        style.value(QStringLiteral("fontPointSize")).toDouble(m_markerFontSpinBox->value()), 0.1, 256.0);
    m_groupStyleTable->setCellWidget(row, fontColumn, fontSpinBox);

    auto* shapeComboBox = new QComboBox(m_groupStyleTable);
    shapeComboBox->addItem(tr("Circle"), QStringLiteral("circle"));
    shapeComboBox->addItem(tr("Square"), QStringLiteral("square"));
    shapeComboBox->setCurrentIndex(markerStyleFromObject(style) == QStringLiteral("square") ? 1 : 0);
    m_groupStyleTable->setCellWidget(row, shapeColumn, shapeComboBox);

    connect(colorButton, &QPushButton::clicked, this, [this, row]() { chooseGroupColor(row); });
    connect(diameterSpinBox, &QDoubleSpinBox::valueChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(fontSpinBox, &QDoubleSpinBox::valueChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(shapeComboBox, &QComboBox::currentIndexChanged, this, &PreferenceDialog::updateJsonPreview);
}

void PreferenceDialog::addGroupStyleRow()
{
    addGroupStyleRow(QJsonObject{});
}

void PreferenceDialog::removeSelectedGroupStyleRows()
{
    QList<int> rows;
    const QModelIndexList selectedRows = m_groupStyleTable->selectionModel()->selectedRows();
    rows.reserve(selectedRows.size());
    for (const QModelIndex& index : selectedRows) {
        rows.append(index.row());
    }
    std::sort(rows.begin(), rows.end(), std::greater<>());
    for (int row : rows) {
        m_groupStyleTable->removeRow(row);
    }
}

void PreferenceDialog::chooseGroupColor(int row)
{
    auto* colorButton = qobject_cast<QPushButton*>(m_groupStyleTable->cellWidget(row, colorColumn));
    if (colorButton == nullptr) {
        return;
    }

    const QColor currentColor(colorButton->property("color").toString());
    const QColor color = QColorDialog::getColor(currentColor, this, tr("Choose group color"));
    if (!color.isValid()) {
        return;
    }

    colorButton->setProperty("color", color.name());
    colorButton->setText(color.name());
    colorButton->setStyleSheet(QStringLiteral("QPushButton { color: %1; }").arg(color.name()));
    updateJsonPreview();
}

void PreferenceDialog::applyPreferences()
{
    const QByteArray json = documentFromUi().toJson(QJsonDocument::Indented);
    labelminus::core::AppPreferencesLoadResult result = labelminus::core::AppPreferences::loadFromJson(json);
    emit preferencesApplied(result);
    setMessage(result.warnings.isEmpty() ? tr("Preferences applied.")
                                         : tr("Preferences applied with warnings; see the main window status bar."),
               !result.warnings.isEmpty());
}

void PreferenceDialog::savePreferences()
{
    QFile file(m_preferencePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setMessage(tr("Could not save preference file."), true);
        return;
    }

    file.write(documentFromUi().toJson(QJsonDocument::Indented));
    file.write("\n");
    file.close();
    applyPreferences();
    setMessage(tr("Preferences saved and applied."));
}

void PreferenceDialog::openPreferenceFile()
{
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(m_preferencePath))) {
        setMessage(tr("Could not open preference file in the system text editor."), true);
    }
}
