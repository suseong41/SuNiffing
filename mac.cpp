#include "mac.h"

void prtMac(char* dest, uint64_t size, ST_MAC mac)
{
    snprintf(dest, size, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac.mac[0], mac.mac[1], mac.mac[2],
             mac.mac[3], mac.mac[4], mac.mac[5]);
}
