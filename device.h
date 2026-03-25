#pragma once
#include <pcap/pcap.h>
#include <string>
#include <vector>

class Device
{
public:
    static Device& getInstance();
    std::vector<std::string> getDevice();
private:
    Device();
    ~Device();
    Device(const Device& device) = delete;
    Device& operator=(const Device&) = delete;
};
