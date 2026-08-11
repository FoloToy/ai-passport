#include "buddy_ble.h"

#include <stdio.h>
#include <string.h>

#ifndef BUDDY_BLE_HOST_TEST
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_att.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_npl.h"
#include "nimble/nimble_npl_os.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "buddy_line.h"
#include "buddy_ble_store.h"
#include "buddy_ble_lifecycle.h"
#endif

size_t buddy_ble_tx_fragment_size(uint16_t mtu)
{
    size_t payload_size;

    if (mtu <= 3U) {
        return 0;
    }

    payload_size = (size_t)mtu - 3U;
    return payload_size < BUDDY_BLE_TX_CHUNK_MAX ? payload_size : BUDDY_BLE_TX_CHUNK_MAX;
}

bool buddy_ble_link_is_secure(bool encrypted, bool authenticated, bool bonded, uint8_t key_size)
{
    return encrypted && authenticated && bonded && key_size == 16U;
}

bool buddy_ble_should_protect_cccd_read(uint16_t uuid16, bool write_encrypted)
{
    return uuid16 == 0x2902U && write_encrypted;
}

bool buddy_ble_should_advertise(bool start_requested, bool host_synced,
                                bool delete_bonds_pending, bool has_physical_link)
{
    return start_requested && host_synced && !delete_bonds_pending && !has_physical_link;
}

uint32_t buddy_ble_retry_delay_ms(unsigned int attempt)
{
    uint32_t delay_ms = 250U;

    while (attempt > 0U && delay_ms < 4000U) {
        delay_ms *= 2U;
        --attempt;
    }
    return delay_ms > 4000U ? 4000U : delay_ms;
}

bool buddy_ble_adv_epoch_allows_start(uint32_t snapshot_epoch, uint32_t current_epoch,
                                      bool start_requested, bool host_synced,
                                      bool delete_bonds_pending, bool has_physical_link)
{
    return snapshot_epoch == current_epoch &&
           buddy_ble_should_advertise(start_requested, host_synced,
                                      delete_bonds_pending, has_physical_link);
}

bool buddy_ble_delete_request_is_new(bool delete_bonds_pending, bool final_reported)
{
    (void)final_reported;
    return !delete_bonds_pending;
}

bool buddy_ble_delete_request_resets_attempts(bool delete_bonds_pending, bool final_reported)
{
    return !delete_bonds_pending || final_reported;
}

#ifndef BUDDY_BLE_HOST_TEST

#define BUDDY_BLE_DEVICE_NAME_SIZE 14U

typedef struct {
    SemaphoreHandle_t mutex;
    buddy_ble_event_cb_t event_cb;
    void *event_context;
    buddy_line_buffer_t rx;
    uint32_t connection_generation;
    uint32_t advertising_epoch;
    uint16_t conn_handle;
    uint16_t rejecting_conn_handle;
    bool initialized;
    bool host_running;
    bool host_synced;
    bool start_requested;
    bool encrypted;
    bool secure;
    bool delete_bonds_pending;
    bool reset_scheduled;
    bool delete_final_reported;
    uint8_t delete_attempts;
    uint8_t adv_retry_attempts;
    int delete_bonds_result;
    int termination_failure_reason;
    ble_addr_t delete_peers[MYNEWT_VAL(BLE_STORE_MAX_BONDS) * 2U];
    struct ble_store_value_sec delete_peer_secs[MYNEWT_VAL(BLE_STORE_MAX_BONDS) * 2U];
    bool delete_peer_sec_present[MYNEWT_VAL(BLE_STORE_MAX_BONDS) * 2U];
    size_t delete_peer_count;
    struct ble_store_value_local_irk delete_local_irk;
    bool delete_snapshot_ready;
    bool init_rollback_blocked;
} buddy_ble_state_t;

static const char *const s_tag = "buddy_ble";
static buddy_ble_state_t s_ble = {
    .conn_handle = BLE_HS_CONN_HANDLE_NONE,
    .rejecting_conn_handle = BLE_HS_CONN_HANDLE_NONE,
};
static char s_device_name[BUDDY_BLE_DEVICE_NAME_SIZE];
static uint8_t s_tx_scratch[BUDDY_BLE_TX_CHUNK_MAX];
static uint16_t s_tx_value_handle;
static uint8_t s_own_addr_type;
static struct ble_npl_event s_delete_bonds_event;
static struct ble_npl_event s_termination_recovery_event;
static struct ble_npl_event s_adv_reconcile_event;
static struct ble_npl_callout s_bond_retry_callout;
static struct ble_npl_callout s_adv_retry_callout;

static const ble_uuid128_t s_nus_service_uuid = BLE_UUID128_INIT(BUDDY_NUS_SERVICE_UUID_BYTES);
static const ble_uuid128_t s_nus_rx_uuid = BLE_UUID128_INIT(BUDDY_NUS_RX_UUID_BYTES);
static const ble_uuid128_t s_nus_tx_uuid = BLE_UUID128_INIT(BUDDY_NUS_TX_UUID_BYTES);

static int buddy_gap_event(struct ble_gap_event *event, void *context);
static int buddy_gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *context, void *arg);
static void buddy_emit(const buddy_ble_event_t *event);
static int buddy_reconcile_advertising(void);

static void buddy_schedule_bond_work(void)
{
    if (!ble_npl_event_is_queued(&s_delete_bonds_event)) {
        ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_delete_bonds_event);
    }
}

static void buddy_schedule_adv_work(void)
{
    if (!ble_npl_event_is_queued(&s_adv_reconcile_event)) {
        ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_adv_reconcile_event);
    }
}

static void buddy_emit_bond_delete_result(int status, bool success)
{
    const buddy_ble_event_t result_event = {
        .type = BUDDY_BLE_EVENT_BOND_DELETE_RESULT,
        .data.bond_delete_result = {
            .status = status,
            .success = success,
        },
    };

    buddy_emit(&result_event);
}

