#pragma once
#include <stdio.h>
#include <string>
#include <pcap/pcap.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include "ipc_proto.h"

#pragma pack(push, 1)
struct ST_INFO
{
    char bssid[18]; // 00:00:00:00:00\0
    char essid[33];
    int16_t pwr;
    int16_t ch;
};
#pragma pack(pop)

class Runner
{
public:
    Runner();
    ~Runner();

    void RXloop(const std::string& dev);
    void stop();
private:
    void TXloop();
    std::thread TXthread;
    std::mutex cmdMutex;
    std::mutex outMutex;
    ST_IPC_CMD currentCmd;

    std::string device;
    std::atomic<bool> isRunning;
    pcap_t* pcap;
    char errbuf[PCAP_ERRBUF_SIZE];

};
