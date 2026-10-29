#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName(QStringLiteral("QtCalculator"));
    a.setApplicationDisplayName(QStringLiteral("计算器"));

    MainWindow w;
    w.show();
    return a.exec();
}