static void buddy_schedule_bond_retry(int status)
{
    ble_npl_time_t ticks;
    bool final_failure = false;
    unsigned int attempt;

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    if (!s_ble.delete_bonds_pending) {
        xSemaphoreGive(s_ble.mutex);
        return;
    }
    s_ble.delete_bonds_result = status;
    if (s_ble.delete_attempts < UINT8_MAX) {
        ++s_ble.delete_attempts;
    }
    attempt = s_ble.delete_attempts;
    if (attempt >= BUDDY_BLE_DELETE_MAX_ATTEMPTS && !s_ble.delete_final_reported) {
        s_ble.delete_final_reported = true;
        final_failure = true;
    }
    xSemaphoreGive(s_ble.mutex);

    if (final_failure) {
        buddy_emit_bond_delete_result(status, false);
        return;
    }
    if (attempt >= BUDDY_BLE_DELETE_MAX_ATTEMPTS ||
        ble_npl_time_ms_to_ticks(buddy_ble_retry_delay_ms(attempt - 1U), &ticks) != 0 ||
        ble_npl_callout_reset(&s_bond_retry_callout, ticks) != BLE_NPL_OK) {
        xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
        if (!s_ble.delete_final_reported) {
            s_ble.delete_final_reported = true;
            final_failure = true;
        }
        xSemaphoreGive(s_ble.mutex);
        if (final_failure) {
            buddy_emit_bond_delete_result(status, false);
        }
    }
}

static void buddy_recover_termination(struct ble_npl_event *event)
{
    bool schedule_reset = false;
    int reason;

    (void)event;
    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    if (!s_ble.reset_scheduled &&
        (s_ble.rejecting_conn_handle != BLE_HS_CONN_HANDLE_NONE ||
         (s_ble.delete_bonds_pending && s_ble.conn_handle != BLE_HS_CONN_HANDLE_NONE))) {
        s_ble.reset_scheduled = true;
        schedule_reset = true;
    }
    reason = s_ble.termination_failure_reason;
    xSemaphoreGive(s_ble.mutex);

    if (schedule_reset) {
        ble_hs_sched_reset(reason);
    }
}

static void buddy_schedule_termination_recovery(int reason)
{
    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    s_ble.termination_failure_reason = reason;
    xSemaphoreGive(s_ble.mutex);

    if (!ble_npl_event_is_queued(&s_termination_recovery_event)) {
        ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_termination_recovery_event);
    }
}

static const struct ble_gatt_svc_def s_gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_nus_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &s_nus_rx_uuid.u,
                .access_cb = buddy_gatt_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP |
                         BLE_GATT_CHR_F_WRITE_ENC,
            },
            {
                .uuid = &s_nus_tx_uuid.u,
                .access_cb = buddy_gatt_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC |
                         BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_NOTIFY_INDICATE_ENC,
                .val_handle = &s_tx_value_handle,
            },
            {0},
        },
    },
    {0},
};

/* ESP-IDF's NimBLE examples use this store initializer, but its public header
 * only exposes the read/write/delete hooks. */
void ble_store_config_init(void);

typedef int buddy_att_svr_access_fn_t(uint16_t conn_handle, uint16_t attr_handle,
                                      uint8_t att_op, uint16_t offset,
                                      struct os_mbuf **om, void *arg);

int __real_ble_att_svr_register(const ble_uuid_t *uuid, uint8_t flags, uint8_t min_key_size,
                                uint16_t *handle_id, buddy_att_svr_access_fn_t *cb,
                                void *cb_arg);

int __wrap_ble_att_svr_register(const ble_uuid_t *uuid, uint8_t flags, uint8_t min_key_size,
                                uint16_t *handle_id, buddy_att_svr_access_fn_t *cb,
                                void *cb_arg)
{
    if (buddy_ble_should_protect_cccd_read(ble_uuid_u16(uuid),
                                           (flags & BLE_ATT_F_WRITE_ENC) != 0U)) {
        flags |= BLE_ATT_F_READ_ENC;
    }
    return __real_ble_att_svr_register(uuid, flags, min_key_size, handle_id, cb, cb_arg);
}

static esp_err_t buddy_ble_error(int rc)
{
    return rc == BLE_HS_ENOMEM ? ESP_ERR_NO_MEM : ESP_FAIL;
}

static void buddy_emit(const buddy_ble_event_t *event)
{
    if (s_ble.event_cb != NULL) {
        s_ble.event_cb(event, s_ble.event_context);
    }
}

typedef struct {
    uint16_t conn_handle;
    uint32_t generation;
} buddy_rx_context_t;

static bool buddy_rx_gate_open(const buddy_rx_context_t *rx_context)
{
    bool open;

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    open = s_ble.start_requested && !s_ble.delete_bonds_pending && s_ble.secure &&
           s_ble.conn_handle == rx_context->conn_handle &&
           s_ble.connection_generation == rx_context->generation;
    xSemaphoreGive(s_ble.mutex);
    return open;
}

static bool buddy_rx_line(const char *line, size_t length, void *context)
{
    const buddy_rx_context_t *rx_context = context;
    const buddy_ble_event_t event = {
        .type = BUDDY_BLE_EVENT_RX_LINE,
        .data.rx_line = {
            .data = line,
            .length = length,
        },
    };

    if (!buddy_rx_gate_open(rx_context)) {
        buddy_line_init(&s_ble.rx);
        return false;
    }
    buddy_emit(&event);
    if (!buddy_rx_gate_open(rx_context)) {
        buddy_line_init(&s_ble.rx);
        return false;
    }
    return true;
}

