#include "ui/PreferenceDialog.h"

#include "ui/ThemeManager.h"

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
#include <QScrollBar>
#include <QSpinBox>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
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

QWidget* makePercentScrollBarWidget(QWidget* parent, QScrollBar*& scrollBar, QLabel*& label, int value)
{
    auto* widget = new QWidget(parent);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    scrollBar = new QScrollBar(Qt::Horizontal, widget);
    scrollBar->setRange(0, 100);
    scrollBar->setSingleStep(5);
    scrollBar->setPageStep(10);
    scrollBar->setValue(value);
    label = new QLabel(widget);
    label->setMinimumWidth(label->fontMetrics().horizontalAdvance(QStringLiteral("100%")));
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(scrollBar, 1);
    layout->addWidget(label);
    return widget;
}

QComboBox* makeModifierComboBox(QWidget* parent)
{
    auto* comboBox = new QComboBox(parent);
    comboBox->setEditable(true);
    comboBox->addItems({QStringLiteral("ctrl"), QStringLiteral("shift"), QStringLiteral("alt"), QStringLiteral("meta"),
                        QStringLiteral("ctrl+shift"), QStringLiteral("none")});
    return comboBox;
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

QString themeDisplayName(const QString& themeName)
{
    if (themeName == QStringLiteral("breezeDark")) {
        return PreferenceDialog::tr("Breeze Dark");
    }
    if (themeName == QStringLiteral("breezeLight")) {
        return PreferenceDialog::tr("Breeze Light");
    }
    return themeName;
}

std::optional<QFont> chooseFontWithQtDialog(QWidget* parent, const QFont& initialFont, const QString& title)
{
    QFontDialog dialog(initialFont, parent);
    dialog.setWindowTitle(title);
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    QFont font = dialog.selectedFont();
    if (font.pointSizeF() <= 0.0) {
        font.setPointSizeF(parent != nullptr ? parent->font().pointSizeF() : initialFont.pointSizeF());
    }
    return font;
}

const labelminus::core::AppPreferences& defaultPreferences()
{
    static const labelminus::core::AppPreferences preferences;
    return preferences;
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
    tabWidget->addTab(createGeneralPage(tabWidget), tr("General"));
    tabWidget->addTab(createKeyMappingPage(tabWidget), tr("Key mappings"));
    tabWidget->addTab(createGroupStylesPage(tabWidget), tr("Group styles"));
    tabWidget->addTab(createJsonPage(tabWidget), tr("JSON preview"));

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

    connect(reloadButton, &QPushButton::clicked, this, &PreferenceDialog::loadFromDisk);
    connect(openButton, &QPushButton::clicked, this, &PreferenceDialog::openPreferenceFile);
    connect(m_applyButton, &QPushButton::clicked, this, &PreferenceDialog::applyPreferences);
    connect(m_saveButton, &QPushButton::clicked, this, &PreferenceDialog::savePreferences);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);

    connectPreferenceChangeSignals();
}

