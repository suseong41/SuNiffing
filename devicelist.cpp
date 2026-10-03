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
    // dBm은 파랑 계열로 세기만 표현(위험 팔레트 red/amber는 보안 배지 전용 -> 충돌/피로 방지)
    if (pwr >= -50) return QColor(0x5f, 0xb3, 0xff); // 강: 밝은 파랑
    if (pwr >= -70) return QColor(0x4a, 0x86, 0xc7); // 중: 중간 파랑
    return QColor(0x6c, 0x7a, 0x88);                 // 약: 흐린 청회색
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
        macFont.setPointSizeF(option.font.pointSizeF() * 0.72);
    } else if (option.font.pixelSize() > 0)
    {
        macFont.setPixelSize(int(option.font.pixelSize() * 0.72));
    }
    painter->setFont(macFont);
    // 보안 배지 폰트: option.font 기준 비율(MAC 축소와 독립). MAC(0.82)보다 살짝 작게.
    QFont secFont = option.font;
    if (secFont.pointSizeF() > 0)     secFont.setPointSizeF(secFont.pointSizeF() * 0.62);
    else if (secFont.pixelSize() > 0) secFont.setPixelSize(int(secFont.pixelSize() * 0.62));

    // 하단 라인: 왼쪽 MAC / 오른쪽 보안 배지. secW만큼 MAC 폭을 줄여 겹침 방지.
    const QString sec = index.data(dev::SecurityRole).toString();
    const int secW = (type == 0 && !sec.isEmpty())
                   ? QFontMetrics(secFont).horizontalAdvance(sec) + 8 : 0;
    QRect macRect(leftRect.x(), leftRect.center().y(),
                  leftRect.width() - secW, leftRect.height()/2);
    painter->setPen(subColor);
    painter->drawText(macRect, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(mac, Qt::ElideRight, macRect.width()));

    // 보안 배지 (색 = 취약도): 취약=빨강, PMF없음=앰버, 견고=초록
    if (secW > 0)
    {
        const bool weakSec = index.data(dev::WeakRole).toBool();
        const int  pmf     = index.data(dev::PmfRole).toInt();
        const QColor secColor = weakSec    ? QColor(0xc6, 0x28, 0x28)
                              : (pmf == 0) ? QColor(0xf9, 0xa8, 0x25)
                              :              QColor(0x2e, 0x7d, 0x32);
        painter->setFont(secFont);
        painter->setPen(selected ? option.palette.highlightedText().color() : secColor);
        painter->drawText(QRect(leftRect.x(), leftRect.center().y(), leftRect.width(), leftRect.height()/2),
                          Qt::AlignVCenter | Qt::AlignRight, sec);
    }

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