static int buddy_gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *context, void *arg)
{
    struct os_mbuf *mbuf;
    buddy_rx_context_t rx_context;
    bool accept_write;

    (void)attr_handle;
    (void)arg;

    if (context->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        return 0;
    }
    if (context->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    accept_write = s_ble.start_requested && s_ble.conn_handle == conn_handle && s_ble.secure;
    rx_context.conn_handle = conn_handle;
    rx_context.generation = s_ble.connection_generation;
    xSemaphoreGive(s_ble.mutex);
    if (!accept_write) {
        return BLE_ATT_ERR_INSUFFICIENT_ENC;
    }

    for (mbuf = context->om; mbuf != NULL; mbuf = SLIST_NEXT(mbuf, om_next)) {
        if (buddy_line_push(&s_ble.rx, mbuf->om_data, mbuf->om_len, buddy_rx_line,
                            &rx_context) == BUDDY_LINE_ABORTED) {
            break;
        }
    }
    return 0;
}

static void buddy_adv_retry(struct ble_npl_event *event)
{
    (void)event;
    (void)buddy_reconcile_advertising();
}

static void buddy_adv_reconcile_work(struct ble_npl_event *event)
{
    (void)event;
    (void)buddy_reconcile_advertising();
}

static void buddy_schedule_adv_retry(int status)
{
    ble_npl_time_t ticks;
    ble_npl_error_t reset_rc;
    unsigned int attempt;

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    if (s_ble.adv_retry_attempts < BUDDY_BLE_DELETE_MAX_ATTEMPTS) {
        ++s_ble.adv_retry_attempts;
    }
    attempt = s_ble.adv_retry_attempts;
    xSemaphoreGive(s_ble.mutex);

    if (attempt >= BUDDY_BLE_DELETE_MAX_ATTEMPTS) {
        ESP_LOGE(s_tag, "BLE advertising state retry exhausted: %d", status);
        return;
    }
    if (ble_npl_time_ms_to_ticks(buddy_ble_retry_delay_ms(attempt - 1U), &ticks) != 0) {
        buddy_schedule_adv_work();
        return;
    }
    reset_rc = ble_npl_callout_reset(&s_adv_retry_callout, ticks);
    if (reset_rc != BLE_NPL_OK) {
        buddy_schedule_adv_work();
    }
}

static int buddy_reconcile_advertising(void)
{
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields response_fields;
    struct ble_gap_adv_params parameters;
    bool desired;
    bool physical_link;
    uint32_t epoch;
    int rc;

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    physical_link = s_ble.conn_handle != BLE_HS_CONN_HANDLE_NONE ||
                    s_ble.rejecting_conn_handle != BLE_HS_CONN_HANDLE_NONE;
    desired = buddy_ble_should_advertise(s_ble.start_requested, s_ble.host_synced,
                                         s_ble.delete_bonds_pending, physical_link);
    epoch = s_ble.advertising_epoch;
    xSemaphoreGive(s_ble.mutex);

    if (!desired) {
        xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
        if (epoch != s_ble.advertising_epoch) {
            xSemaphoreGive(s_ble.mutex);
            buddy_schedule_adv_work();
            return 0;
        }
        rc = ble_gap_adv_active() ? ble_gap_adv_stop() : 0;
        xSemaphoreGive(s_ble.mutex);
        if (rc != 0) {
            buddy_schedule_adv_retry(rc);
        } else {
            ble_npl_callout_stop(&s_adv_retry_callout);
            xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
            s_ble.adv_retry_attempts = 0;
            xSemaphoreGive(s_ble.mutex);
        }
        return rc;
    }
    if (ble_gap_adv_active()) {
        ble_npl_callout_stop(&s_adv_retry_callout);
        xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
        s_ble.adv_retry_attempts = 0;
        xSemaphoreGive(s_ble.mutex);
        return 0;
    }

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t *)&s_nus_service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        buddy_schedule_adv_retry(rc);
        return rc;
    }

    memset(&response_fields, 0, sizeof(response_fields));
    response_fields.name = (uint8_t *)s_device_name;
    response_fields.name_len = strlen(s_device_name);
    response_fields.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&response_fields);
    if (rc != 0) {
        buddy_schedule_adv_retry(rc);
        return rc;
    }

    memset(&parameters, 0, sizeof(parameters));
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    physical_link = s_ble.conn_handle != BLE_HS_CONN_HANDLE_NONE ||
                    s_ble.rejecting_conn_handle != BLE_HS_CONN_HANDLE_NONE;
    if (!buddy_ble_adv_epoch_allows_start(epoch, s_ble.advertising_epoch,
                                          s_ble.start_requested, s_ble.host_synced,
                                          s_ble.delete_bonds_pending, physical_link)) {
        xSemaphoreGive(s_ble.mutex);
        buddy_schedule_adv_work();
        return 0;
    }
    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &parameters, buddy_gap_event,
                           NULL);
    xSemaphoreGive(s_ble.mutex);
    if (rc != 0) {
        buddy_schedule_adv_retry(rc);
    } else {
        ble_npl_callout_stop(&s_adv_retry_callout);
        xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
        s_ble.adv_retry_attempts = 0;
        xSemaphoreGive(s_ble.mutex);
    }
    return rc;
}

static uint32_t buddy_random_passkey(void)
{
    const uint32_t limit = UINT32_MAX - (UINT32_MAX % 1000000U);
    uint32_t value;

    do {
        value = esp_random();
    } while (value >= limit);
    return value % 1000000U;
}

#define BUDDY_NIMBLE_NVS_NAMESPACE "nimble_bond"
#define BUDDY_NIMBLE_LOCAL_IRK_PREFIX "local_irk_"

static int buddy_collect_delete_peer(int obj_type, union ble_store_value *value, void *context)
{
    buddy_ble_state_t *state = context;
    size_t index;

    (void)obj_type;
    for (index = 0; index < state->delete_peer_count; ++index) {
        if (ble_addr_cmp(&state->delete_peers[index], &value->sec.peer_addr) == 0) {
            if (obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC) {
                state->delete_peer_secs[index] = value->sec;
                state->delete_peer_sec_present[index] = true;
            }
            return 0;
        }
    }
    if (state->delete_peer_count >=
        sizeof(state->delete_peers) / sizeof(state->delete_peers[0])) {
        return BLE_HS_ENOMEM;
    }
    state->delete_peers[state->delete_peer_count++] = value->sec.peer_addr;
    if (obj_type == BLE_STORE_OBJ_TYPE_PEER_SEC) {
        state->delete_peer_secs[state->delete_peer_count - 1U] = value->sec;
        state->delete_peer_sec_present[state->delete_peer_count - 1U] = true;
    }
    return 0;
}

