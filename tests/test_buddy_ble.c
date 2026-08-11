#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "buddy_ble.h"

static void test_nus_uuid_bytes_match_standard_values(void)
{
    static const uint8_t service_uuid[] = {BUDDY_NUS_SERVICE_UUID_BYTES};
    static const uint8_t rx_uuid[] = {BUDDY_NUS_RX_UUID_BYTES};
    static const uint8_t tx_uuid[] = {BUDDY_NUS_TX_UUID_BYTES};

    assert(BUDDY_NUS_SERVICE_UUID16_FIRST == 0x0001);
    assert(BUDDY_NUS_RX_UUID16_FIRST == 0x0002);
    assert(BUDDY_NUS_TX_UUID16_FIRST == 0x0003);
    assert(sizeof(service_uuid) == 16);
    assert(sizeof(rx_uuid) == 16);
    assert(sizeof(tx_uuid) == 16);
    assert(service_uuid[12] == 0x01 && rx_uuid[12] == 0x02 && tx_uuid[12] == 0x03);
    assert(service_uuid[15] == 0x6e && rx_uuid[15] == 0x6e && tx_uuid[15] == 0x6e);
}

static void test_tx_fragment_size_reserves_att_overhead_and_clamps(void)
{
    assert(buddy_ble_tx_fragment_size(0) == 0);
    assert(buddy_ble_tx_fragment_size(3) == 0);
    assert(buddy_ble_tx_fragment_size(23) == 20);
    assert(buddy_ble_tx_fragment_size(517) == BUDDY_BLE_TX_CHUNK_MAX);
}

static void test_secure_link_requires_mitm_bond_and_full_sc_key(void)
{
    assert(buddy_ble_link_is_secure(true, true, true, 16));
    assert(!buddy_ble_link_is_secure(false, true, true, 16));
    assert(!buddy_ble_link_is_secure(true, false, true, 16));
    assert(!buddy_ble_link_is_secure(true, true, false, 16));
    assert(!buddy_ble_link_is_secure(true, true, true, 15));
}

int main(void)
{
    test_nus_uuid_bytes_match_standard_values();
    test_tx_fragment_size_reserves_att_overhead_and_clamps();
    test_secure_link_requires_mitm_bond_and_full_sc_key();
    return 0;
}
