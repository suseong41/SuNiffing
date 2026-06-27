#include "mainwindow.h"
#include "./device.h"
#include "./ui_mainwindow.h"
#include "./runner.h"
#include "./display.h"
#include "./ipc_proto.h"

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

    // AP | STATION 전환 토글
    connect(ui->viewToggle, &QSlider::valueChanged, this, &MainWindow::onViewToggleChange);
    onViewToggleChange(0);
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
        QString cmd = QString("LD_PRELOAD=/data/local/tmp/libnexmon.so %1 %2")
        .arg(targetPath, QString::fromStdString(devType));
        args << "-c" << cmd;
        daemonProcess->start("su", args);
    }
#elif defined(Q_OS_MAC)
    QString targetPath = QCoreApplication::applicationDirPath() + "/suseong";
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
    QString cmd = QString("nexutil -m0; svc wifi enable").arg(dev);

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
    const int packetSize = sizeof(ST_IPC_EVENT);
    int totalBytes = daemonBuffer.size();
    int validBytes = (totalBytes/packetSize) * packetSize;
    if(validBytes == 0) return;

    const char* ptr = daemonBuffer.constData();

    for(int i=0; i<validBytes; i+=packetSize)
    {
        ST_IPC_EVENT event;
        memcpy(&event, ptr+i, packetSize);

        uint8_t type = event.type;
        QString displayMac;
        QString displayEssid;
        QString pwr = QString::number(event.pwr);
        QString ch = QString::number(event.ch);

        if(type == 0)
        {
            char bssidStr[18];
            prtMac(bssidStr, sizeof(bssidStr), event.bssid);
            displayMac = QString::fromUtf8(bssidStr);
            displayEssid = QString::fromUtf8(event.essid);
        }
        else if(type == 1)
        {
            char stMacStr[18];
            prtMac(stMacStr, sizeof(stMacStr), event.st_mac);
            displayMac = QString::fromUtf8(stMacStr);

            char linkedBssidStr[18];
            prtMac(linkedBssidStr, sizeof(linkedBssidStr), event.bssid);
            QString linked = QString::fromUtf8(linkedBssidStr);

            if(linked == "FF:FF:FF:FF:FF:FF" || linked == "00:00:00:00:00:00")
            {
                displayEssid = "(Not Associated)";
            }
            else
            {
                displayEssid = "To: " + linked;
            }
            ch = "-";
        }
        else continue;

        if(displayMac.isEmpty() || displayMac == "00:00:00:00:00:00" || displayMac == "FF:FF:FF:FF:FF:FF") continue;
        QString mapKey = QString::number(type) + "_" + displayMac;
        QString uiMacText = displayMac;
        if (type == 1)
        {
            uiMacText = "FROM: " + displayMac;
        }
        if (displayItem.contains(mapKey))
        {
            QListWidgetItem* item = displayItem[mapKey];
            display* rowWidget = qobject_cast<display*>(ui->listWidget->itemWidget(item));
            if (rowWidget) {
                rowWidget->updateInfo(displayEssid, pwr, ch);
            }
            item->setData(Qt::UserRole + itemRole::MacAddress, displayMac);
            item->setData(Qt::UserRole + itemRole::Essid, displayEssid);
            item->setData(Qt::UserRole + itemRole::Channel, ch);
        }
        else
        {
            QListWidgetItem* newItem = new QListWidgetItem();
            display* newWidget = new display(this);

            newWidget->setInfo(displayMac, pwr, ch, displayEssid);
            newItem->setSizeHint(newWidget->sizeHint());

            newItem->setData(Qt::UserRole + itemRole::MacAddress, displayMac);
            newItem->setData(Qt::UserRole + itemRole::Essid, displayEssid);
            newItem->setData(Qt::UserRole + itemRole::Channel, ch);
            newItem->setData(Qt::UserRole + itemRole::Type, type);

            newItem->setHidden(type != currentViewMode);

            ui->listWidget->addItem(newItem);
            ui->listWidget->setItemWidget(newItem, newWidget);

            displayItem.insert(mapKey, newItem);
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

    int targetCh = item->data(Qt::UserRole + itemRole::Channel).toInt();

    QString bssid = item->data(Qt::UserRole + itemRole::MacAddress).toString();
    QString essid = item->data(Qt::UserRole + itemRole::Essid).toString();
    uint8_t itemType = item->data(Qt::UserRole + itemRole::Type).toInt();

    QMenu menu(this);

    // 복사 메뉴
    QAction *copyBssidAct = menu.addAction("Copy BSSID");
    QAction *copyEssidAct = menu.addAction("Copy ESSID");
    menu.addSeparator();

    // 공격 메뉴
    QAction *deauthAct = nullptr;
    QAction *authAct = nullptr;
    QAction *csaAct = nullptr;
    if(itemType == 0)
    {
        deauthAct = menu.addAction("Deauth Attack");
        authAct = menu.addAction("Auth Attack");
        csaAct = menu.addAction("CSA Attack");
    }
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
    else if(itemType == 0 && (selectedAction == deauthAct || selectedAction == authAct || selectedAction == csaAct)) // Deauth attack
    {
        if(daemonProcess && daemonProcess->state() == QProcess::Running)
        {
            QStringList stationList;
            stationList << "Broadcast (FF:FF:FF:FF:FF:FF)";

            for(int i=0; i<ui->listWidget->count(); i++)
            {
                QListWidgetItem* stItem = ui->listWidget->item(i);
                if(stItem->data(Qt::UserRole+itemRole::Type).toInt() == 1)
                {
                    stationList << stItem->data(Qt::UserRole + itemRole::MacAddress).toString();
                }
            }

            attackDialog = new QDialog(this);
            attackDialog->setAttribute(Qt::WA_DeleteOnClose);

            if (selectedAction == deauthAct) { attackDialog->setWindowTitle("Deauth Attack"); attackType = 1; }
            else if (selectedAction == authAct) { attackDialog->setWindowTitle("Auth Attack"); attackType = 2; }
            else { attackDialog->setWindowTitle("CSA Attack"); attackType = 3; }

            attackTargetBssid = bssid;
            attackTargetCh = targetCh;

            QVBoxLayout *layout = new QVBoxLayout(attackDialog);

            QLabel *chLabel = new QLabel("Target Channel (goto):", attackDialog);
            attackChSpin = new QSpinBox(attackDialog);
            attackChSpin->setRange(1, 14);
            attackChSpin->setValue(11);
            layout->addWidget(chLabel);
            layout->addWidget(attackChSpin);

            if (attackType != 3)
            {
                chLabel->hide();
                attackChSpin->hide();
            }

            layout->addWidget(new QLabel("Select Target Station:", attackDialog));
            attackStCombo = new QComboBox(attackDialog);
            attackStCombo->addItems(stationList);
            layout->addWidget(attackStCombo);

            QPushButton *okBtn = new QPushButton("Attack Start", attackDialog);
            layout->addWidget(okBtn);

            connect(okBtn, &QPushButton::clicked, attackDialog, &QDialog::accept);

            connect(attackDialog, &QDialog::accepted, this, &MainWindow::onAttackDialogAccepted);

            attackDialog->open();
        }
    }
    else if(selectedAction == stopAttackAct)
    {
        if(daemonProcess && daemonProcess->state() == QProcess::Running)
        {
            ST_IPC_CMD cmd;
            memset(&cmd, 0, sizeof(ST_IPC_CMD));
            cmd.action = Act::SNIFFING;
            strncpy(cmd.interface, devType.c_str(), 15);
            daemonProcess->write((const char*)&cmd, sizeof(ST_IPC_CMD));

            if(timer && !timer->isActive()) timer->start(500);
            ui->statusbar->showMessage("Attack Stopped", 3000);
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

void MainWindow::onViewToggleChange(int index)
{
    currentViewMode = index;
    if(index == 0)
    {
        ui->viewToggle->setStyleSheet(
            "QSlider::groove:horizontal { border-radius: 12px; height: 24px; background: #e0e0e0; }"
            "QSlider::handle:horizontal { background: #ffffff; border: 1px solid #999999; width: 22px; height: 22px; margin: 1px; border-radius: 11px; }"
            );
    }
    else
    {
        ui->viewToggle->setStyleSheet(
            "QSlider::groove:horizontal { border-radius: 12px; height: 24px; background: #87CEFA; }"
            "QSlider::handle:horizontal { background: #ffffff; border: 1px solid #999999; width: 22px; height: 22px; margin: 1px; border-radius: 11px; }"
            );
    }

    for(int i=0; i<ui->listWidget->count(); i++)
    {
        QListWidgetItem *item = ui->listWidget->item(i);
        if(item)
        {
            int itemType = item->data(Qt::UserRole + itemRole::Type).toInt();
            item->setHidden(itemType != currentViewMode);
        }
    }
}

void MainWindow::onAttackDialogAccepted()
{
    if (!attackStCombo || !attackChSpin) return;
    QString targetStStr = attackStCombo->currentText();
    int chToMove = attackChSpin->value();

    if(timer && timer->isActive()) timer->stop();
    QString chCmd = QString("nexutil -k%1").arg(attackTargetCh);
    QProcess::startDetached("su", QStringList() << "-c" << chCmd);
    ui->currentCh->setText(QString("CH: %1 [is attacking]").arg(attackTargetCh));

    QString pureMac = targetStStr;
    if(targetStStr.startsWith("Broadcast")) pureMac = "FF:FF:FF:FF:FF:FF";

    ST_IPC_CMD cmd;
    memset(&cmd, 0, sizeof(ST_IPC_CMD));

    if (attackType == 1) cmd.action = Act::DEAUTH;
    else if (attackType == 2) cmd.action = Act::AUTH;
    else cmd.action = Act::CSA;

    strncpy(cmd.interface, devType.c_str(), 15);
    cmd.target_ap = qstringToMac(attackTargetBssid);
    cmd.target_st = qstringToMac(pureMac);
    cmd.channel = (attackType == 3) ? chToMove : attackTargetCh;

    if (daemonProcess && daemonProcess->state() == QProcess::Running)
    {
        daemonProcess->write((const char*)&cmd, sizeof(ST_IPC_CMD));
    }
    ui->statusbar->showMessage("Attack started to " + pureMac, 3000);
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
    uint32_t temp[6] = {0};
    if(sscanf(macStr.toStdString().c_str(), "%x:%x:%x:%x:%x:%x",
            &temp[0], &temp[1], &temp[2], &temp[3], &temp[4], &temp[5]) != 6)
    {
        memset(&mac, 0, sizeof(mac));
        return mac;
    }
    for(int i=0; i<6; ++i) mac.mac[i] = (uint8_t)temp[i];
    return mac;
}
