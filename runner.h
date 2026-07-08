#pragma once
#include <stdio.h>
#include <string>
#include <pcap/pcap.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <poll.h>
#include <chrono>
#include <regex>
#include "ipc_proto.h"

class Runner
{
public:
    Runner();
    ~Runner();

    void RXloop(const std::string& dev);
    void stop();
private:
    std::string device;
    std::atomic<bool> isRunning;
    pcap_t* pcapRX;
    pcap_t* pcapTX;
    char errbuf[PCAP_ERRBUF_SIZE];

    void TXloop();
    std::thread TXthread;
    std::mutex cmdMutex;
    std::mutex outMutex;
    ST_IPC_CMD currentCmd;
};