static int buddy_store_snapshot_delete(void)
{
    struct ble_store_key_local_irk key = {0};
    uint8_t active_irk[16];
    int rc;

    if (s_ble.delete_snapshot_ready) {
        return 0;
    }
    s_ble.delete_peer_count = 0;
    memset(s_ble.delete_peer_sec_present, 0, sizeof(s_ble.delete_peer_sec_present));
    rc = ble_store_read_local_irk(&key, &s_ble.delete_local_irk);
    if (rc != 0) {
        return rc;
    }
    rc = ble_gap_read_local_irk(active_irk);
    if (rc != 0) {
        return rc;
    }
    if (memcmp(active_irk, s_ble.delete_local_irk.irk, sizeof(active_irk)) != 0) {
        return BLE_HS_ESTORE_FAIL;
    }
    rc = ble_store_iterate(BLE_STORE_OBJ_TYPE_PEER_SEC, buddy_collect_delete_peer, &s_ble);
    if (rc == 0) {
        rc = ble_store_iterate(BLE_STORE_OBJ_TYPE_OUR_SEC, buddy_collect_delete_peer, &s_ble);
    }
    if (rc == 0) {
        s_ble.delete_snapshot_ready = true;
    }
    return rc;
}

static int buddy_store_remove_controller_peers(void *context)
{
    uint8_t rpa[6];
    size_t index;
    int rc;

    (void)context;
    rc = buddy_store_snapshot_delete();
    if (rc != 0) {
        return rc;
    }
    for (index = 0; index < s_ble.delete_peer_count; ++index) {
        const ble_addr_t *peer = &s_ble.delete_peers[index];
        const uint8_t controller_addr_type = peer->type > BLE_ADDR_RANDOM
                                                 ? peer->type % 2U
                                                 : peer->type;

        rc = ble_gap_unpair(peer);
        if (rc != 0 && rc != BLE_HS_ENOENT) {
            return rc;
        }
        rc = ble_gap_rd_local_resolv_addr(controller_addr_type, peer, rpa);
        if (rc == 0 && s_ble.delete_peer_sec_present[index]) {
            /* ble_gap_unpair() in IDF 5.5.3 can report success after a
             * controller remove failure while deleting the RAM record. Put
             * the captured peer IRK back so its supported unpair lifecycle
             * can retry the controller operation. The data gate remains
             * closed and the verified erase below removes this temporary
             * record. */
            rc = ble_store_write_peer_sec(&s_ble.delete_peer_secs[index]);
            if (rc != 0) {
                return rc;
            }
            rc = ble_gap_unpair(peer);
            if (rc != 0 && rc != BLE_HS_ENOENT) {
                return rc;
            }
            rc = ble_gap_rd_local_resolv_addr(controller_addr_type, peer, rpa);
        }
        if (rc == 0) {
            return BLE_HS_EBUSY;
        }
        if (rc != BLE_HS_HCI_ERR(BLE_ERR_UNK_CONN_ID)) {
            return rc;
        }
    }
    return 0;
}

