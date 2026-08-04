#include "mainwindow.h"

#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("RIGOL Oscilloscope Control Panel - Qt6 C++");
    resize(1200, 750);

    auto *label = new QLabel("RIGOL Oscilloscope GUI - C++ Qt6", this);
    label->setAlignment(Qt::AlignCenter);
    setCentralWidget(label);
}