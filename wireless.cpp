#include "wireless.h"

ST_WL capWl(const u_char* packet)
{
    ST_WL *pHdr = (ST_WL*)packet;
    return *pHdr;
}

ST_BC_COMMON capBc(const u_char* packet)
{
    ST_BC_COMMON *pHdr = (ST_BC_COMMON*)packet;
    return *pHdr;
}

bool chkBeacon(ST_WL target)
{
    if((target.frameControl & 0x00FF) == 0x80) return true;
    return false;
}

bool chkProbeReq(ST_WL target)
{
    if((target.frameControl & 0x00FF) == 0x40) return true;
    return false;
}

// AP | STATION 구별 함수
WLTYPE chkWlType(const ST_WL* wl)
{
    uint16_t fc = wl->frameControl & 0x00FF;

    if(fc == 0x80 || fc == 0x50)
    {
        return WLTYPE::AP;
    }

    if(fc == 0x40 || fc == 0x08)
    {
        return WLTYPE::STATION;
    }

    return WLTYPE::UNKNOWN;
}

bool getStationInfo(const ST_RDT* rdt, const ST_WL* wl, ST_STATION_INFO& info)
{
    memset(&info, 0, sizeof(ST_STATION_INFO));
    uint16_t fc = wl->frameControl & 0x00FF;

    if(fc == 0x40) // Probe Request
    {
        memset(&info.bssid, 0xFF, 6);
        info.st_mac = wl->sa;
    }
    else if(fc == 0x08) // Data Frame
    {
        uint16_t flags = wl->frameControl & 0x0300; // 0000 0011 0000 0000
        if(flags == 0x0100) // TO DS
        {
            info.bssid = wl->da;
            info.st_mac = wl->sa;
        }
        else if(flags == 0x0200)
        {
            info.bssid = wl->sa;
            info.st_mac = wl->da;
        }
        else return false;
    }
    else return false;

    return true;
}

// TAG 0이 없으면, 깨진 비콘 프레임이라 봄.
bool getEssid(char* dest, uint64_t destSize, const u_char* packet, const int beaconLen)
{
    if (destSize > 0) dest[0] = '\0';

    const u_char* index = packet;
    const u_char* end = index + beaconLen;

    while(index+2 <= end)
    {
        uint8_t tagId = index[0];
        uint8_t tagLen = index[1];
        if (index + 2 + tagLen > end) break;
        const u_char* data = index + 2;

        if(tagId == 0)
        {
            if(tagLen == 0)
            {
                snprintf(dest, destSize, "<length: 0>");
                return true;
            }

            bool isHidden = true;
            for(int i=0; i<tagLen; i++)
            {
                if(data[i] != 0x00)
                {
                    isHidden = false;
                    break;
                }
            }

            if(isHidden)
            {
                snprintf(dest, destSize, "<length: %d>", tagLen);
                return true;
            }

            uint64_t copyLen = (tagLen < destSize - 1) ? tagLen : (destSize - 1);
            memcpy(dest, data, copyLen);
            dest[copyLen] = '\0';
            return true;
        }

        /*
         * ds 파라미터에서 채널 번호 빼오기
         * 암호화는 아직 미구현
        if(tagId == 48)
        {
            if(tagLen==0) return "";
        }
        */
        index += (2+tagLen);
    }
    return false;
}

ST_DEAUTH_WL getApDeauth(ST_MAC ap_mac, ST_MAC st_mac)
{
    ST_DEAUTH_WL wl;
    wl.frameControl = 0x00c0;
    wl.duration_id = 0x013a;
    wl.da = ap_mac;
    wl.sa = st_mac;
    wl.bssid = ap_mac;
    wl.seqControl = 0xd204;
    wl.reasonCode = 0x0003;
    return wl; // station 떠남
}

ST_DEAUTH_WL getStDeauth(ST_MAC ap_mac, ST_MAC st_mac)
{
    ST_DEAUTH_WL wl;
    wl.frameControl = 0x00c0;
    wl.duration_id = 0x013a;
    wl.da = st_mac;
    wl.sa = ap_mac;
    wl.bssid = ap_mac;
    wl.seqControl = 0x800d;
    wl.reasonCode = 0x0003;
    return wl;
}

ST_AUTH_H getAuth_H(ST_MAC ap_mac, ST_MAC st_mac)
{
    ST_AUTH_H wl;
    wl.frameControl = 0x00b0;
    wl.duration_id = 0x013a;
    wl.da = ap_mac;
    wl.sa = st_mac;
    wl.bssid = ap_mac;
    wl.seqControl = 0x7b00;
    return wl;
}

ST_AUTH_B getAuth_B()
{
    ST_AUTH_B auth;
    auth.alg_num = 0x0000;
    auth.seq = 0x0001;
    auth.status = 0x0000;
    return auth;
}

ST_ACK getAck(ST_MAC ap_mac)
{
    ST_ACK ack;
    ack.frameControl = 0x00d4;
    ack.duration = 0x0000;
    ack.da = ap_mac;
    return ack;
}

int getInsertTagLoc(const u_char *beaconTagPacket, int tagTotalLen, int insertTagId)
{
    const u_char* index = beaconTagPacket;
    const u_char* end = index + tagTotalLen;
    int prev = -1;
    int currentLoc = 0;
    int insertLoc = 0;
    while (index+2 < end)
    {
        uint8_t tagId = index[0];
        uint8_t tagLen = index[1];
        int totalTagLen = (2+tagLen);
        if (end < index+2+tagLen) break;
        if (tagId < insertTagId && prev < tagId)
        {
            prev = tagId;
            insertLoc = currentLoc + totalTagLen;
        }
        index += totalTagLen;
        currentLoc += totalTagLen;
    }
    return insertLoc;
}

int getCh(const u_char* beaconTagPacket, int tagTotalLen)
{
    const u_char* index = beaconTagPacket;
    const u_char* end = index + tagTotalLen;

    while(index+2 <= end)
    {
        uint8_t tagId = index[0];
        uint8_t tagLen = index[1];
        int totalTagLen = 2 + tagLen;

        if (index + totalTagLen > end) break;

        if (tagId == 3 && tagLen == 1) {
            return index[2];
        }

        index += totalTagLen;
    }

    return 0;
}