QWidget* PreferenceDialog::createGeneralPage(QTabWidget* tabWidget)
{
    auto* generalPage = new QWidget(tabWidget);
    auto* generalLayout = new QFormLayout(generalPage);

    m_markerDiameterSpinBox = makePositiveDoubleSpinBox(defaultPreferences().labelMarkerDiameterPixels(), 1.0, 256.0);
    m_markerFontSpinBox = makePositiveDoubleSpinBox(defaultPreferences().labelMarkerFontPointSize(), 0.1, 256.0);
    m_tableMaxRowsSpinBox = new QSpinBox(generalPage);
    m_tableMaxRowsSpinBox->setRange(1, 50);
    m_tableMaxRowsSpinBox->setValue(defaultPreferences().labelTableMaxTextRows());
    m_applicationStyleComboBox = new QComboBox(generalPage);
    m_applicationStyleComboBox->setEditable(true);
    m_applicationStyleComboBox->addItem(tr("Use system default"), QString());
    const QStringList availableStyles = QStyleFactory::keys();
    for (const QString& styleName : availableStyles) {
        m_applicationStyleComboBox->addItem(styleName, styleName);
    }
    m_applicationThemeComboBox = new QComboBox(generalPage);
    m_applicationThemeComboBox->addItem(tr("Use no application theme"), QString());
    for (const QString& themeName : labelminus::ui::availableApplicationThemes()) {
        m_applicationThemeComboBox->addItem(themeDisplayName(themeName), themeName);
    }

    auto* labelTableFontWidget = makeFontSelectorWidget(generalPage, m_labelTableFontLabel,
                                                        m_chooseLabelTableFontButton, m_resetLabelTableFontButton);
    auto* textEditorFontWidget = makeFontSelectorWidget(generalPage, m_textEditorFontLabel,
                                                        m_chooseTextEditorFontButton, m_resetTextEditorFontButton);
    auto* markerTextBubbleFontWidget =
        makeFontSelectorWidget(generalPage, m_markerTextBubbleFontLabel, m_chooseMarkerTextBubbleFontButton,
                               m_resetMarkerTextBubbleFontButton);
    auto* markerTextBubbleOpacityWidget = makePercentScrollBarWidget(
        generalPage, m_markerTextBubbleOpacityScrollBar, m_markerTextBubbleOpacityLabel,
        static_cast<int>(std::round(defaultPreferences().markerTextBubbleOpacity() * 100.0)));
    m_backupPathEdit = new QLineEdit(generalPage);
    m_backupPathEdit->setText(defaultPreferences().backupPath());
    m_backupIntervalSpinBox = new QSpinBox(generalPage);
    m_backupIntervalSpinBox->setRange(1, 86400);
    m_backupIntervalSpinBox->setValue(defaultPreferences().backupIntervalSeconds());

    generalLayout->addRow(tr("Default marker diameter"), m_markerDiameterSpinBox);
    generalLayout->addRow(tr("Default marker font size"), m_markerFontSpinBox);
    generalLayout->addRow(tr("Qt widget style"), m_applicationStyleComboBox);
    generalLayout->addRow(tr("Breeze stylesheet theme"), m_applicationThemeComboBox);
    generalLayout->addRow(tr("Maximum label table text rows"), m_tableMaxRowsSpinBox);
    generalLayout->addRow(tr("Label table font"), labelTableFontWidget);
    generalLayout->addRow(tr("Text editor font"), textEditorFontWidget);
    generalLayout->addRow(tr("Marker text bubble font"), markerTextBubbleFontWidget);
    generalLayout->addRow(tr("Marker text bubble opacity"), markerTextBubbleOpacityWidget);
    generalLayout->addRow(tr("Backup path"), m_backupPathEdit);
    generalLayout->addRow(tr("Backup interval seconds"), m_backupIntervalSpinBox);
    return generalPage;
}

