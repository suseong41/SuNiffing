#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , isRunning(false)
{
    ui->setupUi(this);
    this->setWindowTitle("SuNiffing");


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

void MainWindow::onStartButton() {}
void MainWindow::onReceivePacket() {}
void MainWindow::onDaemonOutput(){}
void MainWindow::onDaemonError() {}

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

    QString targetPath = targetDir + "/ssdaemon";
    QFile targetFile(targetPath);

    if(targetFile.exists())
    {
        // 기존 파일 발견시 제거
        targetFile.remove();
    }
    // assets에서 추출해옴
    QFile assetFile("assets:/ssdaemon");

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
