#include "security.h"
#include <cstdio>
#include <cstring>

/*
 * RSN (tag 48)
 * Version | 2B (LE)
 * Group Cipher | 4B
 * Pairwise Count | 2B (LE)
 * Pairwise Suite | 4B*Count
 * AKM Count | 2B (LE)
 * AKM Suite | 4B*Count
 * RSN Capabilities | 2B (LE)
 * ..
 */

// RSN/WPA -> 리틀엔디안
static uint16_t le16(const u_char* p)
{
    uint16_t lb = p[0];
    uint16_t hb = p[1] << 8;
    return lb | hb;
}

/*
 * capability - privacy 참조.
 * http://www.ktword.co.kr/test/view/view.php?m_temp1=2319&id=1293
 * WPA1 - WPA1은 IEEE 규격 전 완성 MS OUI[00:50:F2]0x01(wpa1)
 * IEEE[00:0F:AC], 0x02 -> TKIP, 0x04 -> CCMP-128(AES, WPA2), 0x09 -> GCMP-256(WPA3-192)
 * AKM[00:0F:AC], (1, 5) -> 802.1X, (2, 6) -> WPA2-Personal, SAE(+FT) -> WPA3, OWE -> Enhanced Open
 * redis 서버를 iptime공유기.
 */

ST_SECURITY getSecurity(const u_char* tagStart, int tagLen, uint16_t capability)
{
    ST_SECURITY out;
    memset(&out, 0, sizeof(out));

    const bool privacy = (capability & 0x0010) != 0; // Open 점검.

    const u_char* rsn = nullptr;
    int rsnLen = 0;
    const u_char* wpa = nullptr;
    int wpaLen = 0;

    for(int i=0; i+2<tagLen;)
    {
        uint8_t id = tagStart[i];
        uint8_t len = tagStart[i+1];
        if(tagLen < (i+2+(int)len)) break;
        const u_char* data = tagStart+i+2;

        if(id == 48) // RSN (WPA2/WPA3)
        {
            rsn = data;
            rsnLen = (int)len;
        }
        else if(id == 221 && 4 <= len && data[0]==0x00 && data[1]==0x50 && data[2]==0xf2 && data[3]==0x01) // WPA1
        {
            wpa = data+4;
            wpaLen = (int)len-4;
        }
        i += (int)len+2;
    }

    if(rsn)
    {
        bool tkip=false, ccmp=false, gcmp256=false; // pairwise cipher
        bool sae=false, psk=false, dot1x=false, owe=false; //AKM
        int p = 6; // version(2)+group cipher(4)

        if(p+2 <= rsnLen) // count(2)+suite(4)*count
        {
            int cnt = le16(rsn+p);
            p += 2;
            for (int k=0; k<cnt; k++)
            {
                if (rsnLen<p+4) break;
                switch(rsn[p+3])
                {
                case 2: tkip    = true; break;
                case 4: ccmp    = true; break;
                case 9: gcmp256 = true; break;
                }
                p += 4;
            }
        }

        if(p+2 <= rsnLen) // count(2)+suite(4)*count
        {
            int cnt = le16(rsn+p);
            p += 2;
            for (int k=0; k<cnt; k++)
            {
                if (rsnLen < p+4) break;
                switch (rsn[p+3])
                {
                case 1: case 5: dot1x   = true; break; // 802.1X (Enterprise)
                case 2: case 6: psk     = true; break; // PSK
                case 8: case 9: sae     = true; break; // SAE(WPA3)
                case 18:        owe     = true; break; // OWE
                }
                p += 4;
            }
        }

        // RSN Capabilites(2) -> PMF(802.11w)
        if (p+2 <= rsnLen)
        {
            uint16_t caps = le16(rsn+p);
            if (caps & 0x0040) out.pmf = 2;
            else if (caps & 0x0080) out.pmf = 1;
        }

        const char* proto;
        const char* akm;
        if (sae && psk) { proto = "WPA2/3"; akm = "SAE/PSK"; }
        else if (sae)   { proto = "WPA3"; akm = "SAE"; }
        else if (owe)   { proto = "OWE"; akm = ""; }
        else if (dot1x) { proto = "WPA2-Ent"; akm = "802.1X"; }
        else            { proto = "WPA2"; akm = "PSK"; }

        const char* cipher = "?";
        if (ccmp && tkip)
        {
            cipher = "CCMP/TKIP";
        } else if (gcmp256)
        {
            cipher = "GCMP256";
        } else if (ccmp)
        {
            cipher = "CCMP";
        } else if (tkip)
        {
            cipher = "TKIP";
        }

        if (akm[0]) snprintf(out.label, sizeof(out.label), "%s-%s-%s", proto, akm, cipher);
        else snprintf(out.label, sizeof(out.label), "%s-%s", proto, cipher);
        if (tkip) out.weak = 1;
    }
    else if (wpa)
    {
        bool tkip = false;
        bool ccmp = false;
        int p = 6; // version(2) + multicast cipher(4)
        if (p+2 <= wpaLen)
        {
            int cnt = le16(wpa+p);
            p += 2;
            for (int k=0; k<cnt; k++)
            {
                if (wpaLen < p+4) break;
                switch (wpa[p+3])
                {
                    case 2: tkip = true; break;
                    case 4: ccmp = true; break;
                }
                    p += 4;
            }
        }
        const char* cipher = "";
        if (ccmp && tkip)
        {
            cipher = "CCMP/TKIP";
        } else if (ccmp)
        {
            cipher = "CCMP";
        } else
        {
            cipher = "TKIP";
        }
        snprintf(out.label, sizeof(out.label), "WPA1-PSK-%s", cipher);
        out.weak = 1;
    }
    else if (privacy)
    {
        snprintf(out.label, sizeof(out.label), "WEP");
        out.weak = 1;
    }
    else
    {
        snprintf(out.label, sizeof(out.label), "OPEN");
    }

    return out;
}
