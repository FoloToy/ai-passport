#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "buddy_ble.h"
#include "buddy_ble_lifecycle.h"
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
    int controller_peers;
    int local_identity;
    int controller_failures;
    int identity_restore_failures;
} fake_store_t;

static int fake_remove_controller_peers(void *context)
{
    fake_store_t *store = context;

    if (store->controller_failures-- > 0) {
        return -20;
    }
    store->controller_peers = 0;
    return 0;
}

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

static int fake_restore_local_identity(void *context)
{
    fake_store_t *store = context;

    if (store->identity_restore_failures-- > 0) {
        return -21;
    }
    store->local_identity = 1;
    return 0;
}

static int fake_persistent_is_clean(void *context, bool *clean)
{
    fake_store_t *store = context;
    bool empty;
    int rc = fake_persistent_is_empty(context, &empty);

    if (rc != 0) {
        return rc;
    }
    *clean = empty && store->local_identity == 1;
    return 0;
}

static int fake_controller_is_clean(void *context, bool *clean)
{
    fake_store_t *store = context;

    *clean = store->controller_peers == 0 && store->local_identity == 1;
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
        .remove_controller_peers = fake_remove_controller_peers,
        .erase_persistent = fake_erase_persistent,
        .persistent_is_empty = fake_persistent_is_empty,
        .reload_volatile = fake_reload_volatile,
        .restore_local_identity = fake_restore_local_identity,
        .persistent_is_clean = fake_persistent_is_clean,
        .volatile_is_empty = fake_volatile_is_empty,
        .controller_is_clean = fake_controller_is_clean,
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

static void test_tx_generation_gate_rejects_a_reconnected_peer(void)
{
    assert(buddy_ble_tx_generation_matches(true, true, true, true, 7, 7, 7));
    assert(!buddy_ble_tx_generation_matches(true, true, true, true, 7, 7, 8));
    assert(!buddy_ble_tx_generation_matches(true, true, true, true, 7, 8, 8));
    assert(!buddy_ble_tx_generation_matches(false, true, true, true, 7, 7, 7));
    assert(!buddy_ble_tx_generation_matches(true, false, true, true, 7, 7, 7));
    assert(!buddy_ble_tx_generation_matches(true, true, false, true, 7, 7, 7));
    assert(!buddy_ble_tx_generation_matches(true, true, true, false, 7, 7, 7));
}

static void test_pending_stop_suspends_transport_until_the_result_is_known(void)
{
    assert(buddy_ble_transport_available(true, false));
    assert(!buddy_ble_transport_available(true, true));
    assert(!buddy_ble_transport_available(false, false));
}

static void test_asynchronous_stop_failure_recovers_the_committed_termination(void)
{
    assert(buddy_ble_termination_failure_matches(7, 0xffff, 7, 7, false));
    assert(!buddy_ble_termination_failure_matches(8, 0xffff, 7, 7, false));
    assert(buddy_ble_termination_failure_matches(9, 9, 7, 0xffff, false));
    assert(buddy_ble_termination_failure_matches(7, 0xffff, 7, 0xffff, true));
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

static void test_stale_advertising_epoch_never_starts(void)
{
    const uint32_t snapshot = 7;

    assert(buddy_ble_adv_epoch_allows_start(snapshot, snapshot, true, true, false, false));
    /* Simulated delete-bonds request between snapshot and GAP start. */
    assert(!buddy_ble_adv_epoch_allows_start(snapshot, snapshot + 1, true, true, true, false));
    assert(!buddy_ble_adv_epoch_allows_start(snapshot, snapshot + 1, true, true, false, false));
}

static void test_controller_and_identity_faults_keep_delete_unverified(void)
{
    fake_store_t store = {
        .persistent_records = 4,
        .volatile_records = 4,
        .controller_peers = 2,
        .local_identity = 1,
        .controller_failures = 1,
    };
    buddy_ble_store_ops_t ops = fake_store_ops(&store);

    assert(buddy_ble_store_clear_verified(&ops) == -20);
    assert(store.persistent_records == 4);
    assert(store.controller_peers == 2);

    store.identity_restore_failures = 1;
    assert(buddy_ble_store_clear_verified(&ops) == -21);
    assert(store.persistent_records == 0);
    assert(buddy_ble_store_clear_verified(&ops) == 0);
    assert(store.controller_peers == 0);
    assert(store.local_identity == 1);
}

typedef struct {
    int fail_step;
    int step;
    int log[16];
    size_t count;
    int host_deinit_failure;
} fake_lifecycle_t;

static int lifecycle_init_step(void *context)
{
    fake_lifecycle_t *fake = context;
    int step = ++fake->step;

    fake->log[fake->count++] = step;
    return step == fake->fail_step ? -step : 0;
}

static void lifecycle_cleanup(void *context)
{
    fake_lifecycle_t *fake = context;

    fake->log[fake->count++] = 10 + fake->step--;
}

static int lifecycle_host_cleanup(void *context)
{
    fake_lifecycle_t *fake = context;

    fake->log[fake->count++] = 10 + fake->step--;
    return fake->host_deinit_failure;
}

static void lifecycle_clear(void *context)
{
    fake_lifecycle_t *fake = context;

    fake->log[fake->count++] = 99;
    fake->step = 0;
}

static buddy_ble_lifecycle_ops_t fake_lifecycle_ops(fake_lifecycle_t *fake)
{
    const buddy_ble_lifecycle_ops_t ops = {
        .context = fake,
        .host_init = lifecycle_init_step,
        .events_init = lifecycle_init_step,
        .bond_callout_init = lifecycle_init_step,
        .adv_callout_init = lifecycle_init_step,
        .gatt_init = lifecycle_init_step,
        .adv_callout_deinit = lifecycle_cleanup,
        .bond_callout_deinit = lifecycle_cleanup,
        .events_deinit = lifecycle_cleanup,
        .host_deinit = lifecycle_host_cleanup,
        .clear = lifecycle_clear,
    };
    return ops;
}

static void test_init_failure_rolls_back_and_can_retry(void)
{
    fake_lifecycle_t fake = {.fail_step = 4};
    fake_lifecycle_t gatt_fake = {.fail_step = 5};
    bool retry_safe;
    buddy_ble_lifecycle_ops_t ops = fake_lifecycle_ops(&fake);
    buddy_ble_lifecycle_ops_t gatt_ops = fake_lifecycle_ops(&gatt_fake);

    /* The second callout failed: only the first callout and host unwind. */
    assert(buddy_ble_lifecycle_init(&ops, &retry_safe) == -4);
    assert(retry_safe);
    assert(fake.count == 8);
    assert(fake.log[4] == 14 && fake.log[5] == 13 && fake.log[6] == 12 &&
           fake.log[7] == 99);

    fake.fail_step = 0;
    fake.count = 0;
    assert(buddy_ble_lifecycle_init(&ops, &retry_safe) == 0);
    assert(retry_safe);
    assert(fake.count == 5);

    /* A later GATT failure unwinds both callouts before the host. */
    assert(buddy_ble_lifecycle_init(&gatt_ops, &retry_safe) == -5);
    assert(retry_safe);
    assert(gatt_fake.count == 10);
    assert(gatt_fake.log[5] == 15 && gatt_fake.log[6] == 14 &&
           gatt_fake.log[7] == 13 && gatt_fake.log[8] == 12 &&
           gatt_fake.log[9] == 99);
}

static void test_host_deinit_failure_blocks_reinitialization(void)
{
    fake_lifecycle_t fake = {.fail_step = 5, .host_deinit_failure = -30};
    buddy_ble_lifecycle_ops_t ops = fake_lifecycle_ops(&fake);
    bool retry_safe = true;

    assert(buddy_ble_lifecycle_init(&ops, &retry_safe) == -30);
    assert(!retry_safe);
    assert(fake.log[fake.count - 1U] == 12); /* No clear after failed deinit. */
    assert(!buddy_ble_lifecycle_retry_allowed(false, !retry_safe));
}

static void test_host_init_failure_blocks_reinitialization(void)
{
    fake_lifecycle_t fake = {.fail_step = 1};
    buddy_ble_lifecycle_ops_t ops = fake_lifecycle_ops(&fake);
    bool retry_safe = true;

    assert(buddy_ble_lifecycle_init(&ops, &retry_safe) == -1);
    assert(!retry_safe);
    assert(fake.count == 1); /* Unknown partial init state is not cleared. */
    assert(!buddy_ble_lifecycle_retry_allowed(false, !retry_safe));
}

static void test_exhausted_delete_retry_preserves_transaction_snapshot(void)
{
    unsigned int snapshot_peer_count = 3;
    uint32_t snapshot_irk_tag = 0x12345678U;
    unsigned int controller_stale_peers = 1;
    uint32_t controller_irk_tag = 0x87654321U;
    const bool pending = true;
    const bool final_reported = true;

    assert(!buddy_ble_delete_request_is_new(pending, final_reported));
    assert(buddy_ble_delete_request_resets_attempts(pending, final_reported));
    if (buddy_ble_delete_request_is_new(pending, final_reported)) {
        snapshot_peer_count = 0;
        snapshot_irk_tag = 0;
    }
    assert(snapshot_peer_count == 3);
    assert(snapshot_irk_tag == 0x12345678U);
    /* The preserved evidence still detects both stale controller state and
     * an identity mismatch after retry exhaustion. */
    assert(controller_stale_peers != 0 || controller_irk_tag != snapshot_irk_tag);
    controller_stale_peers = 0;
    controller_irk_tag = snapshot_irk_tag;
    assert(controller_stale_peers == 0 && controller_irk_tag == snapshot_irk_tag);

    assert(buddy_ble_delete_request_is_new(false, false));
    assert(!buddy_ble_delete_request_is_new(true, false));
    assert(buddy_ble_delete_request_resets_attempts(false, false));
    assert(!buddy_ble_delete_request_resets_attempts(true, false));
}

int main(void)
{
    test_nus_uuid_bytes_match_standard_values();
    test_tx_fragment_size_reserves_att_overhead_and_clamps();
    test_secure_link_requires_mitm_bond_and_full_sc_key();
    test_tx_generation_gate_rejects_a_reconnected_peer();
    test_pending_stop_suspends_transport_until_the_result_is_known();
    test_asynchronous_stop_failure_recovers_the_committed_termination();
    test_commit_failure_preserves_retry_truth_across_restart();
    test_persistent_verification_failure_never_clears_volatile_store();
    test_silent_partial_erase_is_detected_before_volatile_reload();
    test_reload_failure_requires_a_verified_retry();
    test_persistent_store_is_reverified_after_reload();
    test_delete_retry_backoff_is_bounded();
    test_only_encrypted_cccd_gets_encrypted_read_policy();
    test_advertising_requires_open_desired_state();
    test_stale_advertising_epoch_never_starts();
    test_controller_and_identity_faults_keep_delete_unverified();
    test_init_failure_rolls_back_and_can_retry();
    test_host_deinit_failure_blocks_reinitialization();
    test_host_init_failure_blocks_reinitialization();
    test_exhausted_delete_retry_preserves_transaction_snapshot();
    return 0;
}
