#include "mac.h"

std::string prtMac(ST_MAC mac)
{
    std::string prt = "";
    char buf[4];
    for (int i=0; i<sizeof(mac); i++)
    {
        sprintf(buf, "%02X", mac.mac[i]);
        prt += buf;

        if ( i != 5) prt += ":";
    }

    return prt;
}
