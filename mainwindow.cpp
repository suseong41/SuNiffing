#include "mainwindow.h"
#include "./device.h"
#include "./ui_mainwindow.h"
#include "./display.h"

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

void MainWindow::killDaemon()
{
    isRunning = false;
    if(daemonProcess != nullptr)
    {
        if(daemonProcess->state() == QProcess::Running)
        {
            daemonProcess->kill();
        }
    }

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

    QString cmd = QString("svc wifi disable; "
                          "ifconfig %1 down; "
                          "ifconfig %1 up; "
                          "nexutil -c1; "
                          "nexutil -d; "
                          "nexutil -k1; "
                          "nexutil -g0x613 -i -v2").arg(dev);

    qDebug() << "[EXEC] " << cmd;
    QProcess p;
    p.start("su", QStringList() << "-c" << cmd);
    p.waitForFinished(3000);

    QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
    QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();

    if(!out.isEmpty()) qDebug() << "[OUT]" << out;
    if(!err.isEmpty()) qDebug() << "[ERR]" << err;

    runDaemon();
    isRunning = true;
}

void MainWindow::onStopButton()
{
    if(!isRunning) return;
    killDaemon();
    // todo: interface down , up
    QString dev = ui->devIn->currentText();
    QString cmd = QString("su -c 'nexutil -m0 && svc wifi enable && ifconfig %1 down && ifconfig %1 up'").arg(dev);
    int res = std::system(cmd.toStdString().c_str());
    if(res != 0)
    {
        QMessageBox::warning(this, "ERROR", "WIFI 복구 실패");
    }

    isRunning = false;
    ui->devIn->setEnabled(true);

}
void MainWindow::onRender() {}

void MainWindow::onDaemonOutput()
{
    if(daemonProcess == nullptr) return;

    QByteArray output = daemonProcess->readAllStandardOutput();
    QList<QByteArray> lines = output.split('\n');
    static QRegularExpression re("BSSID:\\s*([0-9a-fA-F:]+)\\s*PWR:\\s*(-?\\d+)\\s*CH:\\s*(\\d+)\\s*ESSID:\\s*(.*)");

    for(const QByteArray& line : lines)
    {
        QString strLine = QString::fromUtf8(line.trimmed());
        if(strLine.isEmpty() || !strLine.contains("BSSID:")) continue;

        QRegularExpressionMatch match = re.match(strLine);
        if(match.hasMatch())
        {
            QString bssid = match.captured(1);
            QString pwr = match.captured(2);
            QString ch = match.captured(3);
            QString essid = match.captured(4);
            if(displayItem.contains(bssid))
            {
                QListWidgetItem* item = displayItem[bssid];
                display* row = qobject_cast<display*>(ui->listWidget->itemWidget(item));
                if(row)
                {
                    row->updateInfo(pwr, ch);
                }

            }
            else
            {
                QListWidgetItem* item = new QListWidgetItem(ui->listWidget);
                display* widget = new display(this);
                widget->setInfo(bssid, pwr, ch, essid);
                item->setSizeHint(widget->sizeHint());
                ui->listWidget->addItem(item);
                ui->listWidget->setItemWidget(item, widget);

                displayItem.insert(bssid, item);
            }
        }

    }
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
