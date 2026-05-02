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
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>
#include <QGestureEvent>
#include <QTapAndHoldGesture>
#include <QTimer>
#include "mac.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
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
    void showContents(const QPoint &pos);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void runDaemon();
    Ui::MainWindow *ui;

    std::string devType;
    bool isRunning;
    QProcess *daemonProcess;
    QByteArray daemonBuffer;

    QTimer* timer;
    const QList<int> hopSeq = {1, 6, 11, 2, 7, 12, 3, 8, 13, 4, 9, 5, 10}; // 미국 ~11, 일본 ~14 어댑터 찾기 iw_list
    int hopIdx = 0;
    void nextChannel();

    QMap<QString, QListWidgetItem*> displayItem;
};

static QString dropPcapDaemon();
static ST_MAC qstringToMac(const QString& macStr);
