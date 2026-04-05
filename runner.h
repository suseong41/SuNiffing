#pragma once
#include <stdio.h>
#include <string>
#include <pcap/pcap.h>

#pragma pack(push, 1)
struct ST_INFO
{
    std::string BSSID;
    std::string PWR;
    std::string BEACONS;
    std::string DATA;
    //std::string s;
    std::string CH;
    //std::string MB;
    std::string ENC;
    //std::string CIPHER;
    //std::string AUTH;
    std::string ESSID;
};
#pragma pack(pop)

class Runner
{
public:
    Runner();
    ~Runner();

    void run(const std::string& dev);
    void stop();
private:
    std::string device;
    bool isRunning;
    pcap_t* pcap;
    char errbuf[PCAP_ERRBUF_SIZE];

};
