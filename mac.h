#pragma once
#include <pcap/pcap.h>
#include <stdio.h>
#include <string>

struct ST_MAC
{
    uint8_t mac[6];
};

std::string prtMac(ST_MAC mac);
