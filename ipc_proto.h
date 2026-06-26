#pragma once
#include "mac.h"
#include <pcap/pcap.h>

#pragma pack(push, 1)

enum class Act : uint8_t
{
    IDLE = 0,
    SNIFFING = 1,
    DEAUTH = 2,
    CSA = 3,
    STOP = 4,
    AUTH = 5,
    DUMMY = 6
};

struct ST_IPC_CMD
{
    Act action;         // 1. Sniffing  |  2. Deauth  |  3. CSA  |  4. STOP  | 5. dummy
    char interface[16];     // wlan0
    ST_MAC target_ap;
    ST_MAC target_st;
    int32_t channel;
};

struct ST_IPC_EVENT
{
    uint8_t type;
    ST_MAC bssid;
    ST_MAC st_mac;
    char essid[33];
    int16_t pwr;
    int16_t ch;
    char message[64];
};

#pragma pack(pop)
