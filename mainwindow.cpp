#include "mainwindow.h"
#include "./device.h"
#include "./ui_mainwindow.h"
#include "./runner.h"
#include "./ipc_proto.h"
#ifdef Q_OS_MAC
#include "./macdebug.h"
#endif

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

    // 장치 리스트: model/view + 카드 델리게이트
    devModel = new QStandardItemModel(this);
    devProxy = new DeviceProxy(this);
    devProxy->setSourceModel(devModel);
    devProxy->setSortRole(dev::PwrRole);
    devProxy->setDynamicSortFilter(true);
    devProxy->sort(0, Qt::DescendingOrder);   // 강한 신호(PWR) 위로

    ui->listView->setModel(devProxy);
    ui->listView->setItemDelegate(new DeviceDelegate(this));
    ui->listView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->listView->setEditTriggers(QAbstractItemView::NoEditTriggers); // 더블클릭 인라인 편집 비활성

    // 꾹 눌렀을 때 메뉴
    ui->listView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->listView, &QListView::customContextMenuRequested, this, &MainWindow::showContents);
    ui->listView->viewport()->installEventFilter(this);
    ui->listView->viewport()->grabGesture(Qt::TapAndHoldGesture);

    // SSID/MAC 검색 + 빈 상태 갱신
    connect(ui->searchEdit, &QLineEdit::textChanged, this, [this](const QString& s){
        devProxy->setSearch(s);
        updateEmptyState();
    });
    connect(devProxy, &QAbstractItemModel::rowsInserted, this, &MainWindow::updateEmptyState);
    connect(devProxy, &QAbstractItemModel::rowsRemoved,  this, &MainWindow::updateEmptyState);
    connect(devProxy, &QAbstractItemModel::modelReset,   this, &MainWindow::updateEmptyState);

    // 에이징: 미관측 항목 흐림/제거
    ageTimer = new QTimer(this);
    connect(ageTimer, &QTimer::timeout, this, &MainWindow::ageDevices);
    ageTimer->start(1000);

    // 공격 상태 배너 + 원클릭 중단 (B-5)
    ui->attackBanner->setAttribute(Qt::WA_StyledBackground, true); // #attackBanner 배경 QSS 적용
    ui->attackBanner->setVisible(false);
    connect(ui->attackStopButton, &QPushButton::clicked, this, &MainWindow::stopAttack);
    attackTimer = new QTimer(this);
    connect(attackTimer, &QTimer::timeout, this, &MainWindow::updateAttackBanner);

    // 홉핑 채널 (#currentCh 스타일은 theme.h 전역 QSS 에서)

    // AP | STATION 전환 (세그먼티드 컨트롤)
    ui->segToggle->setAttribute(Qt::WA_StyledBackground, true); // #segToggle 배경 QSS 적용
    QButtonGroup* viewGroup = new QButtonGroup(this);
    viewGroup->setExclusive(true);
    viewGroup->addButton(ui->btnAP, 0);
    viewGroup->addButton(ui->btnStation, 1);
    connect(viewGroup, &QButtonGroup::idClicked, this, &MainWindow::onViewToggleChange);
    onViewToggleChange(0);

#ifdef Q_OS_MAC
    // macOS: 실제 인터페이스 없이 더미 데이터로 UI 테스트
    macDebug = new MacDebug(this);
    macDebug->onEvents = [this](const QByteArray& b){ injectDebugEvents(b); };
#endif
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

#ifdef Q_OS_MAC
    // macOS: 실제 인터페이스/su/데몬 없이 더미 데이터로 UI 테스트
    devType = ui->devIn->currentText().toStdString();
    ui->devIn->setEnabled(false);
    macDebug->start();
    isRunning = true;
    return;