QWidget* PreferenceDialog::createKeyMappingPage(QTabWidget* tabWidget)
{
    auto* keyMappingPage = new QWidget(tabWidget);
    auto* keyMappingLayout = new QHBoxLayout(keyMappingPage);
    auto* leftColumn = new QWidget(keyMappingPage);
    auto* leftLayout = new QFormLayout(leftColumn);
    auto* rightColumn = new QWidget(keyMappingPage);
    auto* rightLayout = new QFormLayout(rightColumn);
    keyMappingLayout->addWidget(leftColumn, 1);
    keyMappingLayout->addWidget(rightColumn, 1);

    m_moveModifierComboBox = makeModifierComboBox(keyMappingPage);
    m_previousLabelModifierComboBox = makeModifierComboBox(keyMappingPage);
    m_undoShortcutEdit = new QKeySequenceEdit(defaultPreferences().undoShortcut(), keyMappingPage);
    m_redoShortcutEdit = new QKeySequenceEdit(defaultPreferences().redoShortcut(), keyMappingPage);
    m_nextLabelShortcutEdit = new QKeySequenceEdit(defaultPreferences().nextLabelShortcut(), keyMappingPage);
    m_alternatePreviousLabelShortcutEdit =
        new QKeySequenceEdit(defaultPreferences().alternatePreviousLabelShortcut(), keyMappingPage);
    m_alternateNextLabelShortcutEdit =
        new QKeySequenceEdit(defaultPreferences().alternateNextLabelShortcut(), keyMappingPage);
    m_previousPageShortcutEdit = new QKeySequenceEdit(defaultPreferences().previousPageShortcut(), keyMappingPage);
    m_nextPageShortcutEdit = new QKeySequenceEdit(defaultPreferences().nextPageShortcut(), keyMappingPage);
    m_editLabelTextShortcutEdit = new QKeySequenceEdit(defaultPreferences().editLabelTextShortcut(), keyMappingPage);
    m_commitLabelTextShortcutEdit =
        new QKeySequenceEdit(defaultPreferences().commitLabelTextShortcut(), keyMappingPage);

    leftLayout->addRow(tr("Move-label modifier"), m_moveModifierComboBox);
    leftLayout->addRow(tr("Previous label modifier"), m_previousLabelModifierComboBox);
    leftLayout->addRow(tr("Undo"), m_undoShortcutEdit);
    leftLayout->addRow(tr("Redo"), m_redoShortcutEdit);
    leftLayout->addRow(tr("Edit label text"), m_editLabelTextShortcutEdit);
    leftLayout->addRow(tr("Commit label text"), m_commitLabelTextShortcutEdit);

    rightLayout->addRow(tr("Switch label"), m_nextLabelShortcutEdit);
    rightLayout->addRow(tr("Previous label"), m_alternatePreviousLabelShortcutEdit);
    rightLayout->addRow(tr("Next label"), m_alternateNextLabelShortcutEdit);
    rightLayout->addRow(tr("Previous page"), m_previousPageShortcutEdit);
    rightLayout->addRow(tr("Next page"), m_nextPageShortcutEdit);
    return keyMappingPage;
}

QWidget* PreferenceDialog::createGroupStylesPage(QTabWidget* tabWidget)
{
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

    connect(addStyleButton, &QPushButton::clicked, this, [this]() {
        addGroupStyleRow();
        updateJsonPreview();
    });
    connect(removeStyleButton, &QPushButton::clicked, this, [this]() {
        removeSelectedGroupStyleRows();
        updateJsonPreview();
    });
    return groupPage;
}

QWidget* PreferenceDialog::createJsonPage(QTabWidget* tabWidget)
{
    auto* jsonPage = new QWidget(tabWidget);
    auto* jsonLayout = new QVBoxLayout(jsonPage);
    m_jsonPreview = new QPlainTextEdit(jsonPage);
    m_jsonPreview->setReadOnly(true);
    jsonLayout->addWidget(m_jsonPreview);
    return jsonPage;
}

