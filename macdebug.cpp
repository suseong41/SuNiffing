#include "macdebug.h"
#include "ipc_proto.h"
#include <QRandomGenerator>
#include <cstring>

MacDebug::MacDebug(QObject* parent) : QObject(parent)
{
    m_timer.setInterval(1500);
    connect(&m_timer, &QTimer::timeout, this, [this]{ tick(); });
}

void MacDebug::start()
{
    m_tick = 0;
    tick();          // 즉시 1회
    m_timer.start();
}

void MacDebug::stop()
{
    m_timer.stop();
    m_attacking = false;
}

void MacDebug::setAttack(bool active, int type, const QString& targetApMac, int csaCh)
{
    m_attacking  = active;
    m_attackType = type;
    m_targetAp   = targetApMac;
    m_csaCh      = csaCh;
    if (m_timer.isActive()) tick(); // 즉시 반영
}

// ---- helpers ----

static ST_MAC mkMac(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f)
{
    ST_MAC m;
    m.mac[0]=a; m.mac[1]=b; m.mac[2]=c; m.mac[3]=d; m.mac[4]=e; m.mac[5]=f;
    return m;
}

static int jitter(int base)
{
    int v = base + (int)QRandomGenerator::global()->bounded(-4, 5);
    if (v > -20) v = -20;
    if (v < -90) v = -90;
    return v;
}

static QByteArray mkEvent(uint8_t type, ST_MAC bssid, ST_MAC st,
                          const char* essid, int pwr, int ch)
{
    ST_IPC_EVENT e;
    memset(&e, 0, sizeof(e));
    e.type   = type;
    e.bssid  = bssid;
    e.st_mac = st;
    if (essid) strncpy(e.essid, essid, sizeof(e.essid) - 1);
    e.pwr = (int16_t)pwr;
    e.ch  = (int16_t)ch;
    return QByteArray(reinterpret_cast<const char*>(&e), sizeof(e));
}

static QString macStr(ST_MAC m)
{
    char b[18];
    prtMac(b, sizeof(b), m);
    return QString::fromUtf8(b);
}

void MacDebug::tick()
{
    const ST_MAC A1 = mkMac(0x02,0x11,0x22,0x00,0x00,0x01); // HomeWiFi
    const ST_MAC A2 = mkMac(0x02,0x11,0x22,0x00,0x00,0x02); // office_5G
    const ST_MAC A3 = mkMac(0x02,0x11,0x22,0x00,0x00,0x03); // hidden
    const ST_MAC A4 = mkMac(0x02,0x11,0x22,0x00,0x00,0x04); // CoffeeBean
    const ST_MAC A5 = mkMac(0x02,0x11,0x22,0x00,0x00,0x05); // Neighbor (전이: 에이징 데모)

    const ST_MAC BCAST = mkMac(0xFF,0xFF,0xFF,0xFF,0xFF,0xFF);
    const ST_MAC S1 = mkMac(0x06,0xAA,0x00,0x00,0x00,0x01);
    const ST_MAC S2 = mkMac(0x06,0xAA,0x00,0x00,0x00,0x02);
    const ST_MAC S3 = mkMac(0x06,0xAA,0x00,0x00,0x00,0x03);

    const ST_MAC NONE = mkMac(0,0,0,0,0,0);

    struct ApDef { ST_MAC mac; const char* essid; int pwr; int ch; bool transient; };
    const ApDef aps[] = {
        { A1, "HomeWiFi",    -45,  6, false },
        { A2, "office_5G",   -60, 11, false },
        { A3, "<length:0>",  -72,  1, false }, // 숨김 SSID
        { A4, "CoffeeBean",  -55,  9, false },
        { A5, "Neighbor_2G", -82,  3, true  }, // 전이 -> 에이징 데모
    };

    struct StaDef { ST_MAC st; ST_MAC ap; int pwr; bool assoc; };
    const StaDef stas[] = {
        { S1, A1,    -50, true  }, // To: A1
        { S2, BCAST, -65, false }, // (Not Associated)
        { S3, A2,    -58, true  }, // To: A2
    };

    QByteArray batch;

    // AP
    for (const ApDef& ap : aps)
    {
        if (ap.transient && m_tick > 4) continue; // 관측 중단 -> 흐림/제거
        int ch = ap.ch;
        // CSA 공격 효과: 대상 AP 채널이 지정 채널로 바뀐 것처럼
        if (m_attacking && m_attackType == 3 && m_csaCh > 0 && macStr(ap.mac) == m_targetAp)
            ch = m_csaCh;
        batch += mkEvent(0, ap.mac, NONE, ap.essid, jitter(ap.pwr), ch);
    }

    // STATION
    for (const StaDef& s : stas)
    {
        // Deauth 공격 효과: 대상 AP 에 연결된 STATION 은 끊겨 사라짐(미관측 -> 에이징)
        if (m_attacking && m_attackType == 1 && s.assoc && macStr(s.ap) == m_targetAp)
            continue;
        batch += mkEvent(1, s.ap, s.st, nullptr, jitter(s.pwr), 0);
    }

    if (onEvents) onEvents(batch);
    ++m_tick;
}
