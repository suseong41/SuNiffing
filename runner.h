#pragma once
#include <stdio.h>
#include <string>
#include <pcap/pcap.h>

#pragma pack(push, 1)
struct ST_INFO
{
    // 통신 구조 작성
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
