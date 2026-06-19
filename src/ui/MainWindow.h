#pragma once

#include "ui/ImageCanvas.h"

#include <QMainWindow>

class QAction;
class QLabel;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void createActions();
    void createMenus();
    void openImage();

    ImageCanvas *m_canvas{nullptr};
    QLabel *m_statusLabel{nullptr};
    QAction *m_openImageAction{nullptr};
    QAction *m_quitAction{nullptr};
};

