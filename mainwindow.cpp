#include "mainwindow.h"
#include "./device.h"
#include "./ui_mainwindow.h"
#include "./runner.h"
#include "./display.h"
#include "./ipc_proto.h"

// TODO: 라디오탭 CH가 아니라, tag 3에서 파싱해오자

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , isRunning(false)
{
    ui->setupUi(this);
    this->setWindowTitle("SuNiffing");

    // 버튼 연결
    connect(ui->startButton, &QPushButton::clicked, this, &MainWindow::onStartButton);
    connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::onStopButton);

    // 디바이스 연결
    ui->devIn->clear();
    std::vector<std::string> dev = Device::getInstance().getDevice();
    std::vector<std::string>::iterator it = dev.begin();
    while(it != dev.end())
    {
        ui->devIn->addItem(QString::fromStdString(*it));
        it++;
    }

    // 데몬 생성
    daemonProcess = new QProcess(this);
    connect(daemonProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::onDaemonOutput);
    connect(daemonProcess, &QProcess::readyReadStandardError, this, &MainWindow::onDaemonError);

    // 꾹 눌렀을 때 메뉴
    ui->listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listWidget, &QListWidget::customContextMenuRequested, this, &MainWindow::showContents);
    ui->listWidget->viewport()->installEventFilter(this);
    ui->listWidget->viewport()->grabGesture(Qt::TapAndHoldGesture);

    // 홉핑 채널
    ui->currentCh->setStyleSheet("color: #87CEFA; font-weight: bold;");
}

MainWindow::~MainWindow()
{
    if(daemonProcess != nullptr)
    {
        if(daemonProcess->state() == QProcess::Running)
        {
            daemonProcess->kill();
        }
    }
    delete ui;
}

void MainWindow::runDaemon()
{
    QStringList args;
#ifdef Q_OS_ANDROID
    QString targetPath = dropPcapDaemon();
    if(targetPath != "")
    {
        QString cmd = QString("%1 %2").arg(targetPath, QString::fromStdString(devType));
        args << "-c" << cmd;
        daemonProcess->start("su", args);
    }
#elif defined(Q_OS_MAC)
    QString targetPath = QCoreApplictation::applicationDirPath() + "/suseongdaemon";
    args << QString::fromStdString(devType);
    daemonProcess->start(targetPath,args);
#endif

    return;
}

void MainWindow::onStartButton()
{
    // 앱: 데몬 실행
    // 데몬: 모니터 모드 실행

    // 재실행시 무시
    if(isRunning) return;

    QString dev = ui->devIn->currentText();
    if(dev.isEmpty())
    {
        QMessageBox::warning(this, "ERROR", "선택된 디바이스 없음");
        return;
    }
    devType = dev.toStdString();
    ui->devIn->setEnabled(false);

    // 이상하게 nexutil c1은 드라이버가 뻣음..
    QString cmd = QString("svc wifi disable; "
                          "sleep 1.5; "
                          "ifconfig %1 up; "
                          "nexutil -d; "
                          "nexutil -k1; "
                          "nexutil -s0x613 -i -v2").arg(dev);

    qDebug() << "[EXEC] " << cmd;
    QProcess p;
    p.start("su", QStringList() << "-c" << cmd);
    p.waitForFinished(3000);

    QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();

    if(!out.isEmpty()) qDebug() << "[OUT]" << out;
    if(!err.isEmpty()) qDebug() << "[ERR]" << err;


    // 채널 홉핑
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::nextChannel);
    timer->start(500); // 0.5s

    runDaemon();
    isRunning = true;
}

void MainWindow::onStopButton()
{
    if(!isRunning) return;

    if(timer != nullptr && timer->isActive())
    {
        timer->stop();
        delete timer;
        timer = nullptr;
    }

    if (daemonProcess != nullptr && daemonProcess->state() == QProcess::Running)
    {
        daemonProcess->terminate();
        daemonProcess->waitForFinished(2000);
    }
    // todo: interface down , up
    QString dev = ui->devIn->currentText();
    QString cmd = QString(
                          "nexutil -m0; "
                          "svc wifi enable"
                          ).arg(dev);

    QProcess p;
    p.start("su", QStringList() << "-c" << cmd);
    p.waitForFinished(5000);

    QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if(!err.isEmpty())
    {
        qDebug() << "[CLEANUP ERROR]" << err;
    }

    isRunning = false;
    ui->devIn->setEnabled(true);

}

void MainWindow::onRender() {}

void MainWindow::onDaemonOutput()
{
    if(daemonProcess == nullptr) return;

    daemonBuffer.append(daemonProcess->readAllStandardOutput());
    const int packetSize = sizeof(ST_INFO);
    int totalBytes = daemonBuffer.size();
    int validBytes = (totalBytes/packetSize) * packetSize;
    if(validBytes == 0) return;

    const char* ptr = daemonBuffer.constData();

    for(int i=0; i<validBytes; i+=packetSize)
    {
        ST_INFO info;
        memcpy(&info, ptr+i, packetSize);

        QString bssid = QString::fromUtf8(info.bssid);
        QString essid = QString::fromUtf8(info.essid);
        QString pwr = QString::number(info.pwr);
        QString ch = QString::number(info.ch);

        if (bssid.isEmpty() || bssid == "00:00:00:00:00:00") continue;
        if (displayItem.contains(bssid))
        {
            QListWidgetItem* item = displayItem[bssid];
            display* rowWidget = qobject_cast<display*>(ui->listWidget->itemWidget(item));
            if (rowWidget) {
                rowWidget->updateInfo(essid, pwr, ch);
            }
            item->setData(Qt::UserRole, bssid);
            item->setData(Qt::UserRole + 1, essid);
        }
        else
        {
            QListWidgetItem* newItem = new QListWidgetItem(ui->listWidget);
            display* newWidget = new display(this);

            newWidget->setInfo(bssid, pwr, ch, essid);
            newItem->setSizeHint(newWidget->sizeHint());

            newItem->setData(Qt::UserRole, bssid);
            newItem->setData(Qt::UserRole + 1, essid);

            ui->listWidget->addItem(newItem);
            ui->listWidget->setItemWidget(newItem, newWidget);

            displayItem.insert(bssid, newItem);
        }
    }
    daemonBuffer.remove(0, validBytes);
}

