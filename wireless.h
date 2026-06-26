#pragma once
#include <pcap/pcap.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "mac.h"
#include "radiotap.h"

enum class WLTYPE : uint8_t
{
    UNKNOWN = 0,
    AP,
    STATION
};

#pragma pack(push, 1)

// TODO: 프로브 요청 프레임 현재 존재하고있는 네트워크 영역을 찾기 위한(802.11 스캐닝) 목적의 프레임
// http://www.ktword.co.kr/test/view/view.php?m_temp1=2344&id=1293

struct ST_WL
{
    uint16_t frameControl;
    uint16_t duration_id;
    ST_MAC da;
    ST_MAC sa;
    ST_MAC bssid;
    uint16_t seqControl;
};

struct ST_BC_COMMON
{
    uint64_t timestamp;
    uint16_t interval;
    uint16_t capacity;
};

struct ST_DEAUTH_WL
{
    uint16_t frameControl;
    uint16_t duration_id;
    ST_MAC da;
    ST_MAC sa;
    ST_MAC bssid;
    uint16_t seqControl;
    uint16_t reasonCode;
};

struct ST_STATION_INFO
{
    ST_MAC bssid;
    ST_MAC st_mac;
    int8_t pwr;
    char probes[33];
};

struct ST_AUTH_H
{
    uint16_t frameControl;
    uint16_t duration_id;
    ST_MAC da;
    ST_MAC sa;
    ST_MAC bssid;
    uint16_t seqControl;
};

struct ST_AUTH_B
{
    uint16_t alg_num;
    uint16_t seq;
    uint16_t status;
};

struct ST_ACK // 제어프레임
{
    uint16_t frameControl;
    uint16_t duration;
    ST_MAC da;
};
// dummy -> {0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
struct ST_DEAUTH_PACKET
{
    uint8_t rdt_hdr[12];// dummy
    ST_DEAUTH_WL wl;
};

struct ST_AUTH_PACKET
{
    uint8_t rdt_hdr[12]; // dummy
    ST_AUTH_H wl;
    ST_AUTH_B auth;
};

struct ST_ACK_PACKET
{
    uint8_t rdt_hdr[12]; // dummy
    ST_ACK ack;
};

#pragma pack(pop)

ST_WL capWl(const u_char* packet);
bool chkBeacon(ST_WL target);
bool chkProbeReq(ST_WL target);
WLTYPE chkWlType(const ST_WL* wl);

ST_BC_COMMON capBc(const u_char* packet);
bool getEssid(char* dest, uint64_t destSize, const u_char* packet, const int beaconLen);
int getCh(const u_char* beaconTagPacket, int tagTotalLen);
bool getApInfo(const ST_RDT* rdt, const ST_WL* wl);
bool getStationInfo(const ST_RDT* rdt, const ST_WL* wl, ST_STATION_INFO& info);

// DEAUTH
ST_DEAUTH_WL getApDeauth(ST_MAC ap_mac, ST_MAC st_mac);
ST_DEAUTH_WL getStDeauth(ST_MAC ap_mac, ST_MAC st_mac);
ST_AUTH_H getAuth_H(ST_MAC ap_mac, ST_MAC st_mac);
ST_AUTH_B getAuth_B();
ST_ACK getAck(ST_MAC ap_mac);

// CSA Attack
int getInsertTagLoc(const u_char *beaconTagPacket, int tagTotalLen, int insertTagId);