static int buddy_store_erase_persistent(void *context)
{
    nvs_handle_t handle;
    esp_err_t err;

    (void)context;
    err = nvs_open(BUDDY_NIMBLE_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static int buddy_store_persistent_is_empty(void *context, bool *empty)
{
    nvs_handle_t handle;
    size_t used_entries = 0;
    esp_err_t err;

    (void)context;
    err = nvs_open(BUDDY_NIMBLE_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        *empty = true;
        return 0;
    }
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_used_entry_count(handle, &used_entries);
    nvs_close(handle);
    if (err == ESP_OK) {
        *empty = used_entries == 0U;
    }
    return err;
}

static int buddy_store_reload_volatile(void *context)
{
    (void)context;
    ble_store_config_init();
    return 0;
}

static int buddy_store_restore_local_identity(void *context)
{
    (void)context;
    return ble_store_write_local_irk(&s_ble.delete_local_irk);
}

static int buddy_store_persistent_is_clean(void *context, bool *clean)
{
    nvs_handle_t handle;
    nvs_iterator_t iterator = NULL;
    struct ble_store_value_local_irk stored_irk;
    nvs_entry_info_t info;
    size_t blob_size = sizeof(stored_irk);
    size_t identity_entries = 0;
    esp_err_t err;

    (void)context;
    *clean = false;
    err = nvs_open(BUDDY_NIMBLE_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_entry_find_in_handle(handle, NVS_TYPE_ANY, &iterator);
    while (err == ESP_OK) {
        nvs_entry_info(iterator, &info);
        if (strncmp(info.key, BUDDY_NIMBLE_LOCAL_IRK_PREFIX,
                    strlen(BUDDY_NIMBLE_LOCAL_IRK_PREFIX)) != 0) {
            nvs_release_iterator(iterator);
            nvs_close(handle);
            return 0;
        }
        ++identity_entries;
        if (identity_entries != 1U) {
            nvs_release_iterator(iterator);
            nvs_close(handle);
            return 0;
        }
        err = nvs_get_blob(handle, info.key, &stored_irk, &blob_size);
        if (err != ESP_OK || blob_size != sizeof(stored_irk) ||
            memcmp(&stored_irk, &s_ble.delete_local_irk, sizeof(stored_irk)) != 0) {
            nvs_release_iterator(iterator);
            nvs_close(handle);
            return err == ESP_OK ? ESP_ERR_INVALID_STATE : err;
        }
        err = nvs_entry_next(&iterator);
    }
    nvs_release_iterator(iterator);
    nvs_close(handle);
    if (err != ESP_ERR_NVS_NOT_FOUND) {
        return err;
    }
    *clean = identity_entries == 1U;
    return 0;
}

static int buddy_store_volatile_is_empty(void *context, bool *empty)
{
    static const int object_types[] = {
        BLE_STORE_OBJ_TYPE_OUR_SEC,
        BLE_STORE_OBJ_TYPE_PEER_SEC,
        BLE_STORE_OBJ_TYPE_CCCD,
        BLE_STORE_OBJ_TYPE_CSFC,
        BLE_STORE_OBJ_TYPE_PEER_ADDR,
#if MYNEWT_VAL(ENC_ADV_DATA)
        BLE_STORE_OBJ_TYPE_ENC_ADV_DATA,
#endif
    };
    size_t index;

    (void)context;
    for (index = 0; index < sizeof(object_types) / sizeof(object_types[0]); ++index) {
        int count;
        int rc = ble_store_util_count(object_types[index], &count);
        if (rc != 0) {
            return rc;
        }
        if (count != 0) {
            *empty = false;
            return 0;
        }
    }
    {
        struct ble_store_key_local_irk key = {0};
        struct ble_store_value_local_irk identity;
        int count;
        int rc = ble_store_util_count(BLE_STORE_OBJ_TYPE_LOCAL_IRK, &count);
        if (rc != 0) {
            return rc;
        }
        if (count != 1 || ble_store_read_local_irk(&key, &identity) != 0 ||
            memcmp(&identity, &s_ble.delete_local_irk, sizeof(identity)) != 0) {
            *empty = false;
            return 0;
        }
    }
    *empty = true;
    return 0;
}

static int buddy_store_controller_is_clean(void *context, bool *clean)
{
    uint8_t active_irk[16];
    uint8_t rpa[6];
    size_t index;
    int rc;

    (void)context;
    *clean = false;
    rc = ble_gap_read_local_irk(active_irk);
    if (rc != 0) {
        return rc;
    }
    if (memcmp(active_irk, s_ble.delete_local_irk.irk, sizeof(active_irk)) != 0) {
        return 0;
    }
    for (index = 0; index < s_ble.delete_peer_count; ++index) {
        const ble_addr_t *peer = &s_ble.delete_peers[index];
        const uint8_t controller_addr_type = peer->type > BLE_ADDR_RANDOM
                                                 ? peer->type % 2U
                                                 : peer->type;
        rc = ble_gap_rd_local_resolv_addr(controller_addr_type, peer, rpa);
        if (rc == 0) {
            return 0;
        }
        if (rc != BLE_HS_HCI_ERR(BLE_ERR_UNK_CONN_ID)) {
            return rc;
        }
    }
    *clean = true;
    return 0;
}

static int buddy_delete_all_peers(void)
{
    const buddy_ble_store_ops_t ops = {
        .remove_controller_peers = buddy_store_remove_controller_peers,
        .erase_persistent = buddy_store_erase_persistent,
        .persistent_is_empty = buddy_store_persistent_is_empty,
        .reload_volatile = buddy_store_reload_volatile,
        .restore_local_identity = buddy_store_restore_local_identity,
        .persistent_is_clean = buddy_store_persistent_is_clean,
        .volatile_is_empty = buddy_store_volatile_is_empty,
        .controller_is_clean = buddy_store_controller_is_clean,
    };

    return buddy_ble_store_clear_verified(&ops);
}

static void buddy_finish_bond_deletion(struct ble_npl_event *event)
{
    uint16_t conn_handle;
    bool delete_pending;
    bool reset_scheduled;
    bool advertise;
    int rc;

    (void)event;

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    delete_pending = s_ble.delete_bonds_pending && !s_ble.delete_final_reported;
    conn_handle = s_ble.conn_handle != BLE_HS_CONN_HANDLE_NONE
                      ? s_ble.conn_handle
                      : s_ble.rejecting_conn_handle;
    reset_scheduled = s_ble.reset_scheduled;
    xSemaphoreGive(s_ble.mutex);

    if (!delete_pending) {
        return;
    }

    if (conn_handle != BLE_HS_CONN_HANDLE_NONE || reset_scheduled) {
        if (!reset_scheduled) {
            rc = ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            if (rc != 0 && rc != BLE_HS_EALREADY) {
                buddy_schedule_termination_recovery(rc);
            }
        }
        return;
    }

    rc = buddy_reconcile_advertising();
    if (rc != 0 || ble_gap_adv_active()) {
        if (rc == 0) {
            rc = BLE_HS_EBUSY;
        }
        buddy_schedule_bond_retry(rc);
        return;
    }

    rc = buddy_delete_all_peers();

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    s_ble.delete_bonds_result = rc;
    if (rc == 0) {
        s_ble.delete_bonds_pending = false;
        s_ble.delete_snapshot_ready = false;
        s_ble.delete_peer_count = 0;
        ++s_ble.advertising_epoch;
        s_ble.delete_attempts = 0;
        s_ble.delete_final_reported = false;
    }
    advertise = rc == 0 && s_ble.start_requested && s_ble.host_synced &&
                s_ble.conn_handle == BLE_HS_CONN_HANDLE_NONE;
    xSemaphoreGive(s_ble.mutex);

    if (rc != 0) {
        ESP_LOGE(s_tag, "Failed to delete BLE bonds: %d", rc);
        buddy_schedule_bond_retry(rc);
    } else {
        ble_npl_callout_stop(&s_bond_retry_callout);
        buddy_emit_bond_delete_result(0, true);
    }
    if (advertise) {
        rc = buddy_reconcile_advertising();
        if (rc != 0) {
            ESP_LOGE(s_tag, "Failed to restart BLE advertising: %d", rc);
        }
    }
}

static void buddy_on_disconnect(uint16_t conn_handle, int reason)
{
    buddy_ble_event_t event = {
        .type = BUDDY_BLE_EVENT_DISCONNECTED,
        .data.disconnected.reason = reason,
    };
    bool delete_bonds;
    bool emit_disconnect = false;
    bool advertise;

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    if (s_ble.conn_handle == conn_handle) {
        s_ble.conn_handle = BLE_HS_CONN_HANDLE_NONE;
        ++s_ble.connection_generation;
        s_ble.encrypted = false;
        s_ble.secure = false;
        buddy_line_init(&s_ble.rx);
        emit_disconnect = true;
        ++s_ble.advertising_epoch;
    }
    if (s_ble.rejecting_conn_handle == conn_handle) {
        s_ble.rejecting_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        ++s_ble.advertising_epoch;
    }
    delete_bonds = s_ble.delete_bonds_pending;
    advertise = s_ble.start_requested && !delete_bonds &&
                s_ble.conn_handle == BLE_HS_CONN_HANDLE_NONE &&
                s_ble.rejecting_conn_handle == BLE_HS_CONN_HANDLE_NONE;
    xSemaphoreGive(s_ble.mutex);

    if (emit_disconnect) {
        buddy_emit(&event);
    }
    if (delete_bonds) {
        buddy_finish_bond_deletion(NULL);
    } else if (advertise) {
        int rc = buddy_reconcile_advertising();
        if (rc != 0) {
            ESP_LOGE(s_tag, "Failed to restart BLE advertising: %d", rc);
        }
    }
}

static int buddy_gap_event(struct ble_gap_event *event, void *context)
{
    (void)context;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            return buddy_reconcile_advertising();
        }

        xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
        if (!s_ble.start_requested || s_ble.delete_bonds_pending ||
            s_ble.conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            int rc;

            s_ble.rejecting_conn_handle = event->connect.conn_handle;
            ++s_ble.advertising_epoch;
            xSemaphoreGive(s_ble.mutex);
            rc = ble_gap_terminate(event->connect.conn_handle, BLE_ERR_CONN_LIMIT);
            if (rc != 0) {
                buddy_schedule_termination_recovery(rc);
            }
            return rc;
        }
        s_ble.conn_handle = event->connect.conn_handle;
        ++s_ble.advertising_epoch;
        ++s_ble.connection_generation;
        s_ble.encrypted = false;
        s_ble.secure = false;
        buddy_line_init(&s_ble.rx);
        xSemaphoreGive(s_ble.mutex);

        {
            const buddy_ble_event_t connected_event = {
                .type = BUDDY_BLE_EVENT_CONNECTED,
                .data.connected.conn_handle = event->connect.conn_handle,
            };
            buddy_emit(&connected_event);
        }
        {
            bool initiate_security;

            xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
            initiate_security = s_ble.start_requested && !s_ble.delete_bonds_pending &&
                                s_ble.conn_handle == event->connect.conn_handle;
            xSemaphoreGive(s_ble.mutex);
            if (!initiate_security) {
                return 0;
            }

            int rc = ble_gap_security_initiate(event->connect.conn_handle);
            if (rc != 0) {
                bool publish_event;
                const buddy_ble_event_t encryption_event = {
                    .type = BUDDY_BLE_EVENT_ENCRYPTION,
                    .data.encryption.status = rc,
                };

                xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
                publish_event = s_ble.start_requested && !s_ble.delete_bonds_pending &&
                                s_ble.conn_handle == event->connect.conn_handle;
                xSemaphoreGive(s_ble.mutex);
                if (publish_event) {
                    buddy_emit(&encryption_event);
                }
            }
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        buddy_on_disconnect(event->disconnect.conn.conn_handle, event->disconnect.reason);
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        return buddy_reconcile_advertising();

    case BLE_GAP_EVENT_ENC_CHANGE: {
        struct ble_gap_conn_desc description = {0};
        bool publish_event;
        buddy_ble_event_t encryption_event = {
            .type = BUDDY_BLE_EVENT_ENCRYPTION,
            .data.encryption.status = event->enc_change.status,
        };

        if (event->enc_change.status == 0 &&
            ble_gap_conn_find(event->enc_change.conn_handle, &description) == 0) {
            encryption_event.data.encryption.encrypted = description.sec_state.encrypted;
            encryption_event.data.encryption.authenticated = description.sec_state.authenticated;
            encryption_event.data.encryption.bonded = description.sec_state.bonded;
        }

        xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
        publish_event = s_ble.start_requested && !s_ble.delete_bonds_pending &&
                        s_ble.conn_handle == event->enc_change.conn_handle;
        if (publish_event) {
            s_ble.encrypted = encryption_event.data.encryption.encrypted;
            s_ble.secure = buddy_ble_link_is_secure(
                encryption_event.data.encryption.encrypted,
                encryption_event.data.encryption.authenticated,
                encryption_event.data.encryption.bonded,
                description.sec_state.key_size);
        } else if (s_ble.conn_handle == event->enc_change.conn_handle) {
            s_ble.encrypted = false;
            s_ble.secure = false;
        }
        xSemaphoreGive(s_ble.mutex);
        if (publish_event) {
            buddy_emit(&encryption_event);
        }
        return 0;
    }

    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        bool publish_event;

        xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
        publish_event = s_ble.start_requested && !s_ble.delete_bonds_pending &&
                        s_ble.conn_handle == event->passkey.conn_handle;
        xSemaphoreGive(s_ble.mutex);
        if (publish_event && event->passkey.params.action == BLE_SM_IOACT_DISP) {
            const uint32_t passkey = buddy_random_passkey();
            const buddy_ble_event_t passkey_event = {
                .type = BUDDY_BLE_EVENT_PASSKEY,
                .data.passkey.value = passkey,
            };
            struct ble_sm_io io = {
                .action = BLE_SM_IOACT_DISP,
                .passkey = passkey,
            };
            bool inject_passkey;

            buddy_emit(&passkey_event);
            xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
            inject_passkey = s_ble.start_requested && !s_ble.delete_bonds_pending &&
                             s_ble.conn_handle == event->passkey.conn_handle;
            xSemaphoreGive(s_ble.mutex);
            if (!inject_passkey) {
                return BLE_HS_ENOTSUP;
            }
            return ble_sm_inject_io(event->passkey.conn_handle, &io);
        }
        return BLE_HS_ENOTSUP;
    }

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        struct ble_gap_conn_desc description;
        bool allow_pairing;
        int rc;

        xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
        allow_pairing = s_ble.start_requested && !s_ble.delete_bonds_pending &&
                        s_ble.conn_handle == event->repeat_pairing.conn_handle;
        xSemaphoreGive(s_ble.mutex);
        if (!allow_pairing) {
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        }

        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &description);
        if (rc != 0) {
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        }
        rc = ble_store_util_delete_peer(&description.peer_id_addr);
        return rc == 0 ? BLE_GAP_REPEAT_PAIRING_RETRY : BLE_GAP_REPEAT_PAIRING_IGNORE;
    }

    case BLE_GAP_EVENT_TERM_FAILURE: {
        bool recover_delete;

        xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
        recover_delete = s_ble.rejecting_conn_handle == event->term_failure.conn_handle ||
                         (s_ble.delete_bonds_pending &&
                          s_ble.conn_handle == event->term_failure.conn_handle);
        xSemaphoreGive(s_ble.mutex);
        if (recover_delete) {
            buddy_schedule_termination_recovery(event->term_failure.status);
        }
        return 0;
    }

    default:
        return 0;
    }
}

