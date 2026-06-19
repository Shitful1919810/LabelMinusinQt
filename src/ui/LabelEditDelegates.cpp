#include "ui/LabelEditDelegates.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QStyle>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace {
constexpr int groupPopupDelayMs = 120;
constexpr int maximumPreviewTextLines = 4;
constexpr int textCellVerticalPadding = 8;
} // namespace

LabelTextDelegate::LabelTextDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

QWidget* LabelTextDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const
{
    auto* editor = new QPlainTextEdit(parent);
    editor->setFrameShape(QFrame::NoFrame);
    editor->installEventFilter(const_cast<LabelTextDelegate*>(this));
    return editor;
}

void LabelTextDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto* textEdit = qobject_cast<QPlainTextEdit*>(editor);
    if (textEdit == nullptr) {
        return;
    }

    textEdit->setPlainText(index.data(Qt::EditRole).toString());
    textEdit->selectAll();
}

void LabelTextDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    auto* textEdit = qobject_cast<QPlainTextEdit*>(editor);
    if (textEdit == nullptr) {
        return;
    }

    model->setData(index, textEdit->toPlainText(), Qt::EditRole);
}

QSize LabelTextDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    const int maximumHeight = option.fontMetrics.lineSpacing() * maximumPreviewTextLines + textCellVerticalPadding;
    size.setHeight(std::min(size.height(), maximumHeight));
    return size;
}

void LabelTextDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                                             const QModelIndex&) const
{
    editor->setGeometry(option.rect);
}

bool LabelTextDelegate::eventFilter(QObject* object, QEvent* event)
{
    auto* editor = qobject_cast<QPlainTextEdit*>(object);
    if (editor == nullptr) {
        return QStyledItemDelegate::eventFilter(object, event);
    }

    if (event->type() == QEvent::FocusOut) {
        emit commitData(editor);
        emit closeEditor(editor);
        return false;
    }

    if (event->type() == QEvent::KeyPress) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
            emit commitData(editor);
            emit closeEditor(editor);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            emit closeEditor(editor, QAbstractItemDelegate::RevertModelCache);
            return true;
        }
    }

    return QStyledItemDelegate::eventFilter(object, event);
}

LabelGroupDelegate::LabelGroupDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void LabelGroupDelegate::setGroups(QStringList groups, QVector<QColor> groupColors)
{
    m_groups = std::move(groups);
    m_groupColors = std::move(groupColors);
}

QWidget* LabelGroupDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const
{
    auto* comboBox = new QComboBox(parent);
    comboBox->addItems(m_groups);
    for (int i = 0; i < comboBox->count(); ++i) {
        const QColor color = colorForGroup(comboBox->itemText(i));
        if (color.isValid()) {
            comboBox->setItemData(i, color, Qt::ForegroundRole);
        }
    }
    connect(comboBox, &QComboBox::activated, this, [delegate = const_cast<LabelGroupDelegate*>(this), comboBox]() {
        emit delegate->commitData(comboBox);
        emit delegate->closeEditor(comboBox);
    });
    const QPointer<QComboBox> guardedComboBox(comboBox);
    QTimer::singleShot(groupPopupDelayMs, comboBox, [guardedComboBox]() {
        if (guardedComboBox != nullptr) {
            guardedComboBox->showPopup();
        }
    });
    return comboBox;
}

void LabelGroupDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto* comboBox = qobject_cast<QComboBox*>(editor);
    if (comboBox == nullptr) {
        return;
    }

    comboBox->setCurrentText(index.data(Qt::EditRole).toString());
}

void LabelGroupDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    auto* comboBox = qobject_cast<QComboBox*>(editor);
    if (comboBox == nullptr) {
        return;
    }

    model->setData(index, comboBox->currentText(), Qt::EditRole);
}

void LabelGroupDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem coloredOption(option);
    initStyleOption(&coloredOption, index);

    const QColor color = colorForGroup(index.data(Qt::DisplayRole).toString());
    if (color.isValid()) {
        coloredOption.palette.setColor(QPalette::Text, color);
        coloredOption.palette.setColor(QPalette::HighlightedText, color);
    }

    const QWidget* widget = option.widget;
    QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &coloredOption, painter, widget);
}

QColor LabelGroupDelegate::colorForGroup(const QString& group) const
{
    const int index = static_cast<int>(m_groups.indexOf(group));
    if (index < 0 || index >= static_cast<int>(m_groupColors.size())) {
        return {};
    }
    return m_groupColors.at(index);
}
