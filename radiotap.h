#pragma once
#include <pcap/pcap.h>
#include <stdio.h>
#include <string>
#include <map>

#pragma pack(push, 1)
struct ST_RDT
{
    uint8_t version;
    uint8_t padding;
    uint16_t len;
    uint32_t present;
};

struct ST_CH
{
    uint16_t freq;
    uint16_t flags;
};

#pragma pack(pop)

ST_RDT capRdt(const u_char* packet);
int presentCount(const u_char* packet);
std::map<std::string, int> getRdtInfo(const u_char* packet, ST_RDT *rdt, int presentCount);