#endif

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

    // 모니터 모드 준비를 비동기 실행 (UI 스레드 블록 -> ANR 방지)
    ui->startButton->setEnabled(false);
    QProcess* setup = new QProcess(this);
    connect(setup, &QProcess::finished, this,
            [this, setup](int, QProcess::ExitStatus)
    {
        QString err = QString::fromUtf8(setup->readAllStandardError()).trimmed();
        QString out = QString::fromUtf8(setup->readAllStandardOutput()).trimmed();
        if(!out.isEmpty()) qDebug() << "[OUT]" << out;
        if(!err.isEmpty()) qDebug() << "[ERR]" << err;
        setup->deleteLater();

        // 채널 홉핑
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &MainWindow::nextChannel);
        timer->start(500); // 0.5s

        runDaemon();
        isRunning = true;
        ui->startButton->setEnabled(true);
    });
    connect(setup, &QProcess::errorOccurred, this,
            [this, setup](QProcess::ProcessError e)
    {
        if(e == QProcess::FailedToStart)
        {
            qDebug() << "[ERR] setup failed to start:" << setup->errorString();
            setup->deleteLater();
            ui->startButton->setEnabled(true);
            ui->devIn->setEnabled(true);
        }
    });
    setup->start("su", QStringList() << "-c" << cmd);
}

void MainWindow::onStopButton()
{
    if(!isRunning) return;
    isRunning = false;

    // 공격 중이었다면 상태/배너 정리 (데몬은 곧 종료됨)
    if(attacking)
    {
        attacking = false;
        if(attackTimer) attackTimer->stop();
        markAttackTarget(QString());
        ui->attackBanner->setVisible(false);
    }

#ifdef Q_OS_MAC
    // macOS: 더미 주입 중지
    macDebug->stop();
    ui->devIn->setEnabled(true);
    return;
#endif

    if(timer != nullptr && timer->isActive())
    {
        timer->stop();
        delete timer;
        timer = nullptr;
    }

    if (daemonProcess != nullptr && daemonProcess->state() == QProcess::Running)
    {
        daemonProcess->terminate();   // 동기 waitForFinished 제거 (ANR 방지)
    }

    // todo: interface down , up
    // 인터페이스 복구를 비동기 실행 (UI 스레드 블록 -> ANR 방지)
    ui->stopButton->setEnabled(false);
    QString dev = ui->devIn->currentText();
    QString cmd = QString("nexutil -m0; svc wifi enable").arg(dev);

    QProcess* cleanup = new QProcess(this);
    connect(cleanup, &QProcess::finished, this,
            [this, cleanup](int, QProcess::ExitStatus)
    {
        QString err = QString::fromUtf8(cleanup->readAllStandardError()).trimmed();
        if(!err.isEmpty()) qDebug() << "[CLEANUP ERROR]" << err;
        cleanup->deleteLater();
        ui->devIn->setEnabled(true);
        ui->stopButton->setEnabled(true);
    });
    connect(cleanup, &QProcess::errorOccurred, this,
            [this, cleanup](QProcess::ProcessError e)
    {
        if(e == QProcess::FailedToStart)
        {
            qDebug() << "[CLEANUP ERROR] failed to start:" << cleanup->errorString();
            cleanup->deleteLater();
            ui->devIn->setEnabled(true);
            ui->stopButton->setEnabled(true);
        }
    });
    cleanup->start("su", QStringList() << "-c" << cmd);
}

void MainWindow::onRender() {}

void MainWindow::onDaemonOutput()
{
    if(daemonProcess == nullptr) return;
    daemonBuffer.append(daemonProcess->readAllStandardOutput());
    processDaemonBuffer();
}

void MainWindow::injectDebugEvents(const QByteArray& bytes)
{
    daemonBuffer.append(bytes);
    processDaemonBuffer();
}

