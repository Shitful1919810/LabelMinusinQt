#pragma once

#include "core/AppPreferences.h"

#include <QDialog>
#include <QFont>
#include <QJsonDocument>
#include <QString>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

class PreferenceDialog final : public QDialog {
    Q_OBJECT

public:
    explicit PreferenceDialog(QString preferencePath, QWidget* parent = nullptr);

signals:
    void preferencesApplied(labelminus::core::AppPreferencesLoadResult result);

private:
    void createUi();
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
    void applyPreferences();
    void savePreferences();
    void openPreferenceFile();

    QString m_preferencePath;
    QDoubleSpinBox* m_markerDiameterSpinBox{nullptr};
    QDoubleSpinBox* m_markerFontSpinBox{nullptr};
    QSpinBox* m_tableMaxRowsSpinBox{nullptr};
    QLabel* m_labelTableFontLabel{nullptr};
    QPushButton* m_chooseLabelTableFontButton{nullptr};
    QPushButton* m_resetLabelTableFontButton{nullptr};
    QLabel* m_textEditorFontLabel{nullptr};
    QPushButton* m_chooseTextEditorFontButton{nullptr};
    QPushButton* m_resetTextEditorFontButton{nullptr};
    QComboBox* m_moveModifierComboBox{nullptr};
    QLineEdit* m_backupPathEdit{nullptr};
    QSpinBox* m_backupIntervalSpinBox{nullptr};
    QTableWidget* m_groupStyleTable{nullptr};
    QPlainTextEdit* m_jsonPreview{nullptr};
    QLabel* m_messageLabel{nullptr};
    QPushButton* m_applyButton{nullptr};
    QPushButton* m_saveButton{nullptr};
    QFont m_labelTableFont;
    QFont m_textEditorFont;
    bool m_usesDefaultLabelTableFont{true};
    bool m_usesDefaultTextEditorFont{true};
};
