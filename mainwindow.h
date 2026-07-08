#pragma once
#include <QMainWindow>
#include <string>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QProcess>
#include <QCoreApplication>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>
#include <QGestureEvent>
#include <QTapAndHoldGesture>
#include <QTimer>
#include <QDialog>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QComboBox>
#include <QButtonGroup>
#include <QStandardItemModel>
#include <QHash>
#include <QListView>
#include <QLineEdit>
#include <QDateTime>
#include <QAbstractItemView>
#include <QScroller>
#include <QStyle>
#include "./mac.h"
#include "./devicelist.h"


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

    QTimer* timer = nullptr;
    const QList<int> hopSeq = {1, 6, 11, 2, 7, 12, 3, 8, 13, 4, 9, 5, 10}; // 미국 ~11, 일본 ~14 어댑터 찾기 iw_list
    int hopIdx = 0;
    void nextChannel();

    QStandardItemModel* devModel = nullptr;
    DeviceProxy* devProxy = nullptr;
    QHash<QString, QStandardItem*> itemByKey;
    QTimer* ageTimer = nullptr;
    void ageDevices();
    void updateEmptyState();
    void updateScanButton();

    void onViewToggleChange(int index);
    int currentViewMode = 0;

    void onAttackDialogAccepted();
    QDialog *attackDialog = nullptr;
    QComboBox *attackStCombo = nullptr;
    QSpinBox *attackChSpin = nullptr;
    int attackType = 0;
    QString attackTargetBssid;
    int attackTargetCh = 0;

    bool attacking = false;
    qint64 attackStartMs = 0;
    QString attackTypeName;
    QString attackTargetMac;
    QTimer* attackTimer = nullptr;
    void stopAttack();
    void updateAttackBanner();
    void markAttackTarget(const QString& key);
};

static QString dropPcapDaemon();
static QString dropNexmonLib();
static ST_MAC qstringToMac(const QString& macStr);