void MainWindow::onDaemonError()
{
    if(daemonProcess == nullptr) return;

    QByteArray error = daemonProcess->readAllStandardError();
    if(!error.isEmpty())
    {
        qDebug() << "[DAEMON ERROR]" << error.trimmed();
    }
}

void MainWindow::nextChannel()
{
    int ch = hopSeq[hopIdx];
    QString cmd = QString("nexutil -k%1").arg(ch);
    QProcess::startDetached("su", QStringList() << "-c" << cmd);
    hopIdx = (hopIdx+1) % hopSeq.size();
    QString curCh = QString("%1").arg(ch, 2, 10, QChar('0'));
    ui->currentCh->setText(QString("CH:%1").arg(curCh));
}

void MainWindow::showContents(const QPoint &pos)
{
    QListWidgetItem *item = ui->listWidget->itemAt(pos);
    if(!item) return;

    QString bssid = item->data(Qt::UserRole).toString();
    QString essid = item->data(Qt::UserRole+1).toString();

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu {"
        "   background-color: white;"
        "   border: 1px solid lightgray;"
        "   min-width: 100px;" /* 메뉴 가로 넓이 */
        "}"
        "QMenu::item {"
        "   padding: 15px 15px;"
        "   font-size: 14pt;"
        "   color: black;"
        "}"
        "QMenu::item:selected {"
        "   background-color: #E0E0E0;"
        "}"
        );

    // 복사 메뉴
    QAction *copyBssidAct = menu.addAction("Copy BSSID");
    QAction *copyEssidAct = menu.addAction("Copy ESSID");
    // 공격 메뉴
    QAction *deauthAct = menu.addAction("Deauth Attack");
    QAction *stopAttackAct = menu.addAction("Stop Attack");

    QAction *selectedAction = menu.exec(ui->listWidget->viewport()->mapToGlobal(pos));
    QClipboard *clipboard = QApplication::clipboard();
    if(selectedAction == copyBssidAct)
    {
        clipboard->setText(bssid);
    }
    else if(selectedAction == copyEssidAct)
    {
        clipboard->setText(essid);
    }
    else if(selectedAction == deauthAct) // Deauth attack
    {
        if(daemonProcess && daemonProcess->state() == QProcess::Running)
        {
            ST_IPC_CMD cmd;
            memset(&cmd, 0, sizeof(ST_IPC_CMD));

            cmd.action = 2;
            strncpy(cmd.interface, devType.c_str(), 15);
            cmd.target_ap = qstringToMac(bssid);
            // 일단 브로드 캐스트로만
            for(int i=0; i<6; i++) cmd.target_st.mac[i] = 0xFF;

            daemonProcess->write((const char*)&cmd, sizeof(ST_IPC_CMD));
            QMessageBox::information(this, "DEAUTH ATTACK", "target: " + essid);
        }
    }
    else if(selectedAction == stopAttackAct)
    {
        if(daemonProcess && daemonProcess->state() == QProcess::Running)
        {
            ST_IPC_CMD cmd;
            memset(&cmd, 0, sizeof(ST_IPC_CMD));
            cmd.action = 1;
            daemonProcess->write((const char*)&cmd, sizeof(ST_IPC_CMD));
        }
    }

}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if(watched == ui->listWidget->viewport() && event->type() == QEvent::Gesture)
    {
        QGestureEvent *gestureEvent = static_cast<QGestureEvent*>(event);
        if(QGesture *gesture = gestureEvent->gesture(Qt::TapAndHoldGesture))
        {
            QTapAndHoldGesture *tapAndHold = static_cast<QTapAndHoldGesture*>(gesture);
            if(tapAndHold->state() == Qt::GestureFinished)
            {
                QPoint globalPos = tapAndHold->position().toPoint();
                QPoint viewportPos = ui->listWidget->viewport()->mapFromGlobal(globalPos);
                showContents(viewportPos);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

static QString dropPcapDaemon()
{
    // AppDataLocation -> /data/data/<패키지명>/files
    // QStandardPaths::writeableLocation -> 쓰기권한을 가지는 시스템 경로 QString
    QString targetDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(targetDir);

    if(!dir.exists())
    {
        // .은 현재 디렉터리
        dir.mkpath(".");
    }

    QString targetPath = targetDir + "/suseong";
    QFile targetFile(targetPath);

    if(targetFile.exists())
    {
        // 기존 파일 발견시 제거
        targetFile.remove();
    }
    // assets에서 추출해옴
    QFile assetFile("assets:/suseong");

    // chmod 755
    if(assetFile.copy(targetPath))
    {
        QFile::setPermissions(targetPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                                              QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                                              QFileDevice::ReadOther | QFileDevice::ExeOther);
        return targetPath;
    }

    return QString("");
}

static ST_MAC qstringToMac(const QString& macStr)
{
    ST_MAC mac;
    uint32_t temp[6];
    sscanf(macStr.toStdString().c_str(), "%x:%x:%x:%x:%x:%x",
           &temp[0], &temp[1], &temp[2], &temp[3], &temp[4], &temp[5]);
    for(int i=0; i<6; ++i) mac.mac[i] = (uint8_t)temp[i];
    return mac;
}
