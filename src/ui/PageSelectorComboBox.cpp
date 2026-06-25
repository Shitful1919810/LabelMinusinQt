#include "ui/PageSelectorComboBox.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QStyleOptionViewItem>
#include <QStylePainter>
#include <QStyledItemDelegate>

namespace {
constexpr QLatin1Char pageSeparator{'|'};

QString pageDisplayText(int pageIndex, const QString& imageName)
{
    return QStringLiteral("%1%2%3").arg(pageIndex + 1, 3, 10, QLatin1Char('0')).arg(pageSeparator).arg(imageName);
}

void drawPageText(QPainter* painter, const QRect& rect, const QString& text, const QPalette& palette, bool enabled,
                  bool selected = false)
{
    const qsizetype separatorIndex = text.indexOf(pageSeparator);
    if (separatorIndex < 0) {
        painter->setPen(palette.color(enabled ? QPalette::Normal : QPalette::Disabled, QPalette::Text));
        painter->drawText(rect, Qt::AlignVCenter | Qt::AlignLeft, text);
        return;
    }

    const QString pageNumber = text.left(separatorIndex + 1);
    const QString imageName = text.mid(separatorIndex + 1);
    const QColor pageColor = palette.color(enabled ? QPalette::Disabled : QPalette::Disabled, QPalette::Text);
    const QColor imageColor = selected ? palette.color(QPalette::HighlightedText)
                                       : palette.color(enabled ? QPalette::Normal : QPalette::Disabled, QPalette::Text);

    const QFontMetrics metrics(painter->font());
    QRect textRect = rect;
    painter->setPen(pageColor);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, pageNumber);

    const int pageWidth = metrics.horizontalAdvance(pageNumber + QLatin1Char(' '));
    textRect.adjust(pageWidth, 0, 0, 0);
    painter->setPen(imageColor);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                      metrics.elidedText(imageName, Qt::ElideMiddle, textRect.width()));
}

class PageSelectorItemDelegate final : public QStyledItemDelegate {
public:
    explicit PageSelectorItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem itemOption(option);
        initStyleOption(&itemOption, index);
        const QString text = itemOption.text;
        itemOption.text.clear();

        const QWidget* widget = itemOption.widget;
        QStyle* style = widget == nullptr ? QApplication::style() : widget->style();
        style->drawControl(QStyle::CE_ItemViewItem, &itemOption, painter, widget);

        const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &itemOption, widget);
        const bool selected = (option.state & QStyle::State_Selected) != 0;
        drawPageText(painter, textRect, text, itemOption.palette, (option.state & QStyle::State_Enabled) != 0,
                     selected);
    }
};
} // namespace

PageSelectorComboBox::PageSelectorComboBox(QWidget* parent) : QComboBox(parent)
{
    setItemDelegate(new PageSelectorItemDelegate(this));
    if (view() != nullptr) {
        view()->setTextElideMode(Qt::ElideMiddle);
    }
}

void PageSelectorComboBox::addPage(const QString& imageName, int pageIndex)
{
    addItem(pageDisplayText(pageIndex, imageName), imageName);
}

void PageSelectorComboBox::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QStylePainter painter(this);
    QStyleOptionComboBox option;
    initStyleOption(&option);
    option.currentText.clear();
    painter.drawComplexControl(QStyle::CC_ComboBox, option);

    const QRect textRect =
        style()->subControlRect(QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, this).adjusted(4, 0, -4, 0);
    drawPageText(&painter, textRect, currentText(), palette(), isEnabled());
}
