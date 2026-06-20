#pragma once

#include "core/AppPreferences.h"

#include <QColor>
#include <QPersistentModelIndex>
#include <QStringList>
#include <QStyledItemDelegate>
#include <QVector>

class LabelTextDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit LabelTextDelegate(QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const override;

signals:
    void editorHeightHintChanged(QPersistentModelIndex index, QWidget* editor, int height);

protected:
    bool eventFilter(QObject* object, QEvent* event) override;
};

class LabelGroupDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit LabelGroupDelegate(QObject* parent = nullptr);

    void setGroups(QStringList groups, QVector<labelminus::core::LabelGroupStyle> groupStyles);
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    QColor colorForGroup(const QString& group) const;

    QStringList m_groups;
    QVector<labelminus::core::LabelGroupStyle> m_groupStyles;
};