static void buddy_on_reset(int reason)
{
    bool was_connected;

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    was_connected = s_ble.conn_handle != BLE_HS_CONN_HANDLE_NONE;
    s_ble.conn_handle = BLE_HS_CONN_HANDLE_NONE;
    ++s_ble.connection_generation;
    s_ble.rejecting_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_ble.encrypted = false;
    s_ble.secure = false;
    s_ble.host_synced = false;
    ++s_ble.advertising_epoch;
    s_ble.reset_scheduled = false;
    buddy_line_init(&s_ble.rx);
    xSemaphoreGive(s_ble.mutex);

    if (was_connected) {
        const buddy_ble_event_t event = {
            .type = BUDDY_BLE_EVENT_DISCONNECTED,
            .data.disconnected.reason = reason,
        };
        buddy_emit(&event);
    }
}

static void buddy_on_sync(void)
{
    bool delete_bonds;
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) {
        rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    }
    if (rc != 0) {
        ESP_LOGE(s_tag, "Failed to select BLE identity address: %d", rc);
        return;
    }

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    s_ble.host_synced = true;
    ++s_ble.advertising_epoch;
    delete_bonds = s_ble.delete_bonds_pending;
    xSemaphoreGive(s_ble.mutex);

    if (delete_bonds) {
        buddy_schedule_bond_work();
        return;
    }

    rc = buddy_reconcile_advertising();
    if (rc != 0) {
        ESP_LOGE(s_tag, "Failed to start BLE advertising: %d", rc);
    }
}

