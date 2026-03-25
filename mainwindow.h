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
    void onReceivePacket();
    void onDaemonOutput();
    void onDaemonError();
private:
    void runDaemon();
    void killDaemon();
    Ui::MainWindow *ui;

    std::string devType;
    bool isRunning;
    QProcess *daemonProcess;
};

static QString dropPcapDaemon();
