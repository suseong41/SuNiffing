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
