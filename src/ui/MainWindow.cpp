#include "ui/MainWindow.h"

#include <QAction>
#include <QFileDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_canvas(new ImageCanvas(this))
    , m_statusLabel(new QLabel(this))
{
    setWindowTitle(QStringLiteral("LabelMinus"));
    resize(1200, 800);

    setCentralWidget(m_canvas);
    statusBar()->addWidget(m_statusLabel);
    m_statusLabel->setText(QStringLiteral("Ready"));

    createActions();
    createMenus();
}

void MainWindow::createActions()
{
    m_openImageAction = new QAction(tr("&Open Image..."), this);
    m_openImageAction->setShortcut(QKeySequence::Open);
    connect(m_openImageAction, &QAction::triggered, this, &MainWindow::openImage);

    m_quitAction = new QAction(tr("&Quit"), this);
    m_quitAction->setShortcut(QKeySequence::Quit);
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_openImageAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAction);
}

void MainWindow::openImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open image"),
        QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));

    if (path.isEmpty()) {
        return;
    }

    m_canvas->openImage(path);
    m_statusLabel->setText(path);
}
