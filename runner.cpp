#include "runner.h"
#include "suseongtrace.h"

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

    pcap = pcap_open_live(device.c_str(), BUFSIZ, 1, 1, errbuf);
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
        if(res == 0) continue;
        if(res == -1 || res == -2) break;

        ST_INFO info;
        fwrite(&info, sizeof(ST_INFO), 1, stdout);
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
