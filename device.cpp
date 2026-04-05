#include "device.h"

Device::Device(){}
Device::~Device(){}

Device& Device::getInstance()
{
    static Device device;
    return device;
}

std::vector<std::string> Device::getDevice()
{
    std::vector<std::string> devList;
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t* alldevs;
    pcap_if_t* d;

    const int res = pcap_findalldevs(&alldevs, errbuf);
    if (res != 0)
    {
        // error
        return devList;
    }

    for (d = alldevs; d != NULL; d = d->next)
    {
        if(d->name != NULL)
        {
            devList.push_back(std::string(d->name));
        }
    }

    pcap_freealldevs(alldevs);
    return devList;
}
