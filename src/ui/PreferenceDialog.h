#pragma once

#include "core/AppPreferences.h"
#include "services/AutomationService.h"

#include <QDialog>
#include <QFont>
#include <QJsonDocument>
#include <QString>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QScrollBar;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class QWidget;

class PreferenceDialog final : public QDialog {
    Q_OBJECT

public:
    PreferenceDialog(QString preferencePath, labelqt::core::AppPreferences currentPreferences,
                     QVector<labelqt::services::AutomationScript> automationScripts, QWidget* parent = nullptr);

signals:
    void preferencesApplied(labelqt::core::AppPreferencesLoadResult result);

private:
    void createUi();
    QWidget* createGeneralPage(QTabWidget* tabWidget);
    QWidget* createKeyMappingPage(QTabWidget* tabWidget);
    QWidget* createAutomationShortcutsPage(QTabWidget* tabWidget);
    QWidget* createGroupStylesPage(QTabWidget* tabWidget);
    QWidget* createJsonPage(QTabWidget* tabWidget);
    void connectPreferenceChangeSignals();
    void loadFromDisk();
    void loadDocument(const QJsonDocument& document);
    QJsonDocument documentFromUi() const;
    void updateJsonPreview();
    void setMessage(const QString& message, bool warning = false);
    void addGroupStyleRow();
    void addGroupStyleRow(const QJsonObject& style);
    void removeSelectedGroupStyleRows();
    void chooseGroupColor(int row);
    void chooseLabelTableFont();
    void resetLabelTableFont();
    void updateLabelTableFontSummary();
    void chooseTextEditorFont();
    void resetTextEditorFont();
    void updateTextEditorFontSummary();
    void chooseMarkerTextBubbleFont();
    void resetMarkerTextBubbleFont();
    void updateMarkerTextBubbleFontSummary();
    QString automationShortcutConflictText() const;
    void savePreferences();
    void openPreferenceFile();

    QString m_preferencePath;
    labelqt::core::AppPreferences m_currentPreferences;
    QVector<labelqt::services::AutomationScript> m_automationScripts;
    QDoubleSpinBox* m_markerDiameterSpinBox{nullptr};
    QDoubleSpinBox* m_markerFontSpinBox{nullptr};
    QSpinBox* m_tableMaxRowsSpinBox{nullptr};
    QComboBox* m_applicationStyleComboBox{nullptr};
    QComboBox* m_applicationThemeComboBox{nullptr};
    QComboBox* m_applicationLanguageComboBox{nullptr};
    QCheckBox* m_showAutomationRunLogCheckBox{nullptr};
    QLabel* m_labelTableFontLabel{nullptr};
    QPushButton* m_chooseLabelTableFontButton{nullptr};
    QPushButton* m_resetLabelTableFontButton{nullptr};
    QLabel* m_textEditorFontLabel{nullptr};
    QPushButton* m_chooseTextEditorFontButton{nullptr};
    QPushButton* m_resetTextEditorFontButton{nullptr};
    QLabel* m_markerTextBubbleFontLabel{nullptr};
    QPushButton* m_chooseMarkerTextBubbleFontButton{nullptr};
    QPushButton* m_resetMarkerTextBubbleFontButton{nullptr};
    QScrollBar* m_markerTextBubbleOpacityScrollBar{nullptr};
    QLabel* m_markerTextBubbleOpacityLabel{nullptr};
    QScrollBar* m_canvasLabelTextEditorOpacityScrollBar{nullptr};
    QLabel* m_canvasLabelTextEditorOpacityLabel{nullptr};
    QComboBox* m_moveModifierComboBox{nullptr};
    QComboBox* m_previousLabelModifierComboBox{nullptr};
    QKeySequenceEdit* m_undoShortcutEdit{nullptr};
    QKeySequenceEdit* m_redoShortcutEdit{nullptr};
    QKeySequenceEdit* m_nextLabelShortcutEdit{nullptr};
    QKeySequenceEdit* m_alternatePreviousLabelShortcutEdit{nullptr};
    QKeySequenceEdit* m_alternateNextLabelShortcutEdit{nullptr};
    QKeySequenceEdit* m_previousPageShortcutEdit{nullptr};
    QKeySequenceEdit* m_nextPageShortcutEdit{nullptr};
    QKeySequenceEdit* m_editLabelTextShortcutEdit{nullptr};
    QKeySequenceEdit* m_commitLabelTextShortcutEdit{nullptr};
    QTableWidget* m_automationShortcutTable{nullptr};
    QLineEdit* m_backupPathEdit{nullptr};
    QSpinBox* m_backupIntervalSpinBox{nullptr};
    QTableWidget* m_groupStyleTable{nullptr};
    QPlainTextEdit* m_jsonPreview{nullptr};
    QLabel* m_messageLabel{nullptr};
    QPushButton* m_saveButton{nullptr};
    QFont m_labelTableFont;
    QFont m_textEditorFont;
    QFont m_markerTextBubbleFont;
    bool m_usesDefaultLabelTableFont{true};
    bool m_usesDefaultTextEditorFont{true};
    bool m_usesDefaultMarkerTextBubbleFont{true};
};
