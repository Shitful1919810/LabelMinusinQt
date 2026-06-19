#pragma once

#include "core/Label.h"

#include <QAbstractTableModel>
#include <QColor>
#include <QSet>
#include <QStringList>
#include <QVector>

class LabelTableModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit LabelTableModel(QObject* parent = nullptr);

    void setLabels(QVector<labelminus::core::Label>* labels);
    void setGroupFilter(QStringList groupFilter);
    void setGroups(QStringList groups, QVector<QColor> groupColors);
    void refresh();
    void labelChanged(int row);
    int sourceIndexForRow(int row) const;
    int rowForSourceIndex(int sourceIndex) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

signals:
    void labelEdited(int sourceIndex, int column);

private:
    void rebuildVisibleRows();
    QColor colorForGroup(const QString& group) const;

    QVector<labelminus::core::Label>* m_labels{nullptr};
    QVector<int> m_visibleRows;
    QSet<QString> m_groupFilter;
    QStringList m_groups;
    QVector<QColor> m_groupColors;
};
