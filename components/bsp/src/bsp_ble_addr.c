#include "bsp_ble_addr.h"

int bsp_ble_addr_cmp(const uint8_t a[6], const uint8_t b[6])
{
    for (int i = 5; i >= 0; i--) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

bool bsp_ble_addr_should_initiate(const uint8_t own[6], const uint8_t peer[6])
{
    return bsp_ble_addr_cmp(own, peer) > 0;
}
