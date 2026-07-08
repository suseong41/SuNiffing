#include "mainwindow.h"
#include "theme.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    applyDarkTheme(app);
    qDebug() << "Debugger Hello";
    MainWindow win;
    win.show();
    return app.exec();
}
