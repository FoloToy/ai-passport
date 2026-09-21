// Host tests for the BLE address tie-break rule (components/bsp/src/bsp_ble_addr.c).
//
// The rule published in bsp_ble_link.h is "the larger address initiates". The
// comparison therefore has to follow the byte order a scanner shows, which is
// the opposite end of ble_addr_t.val from what memcmp() starts at. The cases
// below pin that down, including one pair where the two orders disagree.

#include "bsp_ble_addr.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Arrays are written in ble_addr_t.val order (least significant byte first), so
// {0x52, 0x6f, 0x32, 0xae, 0x11, 0x4c} is the address 4c:11:ae:32:6f:52.
static const uint8_t board_c4_6f50[6] = {0x52, 0x6f, 0x32, 0xae, 0x11, 0x4c};
static const uint8_t board_c4_b978[6] = {0x7a, 0xb9, 0x30, 0xae, 0x11, 0x4c};

// 00:00:00:00:00:05 and 01:00:00:00:00:05: they differ only in the most
// significant byte, so the printed order has a clear answer while memcmp()
// compares the equal low bytes first and reaches the opposite one.
static const uint8_t lsb_first_smaller[6] = {0x05, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t lsb_first_larger[6] = {0x05, 0x00, 0x00, 0x00, 0x00, 0x01};

static const uint8_t addr_a[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t addr_b[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static const uint8_t addr_c[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

static const uint8_t *const addresses[] = {
    board_c4_6f50, board_c4_b978, lsb_first_smaller, lsb_first_larger,
    addr_a,        addr_b,        addr_c,
};

static void test_recorded_boards(void)
{
    // The two boards from the device evidence. 4c:11:ae:32:6f:52 is larger than
    // 4c:11:ae:30:b9:7a, and it is the side the captured logs show connecting
    // out, so the rule and the measurement agree.
    assert(bsp_ble_addr_should_initiate(board_c4_6f50, board_c4_b978));
    assert(!bsp_ble_addr_should_initiate(board_c4_b978, board_c4_6f50));
}

static void test_printed_order_wins(void)
{
    // memcmp() would call the second address smaller; the printed order calls
    // it larger. The rule follows the printed order.
    assert(memcmp(lsb_first_smaller, lsb_first_larger, 6) < 0);
    assert(bsp_ble_addr_cmp(lsb_first_larger, lsb_first_smaller) > 0);
    assert(bsp_ble_addr_should_initiate(lsb_first_larger, lsb_first_smaller));
    assert(!bsp_ble_addr_should_initiate(lsb_first_smaller, lsb_first_larger));
}

static void test_comparison_contract(void)
{
    for (size_t i = 0; i < sizeof(addresses) / sizeof(addresses[0]); i++) {
        for (size_t j = 0; j < sizeof(addresses) / sizeof(addresses[0]); j++) {
            const int forward = bsp_ble_addr_cmp(addresses[i], addresses[j]);
            const int backward = bsp_ble_addr_cmp(addresses[j], addresses[i]);
            assert(forward == -backward);

            const bool i_initiates = bsp_ble_addr_should_initiate(addresses[i], addresses[j]);
            assert(i_initiates == (forward > 0));

            if (i == j) {
                // Two boards never share an address; the defined answer is that
                // neither side initiates.
                assert(forward == 0);
                assert(!i_initiates);
            } else {
                // Antisymmetric: exactly one side initiates, so a pair can never
                // connect out twice.
                assert(forward != 0);
                assert(i_initiates != bsp_ble_addr_should_initiate(addresses[j], addresses[i]));
            }
        }
    }
}

int main(void)
{
    test_recorded_boards();
    test_printed_order_wins();
    test_comparison_contract();
    return 0;
}
