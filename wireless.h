#pragma once
#include <pcap/pcap.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "mac.h"

#pragma pack(push, 1)

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

struct ST_DEAUTH_PACKET
{
    uint8_t rdt_hdr[12] = {0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // dummy
    ST_DEAUTH_WL wl;
};

#pragma pack(pop)

ST_WL capWl(const u_char* packet);
bool chkBeacon(ST_WL target);

ST_BC_COMMON capBc(const u_char* packet);
bool getEssid(char* dest, uint64_t destSize, const u_char* packet, const int beaconLen);

// DEAUTH
ST_DEAUTH_WL getApDeauth(ST_MAC ap_mac, ST_MAC st_mac);
ST_DEAUTH_WL getStDeauth(ST_MAC ap_mac, ST_MAC st_mac);
ST_AUTH_H getAuth_H(ST_MAC ap_mac, ST_MAC st_mac);
ST_AUTH_B getAuth_B();
ST_ACK getAck(ST_MAC ap_mac);
