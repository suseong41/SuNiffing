#pragma once
#include <QObject>
#include <QTimer>
#include <QByteArray>
#include <QString>
#include <functional>

// macOS 테스트용 더미 데이터 주입기.
// 실제 인터페이스/데몬 없이 ST_IPC_EVENT 를 주기적으로 만들어
// onEvents 콜백으로 흘려보내 UI(리스트/정렬/에이징/배너)를 검증한다.
class MacDebug : public QObject
{
public:
    explicit MacDebug(QObject* parent = nullptr);

    // 더미 이벤트 수신 콜백 (signal 대신 직접 콜백 — 직렬화된 ST_IPC_EVENT 묶음)
    std::function<void(const QByteArray&)> onEvents;

    void start();   // 더미 주입 시작
    void stop();    // 중지

    // 가상 공격 상태 반영 (type: 1=Deauth, 2=Auth, 3=CSA)
    //  - Deauth: 대상 AP 의 STATION 들이 끊겨 사라짐
    //  - CSA   : 대상 AP 의 채널이 csaCh 로 바뀜
    void setAttack(bool active, int type, const QString& targetApMac, int csaCh = 0);

private:
    void tick();

    QTimer m_timer;
    int m_tick = 0;

    bool m_attacking = false;
    int m_attackType = 0;
    QString m_targetAp;   // 대문자 MAC 문자열 (prtMac 포맷)
    int m_csaCh = 0;
};
