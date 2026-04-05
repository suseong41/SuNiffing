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
    uint16_t timestamp;
    uint16_t interval;
    uint16_t capacity;
};

#pragma pack(pop)

ST_WL capWl(const u_char* packet);
bool chkBeacon(ST_WL target);

ST_BC_COMMON capBc(const u_char* packet);
std::string getEssid(const u_char* pacekt, const int beaconLen);
