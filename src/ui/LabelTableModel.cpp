#include "ui/LabelTableModel.h"

#include <QBrush>

#include <utility>

LabelTableModel::LabelTableModel(QObject* parent) : QAbstractTableModel(parent) {}

void LabelTableModel::setLabels(QVector<labelminus::core::Label>* labels)
{
    beginResetModel();
    m_labels = labels;
    rebuildVisibleRows();
    endResetModel();
}

void LabelTableModel::setGroupFilter(QStringList groupFilter)
{
    beginResetModel();
    m_groupFilter = QSet<QString>(groupFilter.cbegin(), groupFilter.cend());
    rebuildVisibleRows();
    endResetModel();
}

void LabelTableModel::setGroups(QStringList groups, QVector<labelminus::core::LabelGroupStyle> groupStyles)
{
    m_groups = std::move(groups);
    m_groupStyles = std::move(groupStyles);
    if (rowCount() > 0) {
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    }
}

void LabelTableModel::refresh()
{
    beginResetModel();
    rebuildVisibleRows();
    endResetModel();
}

void LabelTableModel::labelChanged(int row)
{
    const int visibleRow = rowForSourceIndex(row);
    if (visibleRow < 0) {
        return;
    }

    emit dataChanged(index(visibleRow, 0), index(visibleRow, columnCount() - 1));
}

int LabelTableModel::sourceIndexForRow(int row) const
{
    if (row < 0 || row >= static_cast<int>(m_visibleRows.size())) {
        return -1;
    }
    return m_visibleRows.at(row);
}

int LabelTableModel::rowForSourceIndex(int sourceIndex) const
{
    for (int row = 0; row < static_cast<int>(m_visibleRows.size()); ++row) {
        if (m_visibleRows.at(row) == sourceIndex) {
            return row;
        }
    }
    return -1;
}

int LabelTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || m_labels == nullptr) {
        return 0;
    }
    return static_cast<int>(m_visibleRows.size());
}

int LabelTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 3;
}

QVariant LabelTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || m_labels == nullptr || index.row() < 0 || index.row() >= m_visibleRows.size()) {
        return {};
    }

    const int sourceIndex = m_visibleRows.at(index.row());
    const labelminus::core::Label& label = m_labels->at(sourceIndex);
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case 0:
            return sourceIndex + 1;
        case 1:
            return label.text();
        case 2:
            return label.group();
        default:
            return {};
        }
    }

    if (role == Qt::TextAlignmentRole && index.column() != 1) {
        return Qt::AlignCenter;
    }

    if (role == Qt::ForegroundRole && index.column() == 2) {
        const QColor color = colorForGroup(label.group());
        if (color.isValid()) {
            return QBrush(color);
        }
    }

    return {};
}

bool LabelTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || m_labels == nullptr || index.row() < 0 ||
        index.row() >= m_visibleRows.size()) {
        return false;
    }

    const int sourceIndex = m_visibleRows.at(index.row());
    labelminus::core::Label& label = (*m_labels)[sourceIndex];
    QVariant oldValue;
    QVariant newValue;

    switch (index.column()) {
    case 1: {
        const QString text = value.toString();
        if (label.text() == text) {
            return false;
        }
        oldValue = label.text();
        newValue = text;
        label.setText(text);
        break;
    }
    case 2: {
        const QString group = value.toString();
        if (!m_groups.contains(group) || label.group() == group) {
            return false;
        }
        oldValue = label.group();
        newValue = group;
        label.setGroup(group);
        break;
    }
    default:
        return false;
    }

    emit dataChanged(index, index);
    emit labelEdited(sourceIndex, index.column(), oldValue, newValue);
    return true;
}

Qt::ItemFlags LabelTableModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags itemFlags = QAbstractTableModel::flags(index);
    if (index.isValid() && (index.column() == 1 || index.column() == 2)) {
        itemFlags |= Qt::ItemIsEditable;
    }
    return itemFlags;
}

void LabelTableModel::rebuildVisibleRows()
{
    m_visibleRows.clear();
    if (m_labels == nullptr) {
        return;
    }

    for (int i = 0; i < static_cast<int>(m_labels->size()); ++i) {
        const labelminus::core::Label& label = m_labels->at(i);
        if (!label.isDeleted() && m_groupFilter.contains(label.group())) {
            m_visibleRows.append(i);
        }
    }
}

QColor LabelTableModel::colorForGroup(const QString& group) const
{
    const int index = static_cast<int>(m_groups.indexOf(group));
    if (index < 0 || index >= static_cast<int>(m_groupStyles.size())) {
        return {};
    }
    return m_groupStyles.at(index).groupColor;
}

QVariant LabelTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
    case 0:
        return QStringLiteral("#");
    case 1:
        return QStringLiteral("文本");
    case 2:
        return QStringLiteral("类别");
    default:
        return {};
    }
}
