#include "radiotap.h"

ST_RDT capRdt(const u_char* packet)
{
    ST_RDT *pHdr = (ST_RDT*)packet;
    return *pHdr;
}

static bool hasNextPresent(uint32_t present)
{
    if ((present & 0x80000000) != 0)
    {
        return true;
    }
    return false;
}

static int channelFromMhz(uint16_t freq)
{
    // 2.4GHz: ch = (freq-2407)/5 (1~13), 14는 2484 특수
    if (freq == 2484) return 14;
    if (freq >= 2412 && freq <= 2472) return (freq - 2407) / 5;
    // 5GHz: ch = (freq-5000)/5 (36~165 등). 5GHz 비콘엔 DS Param(태그3) 이 없어
    //        라디오탭 주파수가 유일한 채널 소스라 이 매핑이 꼭 필요.
    if (freq >= 5160 && freq <= 5885) return (freq - 5000) / 5;
    return 0;
}

int presentCount(const u_char* packet)
{
    int count = 0;
    uint32_t* presentPtr = (uint32_t*)(packet+4);
    while(true)
    {
        count++;
        if (hasNextPresent(*presentPtr))
        {
            presentPtr++;
        }
        else break;
    }
    return count;
}

// PWR은 present에서 5| Antena Signal 부분.
ST_RDT_DATA getRdtInfo(const u_char* packet, const ST_RDT *rdt, int presentCount)
{
    ST_RDT_DATA data;
    uint32_t present = rdt->present;
    int offset = 4 + 4*presentCount;

    if ((present & (1<<0)) != 0) // 0. TSFT
    {
        while ( (offset % 8) != 0 ) offset++;
        offset += 8;
    }
    if ((present & (1<<1)) != 0) // 1. FLAGS
    {
        offset += 1;
    }
    if ((present & (1<<2)) != 0) // 2. Rate
    {
        offset += 1;
    }
    if ((present & (1<<3)) != 0) // 3. Channel
    {
        while ( (offset % 2) != 0 ) offset++;
        uint16_t freq = *(uint16_t*)(packet + offset);
        data.ch = channelFromMhz(freq); // 0 is None
        offset += 4;
    }
    if ((present & (1<<4)) != 0) // 4. FHSS
    {
        while ( (offset % 2) != 0 ) offset++;
        offset += 2;
    }
    if ((present & (1<<5)) != 0) // 5. Antenna Signal
    {
        data.pwr = (int8_t)packet[offset]; // 999 is None
    }

    return data;
}

bool hasFcs(const u_char* packet, const ST_RDT *rdt, int presentCount)
{
    uint32_t present = rdt->present;
    int offset = 4 + 4*presentCount;
    if((present & (1<<0)) != 0)
    {
        while((offset % 8) != 0) offset++;
        offset += 8;
    }
    if((present & (1<<1)) != 0)
    {
        uint8_t flags = packet[offset];
        if((flags & 0x10) != 0) return true;
    }
    return false;
}


