#include "./mainwindow.h"
#include "./device.h"
#include "./ui_mainwindow.h"
#include "./ipc_proto.h"
#include <QFrame>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , isRunning(false)
{
    ui->setupUi(this);
    this->setWindowTitle("SuNiffing");

    // export
    qputenv("PATH", qgetenv("PATH") + ":/data/local/tmp");

    connect(ui->scanButton, &QPushButton::clicked, this, [this]{
        const bool starting = !isRunning;
        ui->scanButton->setText(starting ? "■ Stop" : "▶ Start");
        ui->scanButton->setProperty("running", starting);
        ui->scanButton->style()->unpolish(ui->scanButton);
        ui->scanButton->style()->polish(ui->scanButton);
        ui->scanButton->repaint();

        if(starting) onStartButton();
        else         onStopButton();
        updateScanButton();
    });
    updateScanButton();

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

    // 장치 리스트
    devModel = new QStandardItemModel(this);
    devProxy = new DeviceProxy(this);
    devProxy->setSourceModel(devModel);
    devProxy->setSortRole(dev::PwrRole);
    devProxy->setDynamicSortFilter(true);
    devProxy->sort(0, Qt::DescendingOrder);

    ui->listView->setModel(devProxy);
    ui->listView->setItemDelegate(new DeviceDelegate(this));
    ui->listView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->listView->setResizeMode(QListView::Adjust);
    QScroller::grabGesture(ui->listView->viewport(), QScroller::LeftMouseButtonGesture);

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

    // 시간에 따라 정상 -> 흐림
    ageTimer = new QTimer(this);
    connect(ageTimer, &QTimer::timeout, this, &MainWindow::ageDevices);
    ageTimer->start(1000);

    // 공격 상태 배너 + 원클릭 중단
    ui->attackBanner->setAttribute(Qt::WA_StyledBackground, true);
    ui->attackBanner->setVisible(false);
    ui->attackLabel->setWordWrap(true);
    ui->attackLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    connect(ui->attackStopButton, &QPushButton::clicked, this, &MainWindow::stopAttack);
    attackTimer = new QTimer(this);
    connect(attackTimer, &QTimer::timeout, this, &MainWindow::updateAttackBanner);

    // AP | STATION 전환
    ui->segToggle->setAttribute(Qt::WA_StyledBackground, true);
    QButtonGroup* viewGroup = new QButtonGroup(this);
    viewGroup->setExclusive(true);
    viewGroup->addButton(ui->btnAP, 0);
    viewGroup->addButton(ui->btnStation, 1);
    connect(viewGroup, &QButtonGroup::idClicked, this, &MainWindow::onViewToggleChange);
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
        QString libPath = dropNexmonLib();
        QString pre;
        if(libPath != "")
        {
            pre = QString("cp %1 /data/local/tmp/libnexmon.so; chmod 644 /data/local/tmp/libnexmon.so; ").arg(libPath);
        }

        QString cmd = pre + QString("LD_PRELOAD=/data/local/tmp/libnexmon.so %1 %2")
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
    // 앱: 밴드 선택 팝업 -> (선택 시) 모니터 모드 진입 + 데몬 실행
    if(isRunning) return;

    QString dev = ui->devIn->currentText().trimmed();
    if(dev.isEmpty())
    {
        ui->statusbar->showMessage("인터페이스가 선택되지 않았습니다.", 4000);
        ui->devIn->setFocus();
        return;
    }
    devType = dev.toStdString();
    ui->devIn->setEnabled(false);

    bandDialog = new QDialog(this);
    bandDialog->setObjectName("bandDialog");
    bandDialog->setAttribute(Qt::WA_DeleteOnClose);
    bandDialog->setWindowTitle("스캔 대역 선택");
    bandDialog->setMinimumWidth(340);

    // 배경과 구분되도록 내용을 살짝 밝은 카드(QFrame)에 담음
    QVBoxLayout *outer = new QVBoxLayout(bandDialog);
    outer->setContentsMargins(12, 12, 12, 12);
    QFrame *bandCard = new QFrame(bandDialog);
    bandCard->setObjectName("bandCard");
    outer->addWidget(bandCard);

    QVBoxLayout *layout = new QVBoxLayout(bandCard);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(4);
    QLabel *bandTitle = new QLabel("캡처할 주파수 대역을 선택하세요.", bandCard);
    bandTitle->setObjectName("bandTitle");
    layout->addWidget(bandTitle);

    QButtonGroup *bandGroup = new QButtonGroup(bandDialog);

    auto addBandRow = [&](const QString& title, const QString& desc, int id) -> QRadioButton* {
        QRadioButton *rb = new QRadioButton(title, bandDialog);
        rb->setMinimumHeight(34);
        bandGroup->addButton(rb, id);
        layout->addWidget(rb);

        QLabel *sub = new QLabel(desc, bandDialog);
        sub->setStyleSheet("color:#8a8a8a; font-size:11px; margin-left:24px; margin-bottom:6px;");
        sub->setWordWrap(true);
        layout->addWidget(sub);
        return rb;
    };

    addBandRow("2.4 GHz", "채널 1~13 · 스윕이 가장 빠름", (int)Channel::Band::Only24);
    addBandRow("5 GHz", "채널 36~165 · 5GHz AP 전용", (int)Channel::Band::Only5);
    QRadioButton *rbDual = addBandRow("Dual (2.4 + 5 GHz)", "전체 대역 · 기본값", (int)Channel::Band::Dual);
    rbDual->setChecked(true);
    selectedBand = Channel::Band::Dual;

    connect(bandGroup, &QButtonGroup::idClicked, this, [this](int id){
        selectedBand = (Channel::Band)id;
    });

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 12, 0, 0);
    btnRow->setSpacing(10);
    QPushButton *cancelBtn = new QPushButton("취소", bandDialog);
    QPushButton *okBtn = new QPushButton("선택", bandDialog);
    okBtn->setDefault(true);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, bandDialog, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, bandDialog, &QDialog::accept);
    connect(bandDialog, &QDialog::accepted, this, &MainWindow::onBandDialogAccepted);
    connect(bandDialog, &QDialog::rejected, this, [this]{
        ui->devIn->setEnabled(true);
        updateScanButton(); // isRunning 여전히 false -> Start로 복귀
    });

    bandDialog->open();
}

