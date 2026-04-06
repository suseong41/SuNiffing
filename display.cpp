#include "display.h"
#include "ui_display.h"

display::display(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::display)
{
    ui->setupUi(this);
}

display::~display()
{
    delete ui;
}

void display::setInfo(const QString& bssid, const QString& pwr, const QString& ch, const QString& essid)
{
    ui->lBssid->setText(bssid);
    ui->lPwr->setText(pwr);
    ui->lCh->setText("CH" + ch);

    if(essid.isEmpty() || essid.startsWith("<length:"))
    {
        ui->lEssid->setText("Hidden Network");
        ui->lEssid->setStyleSheet("color: gray; font-weight: bold; font-size: 16pt;");
    }
    else
    {
        ui->lEssid->setText(essid);
        ui->lEssid->setStyleSheet("color: black; font-weight: bold; font-size: 16pt;");
    }
}
void display::updateInfo(const QString& pwr, const QString& ch)
{
    ui->lPwr->setText(pwr);
    ui->lCh->setText("CH "+ch);
}
