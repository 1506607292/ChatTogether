#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowTitle("Chat Together");
    w.show();
    w.setWindowIcon(*w.icon);
    return a.exec();
}
