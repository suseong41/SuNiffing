#include "channel.h"
#include <QProcess>
#include <QStringList>

// 전체 후보
const QList<int> Channel::candidates = 
{
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    36, 40, 44, 48,
    52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
    149, 153, 157, 161, 165,
    169, 173, 177
};

// probe 실패 시 폴백
const QList<int> Channel::defaultHops = 
{
    1, 6, 11, 36, 149, 40, 44, 48, 153, 157, 161, 165,
    2, 7, 12, 3, 8, 13, 4, 9, 5, 10,
    52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144
};

bool Channel::inBand(int ch, Band band)
{
    if(band == Band::Dual) return true;
    if(band == Band::Only24) return ch <= 14;
    return ch >= 36; // Only5
}

QList<int> Channel::filterByBand(const QList<int>& in, Band band)
{
    if(band == Band::Dual) return in;
    QList<int> out;
    for(int ch : in) if(inBand(ch, band)) out.append(ch);
    return out;
}

void Channel::probe(const QString& dev, Band band)
{
    Q_UNUSED(dev);
    
    // 선택 대역으로 후보를 걸러 프로브할 목록 구성 (candidates = 정적 전체 후보)
    const QList<int> probeList = filterByBand(candidates, band);

    QString list;
    for(int ch : probeList) list += QString::number(ch) + " ";

    // 각 채널을 돌며 입력 채널과 실제 변경 채널 매칭 여부 검증
    QString script =
        "for ch in " + list + "; do "
        "nexutil -k$ch >/dev/null 2>&1; "
        "rb=$(nexutil -k 2>&1 | sed 's/.*, //; s|/.*||'); "
        "[ \"$rb\" = \"$ch\" ] && printf '%s ' \"$ch\"; "
        "done";

    QProcess p;
    p.start("su", QStringList() << "-c" << script);
    if(!p.waitForFinished(8000))
    {
        m_hopSeq = filterByBand(defaultHops, band);
        m_hopIdx = 0;
        return;
    }

    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    QList<int> supported;
    const QStringList chTokens = out.split(' ', Qt::SkipEmptyParts);
    for(const QString& t : chTokens)
    {
        bool ok = false;
        int c = t.toInt(&ok);
        if(ok && c > 0 && !supported.contains(c)) supported.append(c);
    }

    if(supported.isEmpty())
    {
        m_hopSeq = filterByBand(defaultHops, band);
        m_hopIdx = 0;
        return;
    }

    static const QList<int> prio = {1, 6, 11, 36, 149};
    QList<int> ordered;
    for(int c : prio) if(supported.contains(c)) ordered.append(c);
    for(int c : supported) if(!ordered.contains(c)) ordered.append(c);

    m_hopSeq = ordered;
    m_hopIdx = 0;
}

// 스캔 홉핑
int Channel::next()
{
    if(m_hopSeq.isEmpty()) m_hopSeq = defaultHops;
    int ch = m_hopSeq[m_hopIdx];
    QString cmd = QString("nexutil -k%1; nexutil -s0x613 -i -v2").arg(ch);
    QProcess::startDetached("su", QStringList() << "-c" << cmd);
    m_hopIdx = (m_hopIdx + 1) % m_hopSeq.size();
    return ch;
}

// 공격용 채널 셋팅
void Channel::setChannel(int ch)
{
    QString cmd = QString("nexutil -k%1; nexutil -s0x613 -i -v2").arg(ch);
    QProcess::startDetached("su", QStringList() << "-c" << cmd);
}