static void buddy_host_task(void *context)
{
    (void)context;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static int buddy_runtime_host_init(void *context)
{
    (void)context;
    return nimble_port_init();
}

static int buddy_runtime_events_init(void *context)
{
    (void)context;
    ble_npl_event_init(&s_delete_bonds_event, buddy_finish_bond_deletion, NULL);
    ble_npl_event_init(&s_termination_recovery_event, buddy_recover_termination, NULL);
    ble_npl_event_init(&s_adv_reconcile_event, buddy_adv_reconcile_work, NULL);
    return 0;
}

static int buddy_runtime_bond_callout_init(void *context)
{
    (void)context;
    return ble_npl_callout_init(&s_bond_retry_callout, nimble_port_get_dflt_eventq(),
                                buddy_finish_bond_deletion, NULL);
}

static int buddy_runtime_adv_callout_init(void *context)
{
    (void)context;
    return ble_npl_callout_init(&s_adv_retry_callout, nimble_port_get_dflt_eventq(),
                                buddy_adv_retry, NULL);
}

static int buddy_runtime_gatt_init(void *context)
{
    int rc;

    (void)context;
    ble_hs_cfg.reset_cb = buddy_on_reset;
    ble_hs_cfg.sync_cb = buddy_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_sc_only = 1;
    ble_hs_cfg.sm_sec_lvl = 4;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    rc = ble_gatts_count_cfg(s_gatt_services);
    if (rc == 0) {
        rc = ble_gatts_add_svcs(s_gatt_services);
    }
    if (rc == 0) {
        rc = ble_svc_gap_device_name_set(s_device_name);
    }
    return rc;
}

static void buddy_runtime_adv_callout_deinit(void *context)
{
    (void)context;
    ble_npl_callout_stop(&s_adv_retry_callout);
    ble_npl_callout_deinit(&s_adv_retry_callout);
}

static void buddy_runtime_bond_callout_deinit(void *context)
{
    (void)context;
    ble_npl_callout_stop(&s_bond_retry_callout);
    ble_npl_callout_deinit(&s_bond_retry_callout);
}

static void buddy_runtime_events_deinit(void *context)
{
    (void)context;
    ble_npl_event_deinit(&s_adv_reconcile_event);
    ble_npl_event_deinit(&s_termination_recovery_event);
    ble_npl_event_deinit(&s_delete_bonds_event);
}

static int buddy_runtime_host_deinit(void *context)
{
    (void)context;
    return nimble_port_deinit();
}

static void buddy_runtime_clear(void *context)
{
    (void)context;
    memset(&s_bond_retry_callout, 0, sizeof(s_bond_retry_callout));
    memset(&s_adv_retry_callout, 0, sizeof(s_adv_retry_callout));
    memset(&s_delete_bonds_event, 0, sizeof(s_delete_bonds_event));
    memset(&s_termination_recovery_event, 0, sizeof(s_termination_recovery_event));
    memset(&s_adv_reconcile_event, 0, sizeof(s_adv_reconcile_event));
}

esp_err_t buddy_ble_init(const buddy_ble_config_t *config)
{
    const buddy_ble_lifecycle_ops_t lifecycle_ops = {
        .host_init = buddy_runtime_host_init,
        .events_init = buddy_runtime_events_init,
        .bond_callout_init = buddy_runtime_bond_callout_init,
        .adv_callout_init = buddy_runtime_adv_callout_init,
        .gatt_init = buddy_runtime_gatt_init,
        .adv_callout_deinit = buddy_runtime_adv_callout_deinit,
        .bond_callout_deinit = buddy_runtime_bond_callout_deinit,
        .events_deinit = buddy_runtime_events_deinit,
        .host_deinit = buddy_runtime_host_deinit,
        .clear = buddy_runtime_clear,
    };
    uint8_t mac[6];
    bool retry_safe;
    esp_err_t err;
    int rc;

    if (config == NULL || config->event_cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!buddy_ble_lifecycle_retry_allowed(s_ble.initialized,
                                           s_ble.init_rollback_blocked)) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ble.mutex = xSemaphoreCreateMutex();
    if (s_ble.mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_ble.event_cb = config->event_cb;
    s_ble.event_context = config->event_context;
    buddy_line_init(&s_ble.rx);
    err = esp_read_mac(mac, ESP_MAC_BT);
    if (err != ESP_OK) {
        vSemaphoreDelete(s_ble.mutex);
        s_ble.mutex = NULL;
        return err;
    }
    (void)snprintf(s_device_name, sizeof(s_device_name), "Claude-%02X%02X%02X", mac[3], mac[4],
                   mac[5]);

    rc = buddy_ble_lifecycle_init(&lifecycle_ops, &retry_safe);
    if (rc != 0) {
        if (!retry_safe) {
            s_ble.init_rollback_blocked = true;
            return buddy_ble_error(rc);
        }
        vSemaphoreDelete(s_ble.mutex);
        s_ble.mutex = NULL;
        s_ble.event_cb = NULL;
        s_ble.event_context = NULL;
        return buddy_ble_error(rc);
    }

    ble_store_config_init();
    s_ble.initialized = true;
    return ESP_OK;
}

esp_err_t buddy_ble_start(void)
{
    bool launch_host;
    bool advertise;

    if (!s_ble.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    s_ble.start_requested = true;
    ++s_ble.advertising_epoch;
    launch_host = !s_ble.host_running;
    if (launch_host) {
        s_ble.host_running = true;
    }
    advertise = s_ble.host_synced;
    xSemaphoreGive(s_ble.mutex);

    if (launch_host) {
        nimble_port_freertos_init(buddy_host_task);
        return ESP_OK;
    }
    if (advertise) {
        buddy_schedule_adv_work();
    }
    return ESP_OK;
}

esp_err_t buddy_ble_stop(void)
{
    uint16_t conn_handle;
    int rc = 0;

    if (!s_ble.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    s_ble.start_requested = false;
    ++s_ble.advertising_epoch;
    ++s_ble.connection_generation;
    s_ble.encrypted = false;
    s_ble.secure = false;
    conn_handle = s_ble.conn_handle != BLE_HS_CONN_HANDLE_NONE
                      ? s_ble.conn_handle
                      : s_ble.rejecting_conn_handle;
    xSemaphoreGive(s_ble.mutex);

    buddy_schedule_adv_work();
    if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        int terminate_rc = ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        if (rc == 0) {
            rc = terminate_rc;
        }
    }
    return rc == 0 ? ESP_OK : buddy_ble_error(rc);
}

esp_err_t buddy_ble_send(const char *data, size_t length)
{
    uint16_t conn_handle;
    size_t fragment_size;
    size_t offset = 0;
    esp_err_t result = ESP_OK;

    if (data == NULL && length != 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_ble.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    conn_handle = s_ble.conn_handle;
    if (!s_ble.start_requested || conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_ble.secure) {
        xSemaphoreGive(s_ble.mutex);
        return ESP_ERR_INVALID_STATE;
    }

    fragment_size = buddy_ble_tx_fragment_size(ble_att_mtu(conn_handle));
    if (fragment_size == 0U) {
        xSemaphoreGive(s_ble.mutex);
        return ESP_ERR_INVALID_STATE;
    }

    while (offset < length) {
        struct os_mbuf *mbuf;
        size_t chunk = length - offset;
        int rc;

        if (chunk > fragment_size) {
            chunk = fragment_size;
        }
        memcpy(s_tx_scratch, data + offset, chunk);
        mbuf = ble_hs_mbuf_from_flat(s_tx_scratch, (uint16_t)chunk);
        if (mbuf == NULL) {
            result = ESP_ERR_NO_MEM;
            break;
        }
        rc = ble_gatts_notify_custom(conn_handle, s_tx_value_handle, mbuf);
        if (rc != 0) {
            result = buddy_ble_error(rc);
            break;
        }
        offset += chunk;
    }

    xSemaphoreGive(s_ble.mutex);
    return result;
}

bool buddy_ble_is_connected(void)
{
    bool connected;

    if (!s_ble.initialized) {
        return false;
    }
    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    connected = s_ble.conn_handle != BLE_HS_CONN_HANDLE_NONE;
    xSemaphoreGive(s_ble.mutex);
    return connected;
}

bool buddy_ble_is_encrypted(void)
{
    bool encrypted;

    if (!s_ble.initialized) {
        return false;
    }
    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    encrypted = s_ble.encrypted;
    xSemaphoreGive(s_ble.mutex);
    return encrypted;
}

esp_err_t buddy_ble_delete_bonds(void)
{
    uint16_t conn_handle;
    bool launch_host;
    bool new_request;
    bool reset_attempts;

    if (!s_ble.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    new_request = buddy_ble_delete_request_is_new(s_ble.delete_bonds_pending,
                                                   s_ble.delete_final_reported);
    reset_attempts = buddy_ble_delete_request_resets_attempts(s_ble.delete_bonds_pending,
                                                               s_ble.delete_final_reported);
    s_ble.delete_bonds_pending = true;
    ++s_ble.advertising_epoch;
    ++s_ble.connection_generation;
    s_ble.encrypted = false;
    s_ble.secure = false;
    if (reset_attempts) {
        s_ble.delete_bonds_result = 0;
        s_ble.delete_attempts = 0;
        s_ble.delete_final_reported = false;
    }
    if (new_request) {
        s_ble.delete_snapshot_ready = false;
        s_ble.delete_peer_count = 0;
    }
    conn_handle = s_ble.conn_handle != BLE_HS_CONN_HANDLE_NONE
                      ? s_ble.conn_handle
                      : s_ble.rejecting_conn_handle;
    launch_host = !s_ble.host_running;
    if (launch_host) {
        s_ble.host_running = true;
    }
    xSemaphoreGive(s_ble.mutex);
    ble_npl_callout_stop(&s_bond_retry_callout);

    if (launch_host) {
        nimble_port_freertos_init(buddy_host_task);
    }

    if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        int terminate_rc = ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        if (terminate_rc != 0) {
            buddy_schedule_bond_work();
        }
        return ESP_OK;
    }

    buddy_schedule_bond_work();
    return ESP_OK;
}

#endif
