#pragma once

#include "core/AppPreferences.h"

#include <QDialog>
#include <QJsonDocument>
#include <QString>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
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
    void applyPreferences();
    void savePreferences();
    void openPreferenceFile();

    QString m_preferencePath;
    QDoubleSpinBox* m_markerDiameterSpinBox{nullptr};
    QDoubleSpinBox* m_markerFontSpinBox{nullptr};
    QSpinBox* m_tableMaxRowsSpinBox{nullptr};
    QComboBox* m_moveModifierComboBox{nullptr};
    QTableWidget* m_groupStyleTable{nullptr};
    QPlainTextEdit* m_jsonPreview{nullptr};
    QLabel* m_messageLabel{nullptr};
    QPushButton* m_applyButton{nullptr};
    QPushButton* m_saveButton{nullptr};
};