void MainWindow::onBandDialogAccepted()
{
    QString dev = QString::fromStdString(devType);

    ui->scanButton->setText("■ Stop");
    ui->scanButton->setProperty("running", true);
    ui->scanButton->style()->unpolish(ui->scanButton);
    ui->scanButton->style()->polish(ui->scanButton);
    ui->scanButton->repaint();

    // nexutil을 /data/local/tmp로 배포(매번 갱신)해 self-contained. c1은 드라이버가 뻣음 -> k1 사용
    QString nexutilPath = dropNexutil();
    QString pre;
    if(!nexutilPath.isEmpty())
        pre = QString("cp %1 /data/local/tmp/nexutil; chmod 755 /data/local/tmp/nexutil; ").arg(nexutilPath);

    QString cmd = pre + QString("svc wifi disable; "
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

    // 지원 채널 프로브(모니터 진입 후, 선택 대역만) -> hopSeq 구성. 실패 시 기본 목록 폴백.
    hopper.probe(dev, selectedBand);
    hopper.reset();

    // 채널 홉핑
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::nextChannel);
    timer->start(500); // 0.5s

    runDaemon();
    isRunning = true;
    updateScanButton();
}

void MainWindow::onStopButton()
{
    if(!isRunning) return;
    isRunning = false;

    if(attacking)
    {
        attacking = false;
        if(attackTimer) attackTimer->stop();
        markAttackTarget(QString());
        ui->attackBanner->setVisible(false);
    }

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

    QString dev = ui->devIn->currentText();
    QString cmd = QString("nexutil -m0; svc wifi enable").arg(dev);

    QProcess p;
    p.start("su", QStringList() << "-c" << cmd);
    p.waitForFinished(5000);
    QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if(!err.isEmpty()) qDebug() << "[CLEANUP ERROR]" << err;

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
            ch = 0;
        }
        else continue;

        if(displayMac.isEmpty() || displayMac == "00:00:00:00:00:00" || displayMac == "FF:FF:FF:FF:FF:FF") continue;

        const QString key = QString::number(type) + "_" + displayMac;
        QStandardItem* item = itemByKey.value(key, nullptr);
        if(item)
        {
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
    int ch = hopper.next();
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
        if(daemonProcess && daemonProcess->state() == QProcess::Running)
        {
            QStringList stationList;
            stationList << "Broadcast (FF:FF:FF:FF:FF:FF)";

            for(int r=0; r < devModel->rowCount(); r++)
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

            QLabel *chLabel = new QLabel("Target Channel:", attackDialog);
            attackChSpin = new QSpinBox(attackDialog);
            attackChSpin->setRange(1, 177);
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

            QHBoxLayout *btnRow = new QHBoxLayout();
            QPushButton *cancelBtn = new QPushButton("취소", attackDialog);
            QPushButton *okBtn = new QPushButton("선택", attackDialog);
            btnRow->addWidget(cancelBtn);
            btnRow->addWidget(okBtn);
            layout->addLayout(btnRow);

            connect(cancelBtn, &QPushButton::clicked, attackDialog, &QDialog::reject);
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
    if(index == 0) ui->btnAP->setChecked(true);
    else           ui->btnStation->setChecked(true);

    if(devProxy) devProxy->setTypeFilter(index);
    updateEmptyState();
}

void MainWindow::ageDevices()
{
    if(!devModel) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 FADE_MS = 15000;   // 15초 -> 흐림
    const qint64 REMOVE_MS = 45000; // 45초 -> 제거

    for(int row = devModel->rowCount()-1; row >= 0; --row)
    {
        QStandardItem* it = devModel->item(row);
        if(!it) continue;
        const qint64 age = now - it->data(dev::LastSeenRole).toLongLong();
        if(REMOVE_MS < age)
        {
            itemByKey.remove(it->data(dev::KeyRole).toString());
            devModel->removeRow(row);
        }
        else
        {
            const bool faded = FADE_MS < age;
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

void MainWindow::updateScanButton()
{
    ui->scanButton->setText(isRunning ? "■ Stop" : "▶ Start");
    ui->scanButton->setProperty("running", isRunning);
    ui->scanButton->style()->unpolish(ui->scanButton);
    ui->scanButton->style()->polish(ui->scanButton);
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
    markAttackTarget(QString());
    ui->attackBanner->setVisible(false);
    ui->statusbar->showMessage("Attack Stopped", 3000);
}

void MainWindow::updateAttackBanner()
{
    if(!attacking)
    {
        ui->attackBanner->setVisible(false);
        return;
    }
    const qint64 elapsed = (QDateTime::currentMSecsSinceEpoch() - attackStartMs) / 1000;
    const QString dot = (elapsed % 2 == 0) ? "#c96a6a" : "#6e3a3a";
    ui->attackLabel->setText(QString(
        "<span style='color:%1; font-weight:700;'>●</span> "
        "<span style='color:#d38a8a; font-weight:700;'>공격 중</span>  "
        "<span style='color:#cfcfcf;'>%2</span> "
        "<span style='color:#7a7a7a;'>→</span> <b style='color:#ffffff;'>%3</b>  "
        "<span style='color:#a98a8a;'> %4초</span>")
        .arg(dot, attackTypeName, attackTargetMac).arg(elapsed));
    ui->attackBanner->setVisible(true);
}

void MainWindow::markAttackTarget(const QString& key)
{
    if(!devModel) return;
    for(int row = 0; row < devModel->rowCount(); ++row)
    {
        QStandardItem* it = devModel->item(row);
        if(it && it->data(dev::AttackingRole).toBool())
            it->setData(false, dev::AttackingRole);
    }
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
    // 2.4Ghz / 5Ghz 구분은 1xxx dxxx chanspec 대역으로 구분하는듯?
    int chToMove = attackChSpin->value();
    if(timer && timer->isActive()) timer->stop();
    hopper.setChannel(attackTargetCh);
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

    attacking = true;
    attackStartMs = QDateTime::currentMSecsSinceEpoch();
    attackTypeName = (attackType == 1) ? "Deauth" : (attackType == 2) ? "Auth" : "CSA";
    attackTargetMac = pureMac;
    markAttackTarget("0_" + attackTargetBssid);
    updateAttackBanner();
    attackTimer->start(1000);
}

static QString dropPcapDaemon()
{
    // AppDataLocation -> /data/data/<패키지명>/files
    QString targetDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(targetDir);

    if(!dir.exists())
    {
        dir.mkpath(".");
    }

    QString targetPath = targetDir + "/suseong";
    QFile targetFile(targetPath);

    if(targetFile.exists())
    {
        targetFile.remove();
    }
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

static QString dropNexmonLib()
{
    QString targetDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(targetDir);

    if(!dir.exists())
    {
        dir.mkpath(".");
    }

    QString targetPath = targetDir + "/libnexmon.so";
        QFile targetFile(targetPath);

    if(targetFile.exists())
    {
        targetFile.remove();
    }
    QFile assetFile("assets:/libnexmon.so");

    // chmod 644
    if(assetFile.copy(targetPath))
    {
        QFile::setPermissions(targetPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                        QFileDevice::ReadGroup | QFileDevice::ReadOther);
        return targetPath;
    }

    return QString("");
}

static QString dropNexutil()
{
    QString targetDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(targetDir);

    if(!dir.exists())
    {
        dir.mkpath(".");
    }

    QString targetPath = targetDir + "/nexutil";
    QFile targetFile(targetPath);

    if(targetFile.exists())
    {
        targetFile.remove();
    }
    QFile assetFile("assets:/nexutil");

    // chmod 755 (실행 파일)
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