void MainWindow::processDaemonBuffer()
{
    const int packetSize = sizeof(ST_IPC_EVENT);
    int totalBytes = daemonBuffer.size();
    int validBytes = (totalBytes/packetSize) * packetSize;
    if(validBytes == 0) return;

    const char* ptr = daemonBuffer.constData();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    for(int i=0; i<validBytes; i+=packetSize)
    {
        ST_IPC_EVENT event;
        memcpy(&event, ptr+i, packetSize);

        uint8_t type = event.type;
        QString displayMac;
        QString displayEssid;
        int pwr = event.pwr;
        int ch = 0;

        if(type == 0)
        {
            char bssidStr[18];
            prtMac(bssidStr, sizeof(bssidStr), event.bssid);
            displayMac = QString::fromUtf8(bssidStr);
            displayEssid = QString::fromUtf8(event.essid);
            ch = event.ch;
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
                displayEssid = "(Not Associated)";
            else
                displayEssid = "To: " + linked;
            ch = 0; // STA 행은 채널 표시 안 함
        }
        else continue;

        if(displayMac.isEmpty() || displayMac == "00:00:00:00:00:00" || displayMac == "FF:FF:FF:FF:FF:FF") continue;

        const QString key = QString::number(type) + "_" + displayMac;
        QStandardItem* item = itemByKey.value(key, nullptr);
        if(item)
        {
            // 갱신 (display::updateInfo 가드와 동일: 유효값만 덮어씀)
            if(!displayEssid.isEmpty()) item->setData(displayEssid, dev::EssidRole);
            if(pwr != 0 && pwr != 999)  item->setData(pwr, dev::PwrRole);
            if(ch > 0)                  item->setData(ch, dev::ChRole);
            item->setData(now, dev::LastSeenRole);
            item->setData(false, dev::FadedRole);
        }
        else
        {
            item = new QStandardItem();
            item->setData(displayMac,   dev::MacRole);
            item->setData(displayEssid, dev::EssidRole);
            item->setData(pwr,          dev::PwrRole);
            item->setData(ch,           dev::ChRole);
            item->setData((int)type,    dev::TypeRole);
            item->setData(false,        dev::FadedRole);
            item->setData(now,          dev::LastSeenRole);
            item->setData(key,          dev::KeyRole);
            devModel->appendRow(item);
            itemByKey.insert(key, item);
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
    QModelIndex idx = ui->listView->indexAt(pos);
    if(!idx.isValid()) return;

    int targetCh = idx.data(dev::ChRole).toInt();
    QString bssid = idx.data(dev::MacRole).toString();
    QString essid = idx.data(dev::EssidRole).toString();
    uint8_t itemType = idx.data(dev::TypeRole).toInt();

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

    QAction *selectedAction = menu.exec(ui->listView->viewport()->mapToGlobal(pos));
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
#ifdef Q_OS_MAC
        const bool canAttack = isRunning;            // 더미 모드: 데몬 없이 허용
#else
        const bool canAttack = (daemonProcess && daemonProcess->state() == QProcess::Running);
#endif
        if(canAttack)
        {
            QStringList stationList;
            stationList << "Broadcast (FF:FF:FF:FF:FF:FF)";

            for(int r=0; r<devModel->rowCount(); r++)
            {
                QStandardItem* stItem = devModel->item(r);
                if(stItem && stItem->data(dev::TypeRole).toInt() == 1)
                {
                    stationList << stItem->data(dev::MacRole).toString();
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
        stopAttack();
    }

}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if(watched == ui->listView->viewport() && event->type() == QEvent::Gesture)
    {
        QGestureEvent *gestureEvent = static_cast<QGestureEvent*>(event);
        if(QGesture *gesture = gestureEvent->gesture(Qt::TapAndHoldGesture))
        {
            QTapAndHoldGesture *tapAndHold = static_cast<QTapAndHoldGesture*>(gesture);
            if(tapAndHold->state() == Qt::GestureFinished)
            {
                QPoint globalPos = tapAndHold->position().toPoint();
                QPoint viewportPos = ui->listView->viewport()->mapFromGlobal(globalPos);
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

    // 프로그램적 호출 시 버튼 체크 상태 동기화 (사용자 클릭은 자동)
    if(index == 0) ui->btnAP->setChecked(true);
    else           ui->btnStation->setChecked(true);

    if(devProxy) devProxy->setTypeFilter(index);
    updateEmptyState();
}

void MainWindow::ageDevices()
{
    if(!devModel) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 FADE_MS = 15000;   // 15초 미관측 -> 흐림
    const qint64 REMOVE_MS = 45000; // 45초 미관측 -> 제거

    for(int row = devModel->rowCount()-1; row >= 0; --row)
    {
        QStandardItem* it = devModel->item(row);
        if(!it) continue;
        const qint64 age = now - it->data(dev::LastSeenRole).toLongLong();
        if(age > REMOVE_MS)
        {
            itemByKey.remove(it->data(dev::KeyRole).toString());
            devModel->removeRow(row);   // QStandardItem 삭제됨
        }
        else
        {
            const bool faded = age > FADE_MS;
            if(it->data(dev::FadedRole).toBool() != faded)
                it->setData(faded, dev::FadedRole);
        }
    }
    updateEmptyState();
}

void MainWindow::updateEmptyState()
{
    const bool empty = (devProxy == nullptr) || (devProxy->rowCount() == 0);
    ui->emptyLabel->setVisible(empty);
    ui->listView->setVisible(!empty);
}

void MainWindow::stopAttack()
{
    if(daemonProcess && daemonProcess->state() == QProcess::Running)
    {
        ST_IPC_CMD cmd;
        memset(&cmd, 0, sizeof(ST_IPC_CMD));
        cmd.action = Act::SNIFFING;
        strncpy(cmd.interface, devType.c_str(), 15);
        daemonProcess->write((const char*)&cmd, sizeof(ST_IPC_CMD));

        if(timer && !timer->isActive()) timer->start(500); // 채널 홉핑 재개
    }

    attacking = false;
    if(attackTimer) attackTimer->stop();
    markAttackTarget(QString());        // 배지 해제
    ui->attackBanner->setVisible(false);
    ui->statusbar->showMessage("Attack Stopped", 3000);

#ifdef Q_OS_MAC
    macDebug->setAttack(false, 0, QString(), 0);
#endif
}

void MainWindow::updateAttackBanner()
{
    if(!attacking)
    {
        ui->attackBanner->setVisible(false);
        return;
    }
    const qint64 elapsed = (QDateTime::currentMSecsSinceEpoch() - attackStartMs) / 1000;
    const QString dot = (elapsed % 2 == 0) ? "#ff5c5c" : "#7a3b3b"; // 라이브 깜빡임
    ui->attackLabel->setText(QString(
        "<span style='color:%1; font-weight:700;'>●</span>"
        "&nbsp;<span style='color:#ff8a8a; font-weight:700;'>공격 중</span>"
        "&nbsp;&nbsp;<span style='color:#cfcfcf;'>%2</span>"
        "&nbsp;<span style='color:#7a7a7a;'>→</span>&nbsp;<b style='color:#ffffff;'>%3</b>"
        "&nbsp;&nbsp;<span style='color:#a98a8a;'>경과 %4초</span>")
        .arg(dot, attackTypeName, attackTargetMac).arg(elapsed));
    ui->attackBanner->setVisible(true);
}

void MainWindow::markAttackTarget(const QString& key)
{
    if(!devModel) return;
    // 기존 배지 모두 해제
    for(int row = 0; row < devModel->rowCount(); ++row)
    {
        QStandardItem* it = devModel->item(row);
        if(it && it->data(dev::AttackingRole).toBool())
            it->setData(false, dev::AttackingRole);
    }
    // 대상 배지 설정
    if(!key.isEmpty())
    {
        QStandardItem* it = itemByKey.value(key, nullptr);
        if(it) it->setData(true, dev::AttackingRole);
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

    // 공격 상태 상시 표시 시작
    attacking = true;
    attackStartMs = QDateTime::currentMSecsSinceEpoch();
    attackTypeName = (attackType == 1) ? "Deauth" : (attackType == 2) ? "Auth" : "CSA";
    attackTargetMac = pureMac;
    markAttackTarget("0_" + attackTargetBssid);  // 대상 AP 행 배지
    updateAttackBanner();
    attackTimer->start(1000);

#ifdef Q_OS_MAC
    // 더미 모드: 가상 공격 효과 반영 (Deauth=STA 끊김, CSA=채널 변경)
    macDebug->setAttack(true, attackType, attackTargetBssid, (attackType == 3) ? chToMove : 0);
#endif
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
