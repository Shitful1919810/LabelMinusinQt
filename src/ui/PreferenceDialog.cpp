#include "ui/PreferenceDialog.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFontDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <optional>

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

QWidget* makeFontSelectorWidget(QWidget* parent, QLabel*& label, QPushButton*& chooseButton, QPushButton*& resetButton)
{
    auto* widget = new QWidget(parent);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    label = new QLabel(widget);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    chooseButton = new QPushButton(PreferenceDialog::tr("Choose Font..."), widget);
    resetButton = new QPushButton(PreferenceDialog::tr("Use Default"), widget);
    layout->addWidget(label, 1);
    layout->addWidget(chooseButton);
    layout->addWidget(resetButton);
    return widget;
}

QString comboBoxDataOrText(const QComboBox* comboBox)
{
    if (comboBox == nullptr) {
        return {};
    }

    if (comboBox->currentIndex() >= 0 && comboBox->currentText() == comboBox->itemText(comboBox->currentIndex())) {
        return comboBox->currentData().toString();
    }

    return comboBox->currentText().trimmed();
}

std::optional<QFont> chooseFontWithQtDialog(QWidget* parent, const QFont& initialFont, const QString& title)
{
    QFontDialog dialog(initialFont, parent);
    dialog.setWindowTitle(title);
#ifdef Q_OS_WIN
    dialog.setOption(QFontDialog::DontUseNativeDialog, true);
#endif
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    QFont font = dialog.selectedFont();
    if (font.pointSizeF() <= 0.0) {
        font.setPointSizeF(parent != nullptr ? parent->font().pointSizeF() : initialFont.pointSizeF());
    }
    return font;
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
    m_applicationStyleComboBox = new QComboBox(generalPage);
    m_applicationStyleComboBox->setEditable(true);
    m_applicationStyleComboBox->addItem(tr("Use system default"), QString());
    const QStringList availableStyles = QStyleFactory::keys();
    for (const QString& styleName : availableStyles) {
        m_applicationStyleComboBox->addItem(styleName, styleName);
    }
    auto* labelTableFontWidget = makeFontSelectorWidget(generalPage, m_labelTableFontLabel,
                                                        m_chooseLabelTableFontButton, m_resetLabelTableFontButton);
    auto* textEditorFontWidget = makeFontSelectorWidget(generalPage, m_textEditorFontLabel,
                                                        m_chooseTextEditorFontButton, m_resetTextEditorFontButton);
    m_backupPathEdit = new QLineEdit(generalPage);
    m_backupPathEdit->setText(QStringLiteral("bak"));
    m_backupIntervalSpinBox = new QSpinBox(generalPage);
    m_backupIntervalSpinBox->setRange(1, 86400);
    m_backupIntervalSpinBox->setValue(60);

    generalLayout->addRow(tr("Default marker diameter"), m_markerDiameterSpinBox);
    generalLayout->addRow(tr("Default marker font size"), m_markerFontSpinBox);
    generalLayout->addRow(tr("Application style"), m_applicationStyleComboBox);
    generalLayout->addRow(tr("Maximum label table text rows"), m_tableMaxRowsSpinBox);
    generalLayout->addRow(tr("Label table font"), labelTableFontWidget);
    generalLayout->addRow(tr("Text editor font"), textEditorFontWidget);
    generalLayout->addRow(tr("Backup path"), m_backupPathEdit);
    generalLayout->addRow(tr("Backup interval seconds"), m_backupIntervalSpinBox);
    tabWidget->addTab(generalPage, tr("General"));

    auto* keyMappingPage = new QWidget(tabWidget);
    auto* keyMappingLayout = new QFormLayout(keyMappingPage);
    m_moveModifierComboBox = new QComboBox(keyMappingPage);
    m_moveModifierComboBox->setEditable(true);
    m_moveModifierComboBox->addItems({QStringLiteral("ctrl"), QStringLiteral("shift"), QStringLiteral("alt"),
                                      QStringLiteral("meta"), QStringLiteral("ctrl+shift"), QStringLiteral("none")});
    m_undoShortcutEdit = new QKeySequenceEdit(QKeySequence(QStringLiteral("Ctrl+Z")), keyMappingPage);
    m_redoShortcutEdit = new QKeySequenceEdit(QKeySequence(QStringLiteral("Ctrl+Y")), keyMappingPage);
    keyMappingLayout->addRow(tr("Move-label modifier"), m_moveModifierComboBox);
    keyMappingLayout->addRow(tr("Undo shortcut"), m_undoShortcutEdit);
    keyMappingLayout->addRow(tr("Redo shortcut"), m_redoShortcutEdit);
    tabWidget->addTab(keyMappingPage, tr("Key mappings"));

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
    connect(m_applicationStyleComboBox, &QComboBox::currentTextChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_tableMaxRowsSpinBox, &QSpinBox::valueChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_chooseLabelTableFontButton, &QPushButton::clicked, this, &PreferenceDialog::chooseLabelTableFont);
    connect(m_resetLabelTableFontButton, &QPushButton::clicked, this, &PreferenceDialog::resetLabelTableFont);
    connect(m_chooseTextEditorFontButton, &QPushButton::clicked, this, &PreferenceDialog::chooseTextEditorFont);
    connect(m_resetTextEditorFontButton, &QPushButton::clicked, this, &PreferenceDialog::resetTextEditorFont);
    connect(m_moveModifierComboBox, &QComboBox::currentTextChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_undoShortcutEdit, &QKeySequenceEdit::keySequenceChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_redoShortcutEdit, &QKeySequenceEdit::keySequenceChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_backupPathEdit, &QLineEdit::textChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_backupIntervalSpinBox, &QSpinBox::valueChanged, this, &PreferenceDialog::updateJsonPreview);
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
    const QJsonObject appearance = root.value(QStringLiteral("appearance")).toObject();
    const QJsonObject labelTable = root.value(QStringLiteral("labelTable")).toObject();
    const QJsonObject labelTextEditor = root.value(QStringLiteral("labelTextEditor")).toObject();
    const QJsonObject input = root.value(QStringLiteral("input")).toObject();

    m_markerDiameterSpinBox->setValue(labelMarker.value(QStringLiteral("diameter")).toDouble(20.0));
    m_markerFontSpinBox->setValue(labelMarker.value(QStringLiteral("fontPointSize")).toDouble(10.0));
    const QString applicationStyle = appearance.value(QStringLiteral("style")).toString().trimmed();
    if (applicationStyle.isEmpty()) {
        m_applicationStyleComboBox->setCurrentIndex(0);
    }
    else {
        m_applicationStyleComboBox->setCurrentText(applicationStyle);
    }
    m_tableMaxRowsSpinBox->setValue(labelTable.value(QStringLiteral("maxTextRows")).toInt(4));
    const QString labelTableFontFamily = labelTable.value(QStringLiteral("fontFamily")).toString().trimmed();
    const double labelTableFontPointSize = labelTable.value(QStringLiteral("fontPointSize")).toDouble(0.0);
    m_usesDefaultLabelTableFont = labelTableFontFamily.isEmpty() && labelTableFontPointSize <= 0.0;
    m_labelTableFont = font();
    if (!labelTableFontFamily.isEmpty()) {
        m_labelTableFont.setFamily(labelTableFontFamily);
    }
    if (labelTableFontPointSize > 0.0) {
        m_labelTableFont.setPointSizeF(labelTableFontPointSize);
    }
    updateLabelTableFontSummary();

    const QString textEditorFontFamily = labelTextEditor.value(QStringLiteral("fontFamily")).toString().trimmed();
    const double textEditorFontPointSize = labelTextEditor.value(QStringLiteral("fontPointSize")).toDouble(0.0);
    m_usesDefaultTextEditorFont = textEditorFontFamily.isEmpty() && textEditorFontPointSize <= 0.0;
    m_textEditorFont = font();
    if (!textEditorFontFamily.isEmpty()) {
        m_textEditorFont.setFamily(textEditorFontFamily);
    }
    if (textEditorFontPointSize > 0.0) {
        m_textEditorFont.setPointSizeF(textEditorFontPointSize);
    }
    updateTextEditorFontSummary();
    m_moveModifierComboBox->setCurrentText(
        input.value(QStringLiteral("moveLabelModifier")).toString(QStringLiteral("ctrl")));
    const QKeySequence undoShortcut = QKeySequence::fromString(
        input.value(QStringLiteral("undoShortcut")).toString(QStringLiteral("Ctrl+Z")), QKeySequence::PortableText);
    const QKeySequence redoShortcut = QKeySequence::fromString(
        input.value(QStringLiteral("redoShortcut")).toString(QStringLiteral("Ctrl+Y")), QKeySequence::PortableText);
    m_undoShortcutEdit->setKeySequence(undoShortcut.isEmpty() ? QKeySequence(QStringLiteral("Ctrl+Z")) : undoShortcut);
    m_redoShortcutEdit->setKeySequence(redoShortcut.isEmpty() ? QKeySequence(QStringLiteral("Ctrl+Y")) : redoShortcut);
    m_backupPathEdit->setText(root.value(QStringLiteral("backupPath")).toString(QStringLiteral("bak")));
    m_backupIntervalSpinBox->setValue(root.value(QStringLiteral("backupIntervalSeconds")).toInt(60));

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

    QJsonObject appearance;
    appearance.insert(QStringLiteral("style"), comboBoxDataOrText(m_applicationStyleComboBox));

    QJsonObject labelTable;
    labelTable.insert(QStringLiteral("maxTextRows"), m_tableMaxRowsSpinBox->value());
    labelTable.insert(QStringLiteral("fontFamily"),
                      m_usesDefaultLabelTableFont ? QString() : m_labelTableFont.family());
    labelTable.insert(QStringLiteral("fontPointSize"),
                      m_usesDefaultLabelTableFont ? 0.0 : m_labelTableFont.pointSizeF());

    QJsonObject labelTextEditor;
    labelTextEditor.insert(QStringLiteral("fontFamily"),
                           m_usesDefaultTextEditorFont ? QString() : m_textEditorFont.family());
    labelTextEditor.insert(QStringLiteral("fontPointSize"),
                           m_usesDefaultTextEditorFont ? 0.0 : m_textEditorFont.pointSizeF());

    QJsonObject input;
    input.insert(QStringLiteral("moveLabelModifier"), m_moveModifierComboBox->currentText().trimmed());
    input.insert(QStringLiteral("undoShortcut"),
                 m_undoShortcutEdit->keySequence().toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("redoShortcut"),
                 m_redoShortcutEdit->keySequence().toString(QKeySequence::PortableText));

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
    root.insert(QStringLiteral("appearance"), appearance);
    root.insert(QStringLiteral("labelMarker"), labelMarker);
    root.insert(QStringLiteral("labelTable"), labelTable);
    root.insert(QStringLiteral("labelTextEditor"), labelTextEditor);
    root.insert(QStringLiteral("input"), input);
    root.insert(QStringLiteral("backupPath"), m_backupPathEdit->text().trimmed());
    root.insert(QStringLiteral("backupIntervalSeconds"), m_backupIntervalSpinBox->value());
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

void PreferenceDialog::chooseLabelTableFont()
{
    const std::optional<QFont> font = chooseFontWithQtDialog(
        this, m_usesDefaultLabelTableFont ? this->font() : m_labelTableFont, tr("Choose label table font"));
    if (!font.has_value()) {
        return;
    }

    m_labelTableFont = font.value();
    m_usesDefaultLabelTableFont = false;
    updateLabelTableFontSummary();
    updateJsonPreview();
}

void PreferenceDialog::resetLabelTableFont()
{
    m_labelTableFont = font();
    m_usesDefaultLabelTableFont = true;
    updateLabelTableFontSummary();
    updateJsonPreview();
}

void PreferenceDialog::updateLabelTableFontSummary()
{
    if (m_labelTableFontLabel == nullptr) {
        return;
    }

    if (m_usesDefaultLabelTableFont) {
        m_labelTableFontLabel->setText(tr("Default font and size"));
        m_labelTableFontLabel->setFont(font());
        return;
    }

    m_labelTableFontLabel->setText(
        tr("%1, %2 pt").arg(m_labelTableFont.family()).arg(m_labelTableFont.pointSizeF(), 0, 'f', 1));
    m_labelTableFontLabel->setFont(font());
}

void PreferenceDialog::chooseTextEditorFont()
{
    const std::optional<QFont> font = chooseFontWithQtDialog(
        this, m_usesDefaultTextEditorFont ? this->font() : m_textEditorFont, tr("Choose text editor font"));
    if (!font.has_value()) {
        return;
    }

    m_textEditorFont = font.value();
    m_usesDefaultTextEditorFont = false;
    updateTextEditorFontSummary();
    updateJsonPreview();
}

void PreferenceDialog::resetTextEditorFont()
{
    m_textEditorFont = font();
    m_usesDefaultTextEditorFont = true;
    updateTextEditorFontSummary();
    updateJsonPreview();
}

void PreferenceDialog::updateTextEditorFontSummary()
{
    if (m_textEditorFontLabel == nullptr) {
        return;
    }

    if (m_usesDefaultTextEditorFont) {
        m_textEditorFontLabel->setText(tr("Default font and size"));
        m_textEditorFontLabel->setFont(font());
        return;
    }

    m_textEditorFontLabel->setText(
        tr("%1, %2 pt").arg(m_textEditorFont.family()).arg(m_textEditorFont.pointSizeF(), 0, 'f', 1));
    m_textEditorFontLabel->setFont(font());
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
