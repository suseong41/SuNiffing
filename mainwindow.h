#pragma once
#include <QMainWindow>
#include <string>
#include <QTableWidget>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QProcess>
#include <QCoreApplication>
#include <QScroller>
#include <QMessageBox>
#include <QMap>
#include <QListWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void onStartButton();
    void onStopButton();
    void onRender();
    void onDaemonOutput();
    void onDaemonError();
private:
    void runDaemon();
    void killDaemon();
    Ui::MainWindow *ui;

    std::string devType;
    bool isRunning;
    QProcess *daemonProcess;

    QMap<QString, QListWidgetItem*> displayItem;
};

static QString dropPcapDaemon();
