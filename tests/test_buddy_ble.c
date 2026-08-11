#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "buddy_ble.h"
#include "buddy_ble_store.h"

typedef struct {
    int persistent_records;
    int volatile_records;
    int commit_failures;
    int verify_failures;
    int verify_calls;
    int fail_verify_call;
    int reload_failures;
    int reloads;
    bool erase_noop;
} fake_store_t;

static int fake_erase_persistent(void *context)
{
    fake_store_t *store = context;

    if (store->commit_failures > 0) {
        --store->commit_failures;
        return -10;
    }
    if (!store->erase_noop) {
        store->persistent_records = 0;
    }
    return 0;
}

static int fake_persistent_is_empty(void *context, bool *empty)
{
    fake_store_t *store = context;

    ++store->verify_calls;
    if (store->verify_calls == store->fail_verify_call) {
        return -13;
    }
    if (store->verify_failures > 0) {
        --store->verify_failures;
        return -11;
    }
    *empty = store->persistent_records == 0;
    return 0;
}

static int fake_reload_volatile(void *context)
{
    fake_store_t *store = context;

    ++store->reloads;
    if (store->reload_failures > 0) {
        --store->reload_failures;
        return -12;
    }
    store->volatile_records = store->persistent_records;
    return 0;
}

static int fake_volatile_is_empty(void *context, bool *empty)
{
    fake_store_t *store = context;

    *empty = store->volatile_records == 0;
    return 0;
}

static buddy_ble_store_ops_t fake_store_ops(fake_store_t *store)
{
    const buddy_ble_store_ops_t ops = {
        .context = store,
        .erase_persistent = fake_erase_persistent,
        .persistent_is_empty = fake_persistent_is_empty,
        .reload_volatile = fake_reload_volatile,
        .volatile_is_empty = fake_volatile_is_empty,
    };

    return ops;
}

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

static void test_commit_failure_preserves_retry_truth_across_restart(void)
{
    fake_store_t store = {
        .persistent_records = 6,
        .volatile_records = 6,
        .commit_failures = 1,
    };
    buddy_ble_store_ops_t ops = fake_store_ops(&store);

    assert(buddy_ble_store_clear_verified(&ops) == -10);
    assert(store.persistent_records == 6);
    assert(store.volatile_records == 6);
    store.volatile_records = store.persistent_records; /* Simulated reboot reload. */
    assert(store.volatile_records == 6);

    assert(buddy_ble_store_clear_verified(&ops) == 0);
    assert(store.persistent_records == 0);
    assert(store.volatile_records == 0);
    assert(store.reloads == 1);
}

static void test_persistent_verification_failure_never_clears_volatile_store(void)
{
    fake_store_t store = {
        .persistent_records = 4,
        .volatile_records = 4,
        .verify_failures = 1,
    };
    buddy_ble_store_ops_t ops = fake_store_ops(&store);

    assert(buddy_ble_store_clear_verified(&ops) == -11);
    assert(store.volatile_records == 4);
    assert(store.reloads == 0);
}

static void test_silent_partial_erase_is_detected_before_volatile_reload(void)
{
    fake_store_t store = {
        .persistent_records = 6, /* OUR_SEC, PEER_SEC, CCCD, CSFC, RPA and local IRK. */
        .volatile_records = 6,
        .erase_noop = true,
    };
    buddy_ble_store_ops_t ops = fake_store_ops(&store);

    assert(buddy_ble_store_clear_verified(&ops) != 0);
    assert(store.persistent_records == 6);
    assert(store.volatile_records == 6);
    assert(store.reloads == 0);

    /* A reboot still reloads all six records; retry truth was not destroyed. */
    store.volatile_records = store.persistent_records;
    store.erase_noop = false;
    assert(buddy_ble_store_clear_verified(&ops) == 0);
    assert(store.persistent_records == 0);
    assert(store.volatile_records == 0);
}

static void test_reload_failure_requires_a_verified_retry(void)
{
    fake_store_t store = {
        .persistent_records = 6,
        .volatile_records = 6,
        .reload_failures = 1,
    };
    buddy_ble_store_ops_t ops = fake_store_ops(&store);

    assert(buddy_ble_store_clear_verified(&ops) == -12);
    assert(store.persistent_records == 0);
    assert(store.volatile_records == 6);
    assert(buddy_ble_store_clear_verified(&ops) == 0);
    assert(store.volatile_records == 0);
    assert(store.reloads == 2);
}

static void test_persistent_store_is_reverified_after_reload(void)
{
    fake_store_t store = {
        .persistent_records = 6,
        .volatile_records = 6,
        .fail_verify_call = 2,
    };
    buddy_ble_store_ops_t ops = fake_store_ops(&store);

    assert(buddy_ble_store_clear_verified(&ops) == -13);
    assert(store.persistent_records == 0);
    assert(store.volatile_records == 0);
    assert(store.verify_calls == 2);
}

static void test_delete_retry_backoff_is_bounded(void)
{
    assert(buddy_ble_retry_delay_ms(0) == 250);
    assert(buddy_ble_retry_delay_ms(1) == 500);
    assert(buddy_ble_retry_delay_ms(2) == 1000);
    assert(buddy_ble_retry_delay_ms(3) == 2000);
    assert(buddy_ble_retry_delay_ms(4) == 4000);
    assert(buddy_ble_retry_delay_ms(30) == 4000);
    assert(BUDDY_BLE_DELETE_MAX_ATTEMPTS == 5);
}

static void test_only_encrypted_cccd_gets_encrypted_read_policy(void)
{
    assert(buddy_ble_should_protect_cccd_read(0x2902, true));
    assert(!buddy_ble_should_protect_cccd_read(0x2902, false));
    assert(!buddy_ble_should_protect_cccd_read(0x2901, true));
}

static void test_advertising_requires_open_desired_state(void)
{
    assert(buddy_ble_should_advertise(true, true, false, false));
    assert(!buddy_ble_should_advertise(false, true, false, false));
    assert(!buddy_ble_should_advertise(true, false, false, false));
    assert(!buddy_ble_should_advertise(true, true, true, false));
    assert(!buddy_ble_should_advertise(true, true, false, true));
}

int main(void)
{
    test_nus_uuid_bytes_match_standard_values();
    test_tx_fragment_size_reserves_att_overhead_and_clamps();
    test_secure_link_requires_mitm_bond_and_full_sc_key();
    test_commit_failure_preserves_retry_truth_across_restart();
    test_persistent_verification_failure_never_clears_volatile_store();
    test_silent_partial_erase_is_detected_before_volatile_reload();
    test_reload_failure_requires_a_verified_retry();
    test_persistent_store_is_reverified_after_reload();
    test_delete_retry_backoff_is_bounded();
    test_only_encrypted_cccd_gets_encrypted_read_policy();
    test_advertising_requires_open_desired_state();
    return 0;
}
