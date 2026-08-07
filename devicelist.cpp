#include "devicelist.h"
#include <QPainter>

bool DeviceProxy::filterAcceptsRow(int row, const QModelIndex& parent) const
{
    const QModelIndex idx = sourceModel()->index(row, 0, parent);

    if (idx.data(dev::TypeRole).toInt() != m_type) return false;

    if (!m_search.isEmpty())
    {
        const QString essid = idx.data(dev::EssidRole).toString();
        const QString mac   = idx.data(dev::MacRole).toString();
        if (!essid.contains(m_search, Qt::CaseInsensitive) &&
            !mac.contains(m_search, Qt::CaseInsensitive))
            return false;
    }
    return true;
}

// ---- DeviceDelegate ----
static QColor pwrColor(int pwr)
{
    if (pwr >= -50) return QColor(0x2e, 0x7d, 0x32); // green
    if (pwr >= -70) return QColor(0xf9, 0xa8, 0x25); // amber
    return QColor(0xc6, 0x28, 0x28);                 // red
}

QSize DeviceDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(index);
    return QSize(option.rect.width(), 60);
}

void DeviceDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();

    const bool selected  = option.state & QStyle::State_Selected;
    const bool faded     = index.data(dev::FadedRole).toBool();
    const bool attacking = index.data(dev::AttackingRole).toBool();
    const int  type      = index.data(dev::TypeRole).toInt();

    QString essid = index.data(dev::EssidRole).toString();
    QString mac   = index.data(dev::MacRole).toString();
    const int pwr = index.data(dev::PwrRole).toInt();
    const int ch  = index.data(dev::ChRole).toInt();

    if (type == 1) mac = "FROM: " + mac;

    if (selected)
        painter->fillRect(option.rect, option.palette.highlight());
    else if (index.row() % 2)
        painter->fillRect(option.rect, option.palette.alternateBase());
    if (attacking && !selected)
        painter->fillRect(option.rect, QColor(0x8f, 0x3a, 0x3a, 80));

    if (faded) painter->setOpacity(0.45);

    const QColor textColor = selected ? option.palette.highlightedText().color()
                                      : option.palette.text().color();
    const QColor subColor  = selected ? option.palette.highlightedText().color()
                                      : option.palette.color(QPalette::Disabled, QPalette::Text);

    const QRect r = option.rect.adjusted(12, 6, -12, -6);

    QFont essidFont = option.font;
    essidFont.setBold(true);
    if (option.font.pointSizeF() > 0)
        essidFont.setPointSizeF(option.font.pointSizeF() + 2.0);
    else if (option.font.pixelSize() > 0)
        essidFont.setPixelSize(option.font.pixelSize() + 2);
    painter->setFont(essidFont);
    const bool hiddenSsid = essid.startsWith("<length:") || essid.isEmpty();
    painter->setPen(hiddenSsid && !selected ? subColor : textColor);
    const int rightReserve = 90; // PWR/CH
    QRect leftRect = r.adjusted(0, 0, -rightReserve, 0);
    painter->drawText(QRect(leftRect.x(), leftRect.y(), leftRect.width(), leftRect.height()/2),
                      Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(essid, Qt::ElideRight, leftRect.width()));

    QFont macFont = option.font;
    if (option.font.pointSizeF() > 0)
    {
        macFont.setPointSizeF(option.font.pointSizeF() - 1.0);
    } else if (option.font.pixelSize() > 0)
    {
        macFont.setPixelSize(option.font.pixelSize() - 1);
    }
    painter->setFont(macFont);
    painter->setPen(subColor);
    painter->drawText(QRect(leftRect.x(), leftRect.center().y(), leftRect.width(), leftRect.height()/2),
                      Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(mac, Qt::ElideRight, leftRect.width()));

    // 우측: PWR/CH
    QRect rightRect(r.right() - rightReserve, r.y(), rightReserve, r.height());
    if (pwr != 0 && pwr != 999)
    {
        QFont pwrFont = option.font;
        pwrFont.setBold(true);
        painter->setFont(pwrFont);
        painter->setPen(selected ? option.palette.highlightedText().color() : pwrColor(pwr));
        painter->drawText(QRect(rightRect.x(), rightRect.y(), rightRect.width(), rightRect.height()/2), Qt::AlignVCenter | Qt::AlignRight, QString("%1 dBm").arg(pwr));
    }
    if (ch > 0)
    {
        painter->setFont(macFont);
        painter->setPen(subColor);
        painter->drawText(QRect(rightRect.x(), rightRect.center().y(), rightRect.width(), rightRect.height()/2), Qt::AlignVCenter | Qt::AlignRight, QString("CH %1").arg(ch));
    }
    if (attacking)
    {
        painter->setOpacity(1.0);
        painter->fillRect(QRect(option.rect.left(), option.rect.top(), 4, option.rect.height()), QColor(0x8f, 0x3a, 0x3a));
    }

    painter->restore();
}
