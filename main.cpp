#include "mainwindow.h"
#include "theme.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    applyDarkTheme(a);   // 전역 다크 테마 (B-6)
    qDebug() << "Debugger Hello";
    MainWindow w;
    w.show();
    return a.exec();
}
