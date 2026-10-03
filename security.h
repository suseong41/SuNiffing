#pragma once
#include <pcap/pcap.h>

struct ST_SECURITY
{
    char label[24];
    uint8_t pmf;
    uint8_t weak;
};

ST_SECURITY getSecurity(const u_char* tagStart, int tagLen, uint16_t capability);
