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

    if(essid.isEmpty() || essid.startsWith(("<length:")))
    {
        ui->lEssid->setText(essid);
        ui->lEssid->setStyleSheet("color: gray; font-weight: bold; font-size: 16pt;");
    }
    else
    {
        ui->lEssid->setText(essid);
        ui->lEssid->setStyleSheet("color: black; font-weight: bold; font-size: 16pt;");
    }
}
void display::updateInfo(const QString& essid, const QString& pwr, const QString& ch)
{
    if(!essid.isEmpty() || essid != ui->lEssid->text())
    {
        ui->lEssid->setText(essid);
        if(essid.startsWith("<length:"))
        {
            ui->lEssid->setStyleSheet("color: gray; font-weight: bold; font-size: 16pt;");
        }
        else
        {
            ui->lEssid->setStyleSheet("color: black; font-weight: bold; font-size: 16pt;");
        }
    }
    if(!pwr.isEmpty() && pwr != "0" && pwr != "999")
    {
        ui->lPwr->setText(pwr);
    }
    if(!ch.isEmpty() && ch != "0")
    {
        ui->lCh->setText("CH "+ch);
    }
}
