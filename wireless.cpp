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

std::string getEssid(const u_char* packet, const int beaconLen)
{
    std::vector<std::vector<std::string>> info;
    const u_char* index = packet;
    const u_char* end = index + beaconLen;
    while(index+2 < end)
    {
        uint8_t tagId = index[0];
        uint8_t tagLen = index[1];
        const u_char* data = index + 2;

        if(tagId == 0)
        {
            if(tagLen == 0) return "<length: 0>";
            if(data[0] == 0 | data[1] == 0) return "<length: " + std::to_string(tagLen) + ">";
            return std::string((char*)data, tagLen);
        }
        /*
         * 암호화는 아직 미구현
        if(tagId == 48)
        {
            if(tagLen==0) return "";
        }
        */
        index += (2+tagLen);
    }
    return "";
}
