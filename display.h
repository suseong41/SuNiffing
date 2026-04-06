#ifndef DISPLAY_H
#define DISPLAY_H

#include <QWidget>

namespace Ui {
class display;
}

class display : public QWidget
{
    Q_OBJECT

public:
    explicit display(QWidget *parent = nullptr);
    ~display();

    void setInfo(const QString& bssid, const QString& pwr, const QString& ch, const QString& essid);
    void updateInfo(const QString& pwr, const QString& ch);

private:
    Ui::display *ui;
};

#endif // DISPLAY_H