void PreferenceDialog::connectPreferenceChangeSignals()
{
    connect(m_markerDiameterSpinBox, &QDoubleSpinBox::valueChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_markerFontSpinBox, &QDoubleSpinBox::valueChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_applicationStyleComboBox, &QComboBox::currentTextChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_applicationThemeComboBox, &QComboBox::currentIndexChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_tableMaxRowsSpinBox, &QSpinBox::valueChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_chooseLabelTableFontButton, &QPushButton::clicked, this, &PreferenceDialog::chooseLabelTableFont);
    connect(m_resetLabelTableFontButton, &QPushButton::clicked, this, &PreferenceDialog::resetLabelTableFont);
    connect(m_chooseTextEditorFontButton, &QPushButton::clicked, this, &PreferenceDialog::chooseTextEditorFont);
    connect(m_resetTextEditorFontButton, &QPushButton::clicked, this, &PreferenceDialog::resetTextEditorFont);
    connect(m_chooseMarkerTextBubbleFontButton, &QPushButton::clicked, this,
            &PreferenceDialog::chooseMarkerTextBubbleFont);
    connect(m_resetMarkerTextBubbleFontButton, &QPushButton::clicked, this,
            &PreferenceDialog::resetMarkerTextBubbleFont);
    connect(m_markerTextBubbleOpacityScrollBar, &QScrollBar::valueChanged, this, [this](int value) {
        m_markerTextBubbleOpacityLabel->setText(tr("%1%").arg(value));
        updateJsonPreview();
    });
    connect(m_moveModifierComboBox, &QComboBox::currentTextChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_previousLabelModifierComboBox, &QComboBox::currentTextChanged, this,
            &PreferenceDialog::updateJsonPreview);
    connect(m_undoShortcutEdit, &QKeySequenceEdit::keySequenceChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_redoShortcutEdit, &QKeySequenceEdit::keySequenceChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_nextLabelShortcutEdit, &QKeySequenceEdit::keySequenceChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_alternatePreviousLabelShortcutEdit, &QKeySequenceEdit::keySequenceChanged, this,
            &PreferenceDialog::updateJsonPreview);
    connect(m_alternateNextLabelShortcutEdit, &QKeySequenceEdit::keySequenceChanged, this,
            &PreferenceDialog::updateJsonPreview);
    connect(m_previousPageShortcutEdit, &QKeySequenceEdit::keySequenceChanged, this,
            &PreferenceDialog::updateJsonPreview);
    connect(m_nextPageShortcutEdit, &QKeySequenceEdit::keySequenceChanged, this, &PreferenceDialog::updateJsonPreview);
    connect(m_editLabelTextShortcutEdit, &QKeySequenceEdit::keySequenceChanged, this,
            &PreferenceDialog::updateJsonPreview);
    connect(m_commitLabelTextShortcutEdit, &QKeySequenceEdit::keySequenceChanged, this,
            &PreferenceDialog::updateJsonPreview);
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
    const QJsonObject markerTextBubble = root.value(QStringLiteral("markerTextBubble")).toObject();
    const QJsonObject input = root.value(QStringLiteral("input")).toObject();

    m_markerDiameterSpinBox->setValue(
        labelMarker.value(QStringLiteral("diameter")).toDouble(defaultPreferences().labelMarkerDiameterPixels()));
    m_markerFontSpinBox->setValue(
        labelMarker.value(QStringLiteral("fontPointSize")).toDouble(defaultPreferences().labelMarkerFontPointSize()));
    const QString applicationStyle = appearance.value(QStringLiteral("style")).toString().trimmed();
    if (applicationStyle.isEmpty()) {
        m_applicationStyleComboBox->setCurrentIndex(0);
    }
    else {
        m_applicationStyleComboBox->setCurrentText(applicationStyle);
    }
    const QString applicationTheme = appearance.value(QStringLiteral("theme")).toString().trimmed();
    const int themeIndex = m_applicationThemeComboBox->findData(applicationTheme);
    m_applicationThemeComboBox->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);
    m_tableMaxRowsSpinBox->setValue(
        labelTable.value(QStringLiteral("maxTextRows")).toInt(defaultPreferences().labelTableMaxTextRows()));
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

    const QString markerTextBubbleFontFamily =
        markerTextBubble.value(QStringLiteral("fontFamily")).toString().trimmed();
    const double markerTextBubbleFontPointSize = markerTextBubble.value(QStringLiteral("fontPointSize")).toDouble(0.0);
    m_usesDefaultMarkerTextBubbleFont = markerTextBubbleFontFamily.isEmpty() && markerTextBubbleFontPointSize <= 0.0;
    m_markerTextBubbleFont = font();
    if (!markerTextBubbleFontFamily.isEmpty()) {
        m_markerTextBubbleFont.setFamily(markerTextBubbleFontFamily);
    }
    if (markerTextBubbleFontPointSize > 0.0) {
        m_markerTextBubbleFont.setPointSizeF(markerTextBubbleFontPointSize);
    }
    updateMarkerTextBubbleFontSummary();
    const int markerTextBubbleOpacity = std::clamp(
        static_cast<int>(std::round(
            markerTextBubble.value(QStringLiteral("opacity")).toDouble(defaultPreferences().markerTextBubbleOpacity()) *
            100.0)),
        0, 100);
    m_markerTextBubbleOpacityScrollBar->setValue(markerTextBubbleOpacity);
    m_markerTextBubbleOpacityLabel->setText(tr("%1%").arg(markerTextBubbleOpacity));

    m_moveModifierComboBox->setCurrentText(
        input.value(QStringLiteral("moveLabelModifier")).toString(QStringLiteral("ctrl")));
    m_previousLabelModifierComboBox->setCurrentText(
        input.value(QStringLiteral("previousLabelModifier")).toString(QStringLiteral("ctrl")));
    const QKeySequence undoShortcut = QKeySequence::fromString(
        input.value(QStringLiteral("undoShortcut"))
            .toString(defaultPreferences().undoShortcut().toString(QKeySequence::PortableText)),
        QKeySequence::PortableText);
    const QKeySequence redoShortcut = QKeySequence::fromString(
        input.value(QStringLiteral("redoShortcut"))
            .toString(defaultPreferences().redoShortcut().toString(QKeySequence::PortableText)),
        QKeySequence::PortableText);
    const QKeySequence nextLabelShortcut = QKeySequence::fromString(
        input.value(QStringLiteral("nextLabelShortcut"))
            .toString(defaultPreferences().nextLabelShortcut().toString(QKeySequence::PortableText)),
        QKeySequence::PortableText);
    const QKeySequence alternatePreviousLabelShortcut = QKeySequence::fromString(
        input.value(QStringLiteral("alternatePreviousLabelShortcut"))
            .toString(defaultPreferences().alternatePreviousLabelShortcut().toString(QKeySequence::PortableText)),
        QKeySequence::PortableText);
    const QKeySequence alternateNextLabelShortcut = QKeySequence::fromString(
        input.value(QStringLiteral("alternateNextLabelShortcut"))
            .toString(defaultPreferences().alternateNextLabelShortcut().toString(QKeySequence::PortableText)),
        QKeySequence::PortableText);
    const QKeySequence previousPageShortcut = QKeySequence::fromString(
        input.value(QStringLiteral("previousPageShortcut"))
            .toString(defaultPreferences().previousPageShortcut().toString(QKeySequence::PortableText)),
        QKeySequence::PortableText);
    const QKeySequence nextPageShortcut = QKeySequence::fromString(
        input.value(QStringLiteral("nextPageShortcut"))
            .toString(defaultPreferences().nextPageShortcut().toString(QKeySequence::PortableText)),
        QKeySequence::PortableText);
    const QKeySequence editLabelTextShortcut = QKeySequence::fromString(
        input.value(QStringLiteral("editLabelTextShortcut"))
            .toString(defaultPreferences().editLabelTextShortcut().toString(QKeySequence::PortableText)),
        QKeySequence::PortableText);
    const QKeySequence commitLabelTextShortcut = QKeySequence::fromString(
        input.value(QStringLiteral("commitLabelTextShortcut"))
            .toString(defaultPreferences().commitLabelTextShortcut().toString(QKeySequence::PortableText)),
        QKeySequence::PortableText);
    m_undoShortcutEdit->setKeySequence(undoShortcut.isEmpty() ? defaultPreferences().undoShortcut() : undoShortcut);
    m_redoShortcutEdit->setKeySequence(redoShortcut.isEmpty() ? defaultPreferences().redoShortcut() : redoShortcut);
    m_nextLabelShortcutEdit->setKeySequence(nextLabelShortcut.isEmpty() ? defaultPreferences().nextLabelShortcut()
                                                                        : nextLabelShortcut);
    m_alternatePreviousLabelShortcutEdit->setKeySequence(alternatePreviousLabelShortcut.isEmpty()
                                                             ? defaultPreferences().alternatePreviousLabelShortcut()
                                                             : alternatePreviousLabelShortcut);
    m_alternateNextLabelShortcutEdit->setKeySequence(alternateNextLabelShortcut.isEmpty()
                                                         ? defaultPreferences().alternateNextLabelShortcut()
                                                         : alternateNextLabelShortcut);
    m_previousPageShortcutEdit->setKeySequence(
        previousPageShortcut.isEmpty() ? defaultPreferences().previousPageShortcut() : previousPageShortcut);
    m_nextPageShortcutEdit->setKeySequence(nextPageShortcut.isEmpty() ? defaultPreferences().nextPageShortcut()
                                                                      : nextPageShortcut);
    m_editLabelTextShortcutEdit->setKeySequence(
        editLabelTextShortcut.isEmpty() ? defaultPreferences().editLabelTextShortcut() : editLabelTextShortcut);
    m_commitLabelTextShortcutEdit->setKeySequence(
        commitLabelTextShortcut.isEmpty() ? defaultPreferences().commitLabelTextShortcut() : commitLabelTextShortcut);
    m_backupPathEdit->setText(root.value(QStringLiteral("backupPath")).toString(defaultPreferences().backupPath()));
    m_backupIntervalSpinBox->setValue(
        root.value(QStringLiteral("backupIntervalSeconds")).toInt(defaultPreferences().backupIntervalSeconds()));

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
    appearance.insert(QStringLiteral("theme"), comboBoxDataOrText(m_applicationThemeComboBox));

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

    QJsonObject markerTextBubble;
    markerTextBubble.insert(QStringLiteral("fontFamily"),
                            m_usesDefaultMarkerTextBubbleFont ? QString() : m_markerTextBubbleFont.family());
    markerTextBubble.insert(QStringLiteral("fontPointSize"),
                            m_usesDefaultMarkerTextBubbleFont ? 0.0 : m_markerTextBubbleFont.pointSizeF());
    markerTextBubble.insert(QStringLiteral("opacity"),
                            static_cast<double>(m_markerTextBubbleOpacityScrollBar->value()) / 100.0);

    QJsonObject input;
    input.insert(QStringLiteral("moveLabelModifier"), m_moveModifierComboBox->currentText().trimmed());
    input.insert(QStringLiteral("previousLabelModifier"), m_previousLabelModifierComboBox->currentText().trimmed());
    input.insert(QStringLiteral("undoShortcut"),
                 m_undoShortcutEdit->keySequence().toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("redoShortcut"),
                 m_redoShortcutEdit->keySequence().toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("nextLabelShortcut"),
                 m_nextLabelShortcutEdit->keySequence().toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("alternatePreviousLabelShortcut"),
                 m_alternatePreviousLabelShortcutEdit->keySequence().toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("alternateNextLabelShortcut"),
                 m_alternateNextLabelShortcutEdit->keySequence().toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("previousPageShortcut"),
                 m_previousPageShortcutEdit->keySequence().toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("nextPageShortcut"),
                 m_nextPageShortcutEdit->keySequence().toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("editLabelTextShortcut"),
                 m_editLabelTextShortcutEdit->keySequence().toString(QKeySequence::PortableText));
    input.insert(QStringLiteral("commitLabelTextShortcut"),
                 m_commitLabelTextShortcutEdit->keySequence().toString(QKeySequence::PortableText));

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
    root.insert(QStringLiteral("markerTextBubble"), markerTextBubble);
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

