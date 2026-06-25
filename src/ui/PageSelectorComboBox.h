#pragma once

#include <QComboBox>

class PageSelectorComboBox final : public QComboBox {
    Q_OBJECT

public:
    explicit PageSelectorComboBox(QWidget* parent = nullptr);

    void addPage(const QString& imageName, int pageIndex);

protected:
    void paintEvent(QPaintEvent* event) override;
};
