#include "runner.h"
#include "suseongtrace.h"
#include "mac.h"
#include "radiotap.h"
#include "wireless.h"

Runner::Runner()
{
    isRunning = false;
    pcap = nullptr;
}
Runner::~Runner()
{
    stop();
}

void Runner::run(const std::string& dev)
{
    isRunning = true;
    device = dev;

    pcap = pcap_open_live(device.c_str(), BUFSIZ, 1, 100, errbuf);
    if(pcap == nullptr)
    {
        std::string errMsg = "pcap_open_live failed: ";
        errMsg += errbuf;
        TRACE(errMsg);
        return;
    }

    struct pcap_pkthdr* header;
    const u_char* packet;
    int res;

    while(isRunning==true)
    {
        res = pcap_next_ex(pcap, &header, &packet);
        if(res == 0) continue; // 타임아웃
        if(res == -1 || res == -2) break; // error, breakloop

        ST_RDT rdt = capRdt(packet);
        ST_WL wl = capWl(packet+rdt.len);
        uint64_t wirelessLen = sizeof(wl);

        // 비콘 체크
        if (!chkBeacon(wl)) continue;

        ST_INFO info;
        info.BSSID = prtMac(wl.bssid);

        ST_BC_COMMON bc = capBc(packet + rdt.len + wirelessLen);
        uint64_t bcLen = sizeof(bc);

        const u_char* tagStart = (packet + rdt.len + wirelessLen + bcLen);
        info.ESSID = getEssid(tagStart, (header->caplen) - rdt.len - wirelessLen - bcLen);

        std::map<std::string, int> rdtInfo = getRdtInfo(packet, &rdt, presentCount(packet));
        if(rdtInfo["PWR"] != 999) info.PWR = std::to_string(rdtInfo["PWR"]);
        if(rdtInfo["CH"] != 0) info.CH = std::to_string(rdtInfo["CH"]);

        /* TEST
        char logBuf[512];
        snprintf(logBuf, sizeof(logBuf), "BSSID: %s\tPWR: %s\tCH: %s\tESSID: %s",
                 info.BSSID.c_str(), info.PWR.c_str(), info.CH.c_str(), info.ESSID.c_str());
        TRACE(std::string(logBuf));
        */

        printf("BSSID: %s\tPWR: %s\tCH: %s\tESSID: %s\n",
               info.BSSID.c_str(), info.PWR.c_str(), info.CH.c_str(), info.ESSID.c_str());

        fflush(stdout);
    }

    pcap_close(pcap);
    pcap = nullptr;


    // 파싱 로직 구현

}

void Runner::stop()
{
    isRunning = false;
    if(pcap != nullptr)
    {
        pcap_breakloop(pcap);
    }
}
