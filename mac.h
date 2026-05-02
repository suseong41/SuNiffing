#pragma once
#include <pcap/pcap.h>
#include <stdio.h>
#include <string>

struct ST_MAC
{
    uint8_t mac[6];
};

void prtMac(char* dest, uint64_t size, ST_MAC mac);