void PreferenceDialog::chooseMarkerTextBubbleFont()
{
    const std::optional<QFont> selectedFont =
        chooseFontWithQtDialog(this, m_usesDefaultMarkerTextBubbleFont ? this->font() : m_markerTextBubbleFont,
                               tr("Choose marker text bubble font"));
    if (!selectedFont.has_value()) {
        return;
    }

    m_markerTextBubbleFont = selectedFont.value();
    m_usesDefaultMarkerTextBubbleFont = false;
    updateMarkerTextBubbleFontSummary();
    updateJsonPreview();
}

void PreferenceDialog::resetMarkerTextBubbleFont()
{
    m_markerTextBubbleFont = font();
    m_usesDefaultMarkerTextBubbleFont = true;
    updateMarkerTextBubbleFontSummary();
    updateJsonPreview();
}

void PreferenceDialog::updateMarkerTextBubbleFontSummary()
{
    if (m_markerTextBubbleFontLabel == nullptr) {
        return;
    }

    if (m_usesDefaultMarkerTextBubbleFont) {
        m_markerTextBubbleFontLabel->setText(tr("Default font and size"));
        m_markerTextBubbleFontLabel->setFont(font());
        return;
    }

    m_markerTextBubbleFontLabel->setText(
        tr("%1, %2 pt").arg(m_markerTextBubbleFont.family()).arg(m_markerTextBubbleFont.pointSizeF(), 0, 'f', 1));
    m_markerTextBubbleFontLabel->setFont(font());
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
