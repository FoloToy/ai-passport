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
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "buddy_line.h"
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

#ifndef BUDDY_BLE_HOST_TEST

#define BUDDY_BLE_DEVICE_NAME_SIZE 14U

typedef struct {
    SemaphoreHandle_t mutex;
    buddy_ble_event_cb_t event_cb;
    void *event_context;
    buddy_line_buffer_t rx;
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
    int delete_bonds_result;
    int termination_failure_reason;
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

static const ble_uuid128_t s_nus_service_uuid = BLE_UUID128_INIT(BUDDY_NUS_SERVICE_UUID_BYTES);
static const ble_uuid128_t s_nus_rx_uuid = BLE_UUID128_INIT(BUDDY_NUS_RX_UUID_BYTES);
static const ble_uuid128_t s_nus_tx_uuid = BLE_UUID128_INIT(BUDDY_NUS_TX_UUID_BYTES);

static int buddy_gap_event(struct ble_gap_event *event, void *context);
static int buddy_gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *context, void *arg);

static void buddy_schedule_bond_work(void)
{
    if (!ble_npl_event_is_queued(&s_delete_bonds_event)) {
        ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_delete_bonds_event);
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

static void buddy_rx_line(const char *line, size_t length, void *context)
{
    const buddy_ble_event_t event = {
        .type = BUDDY_BLE_EVENT_RX_LINE,
        .data.rx_line = {
            .data = line,
            .length = length,
        },
    };

    (void)context;
    buddy_emit(&event);
}

static int buddy_gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *context, void *arg)
{
    struct os_mbuf *mbuf;
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
    xSemaphoreGive(s_ble.mutex);
    if (!accept_write) {
        return BLE_ATT_ERR_INSUFFICIENT_ENC;
    }

    for (mbuf = context->om; mbuf != NULL; mbuf = SLIST_NEXT(mbuf, om_next)) {
        (void)buddy_line_push(&s_ble.rx, mbuf->om_data, mbuf->om_len, buddy_rx_line, NULL);
    }
    return 0;
}

static int buddy_start_advertising(void)
{
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields response_fields;
    struct ble_gap_adv_params parameters;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = (ble_uuid128_t *)&s_nus_service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        return rc;
    }

    memset(&response_fields, 0, sizeof(response_fields));
    response_fields.name = (uint8_t *)s_device_name;
    response_fields.name_len = strlen(s_device_name);
    response_fields.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&response_fields);
    if (rc != 0) {
        return rc;
    }

    memset(&parameters, 0, sizeof(parameters));
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    if (!s_ble.start_requested || !s_ble.host_synced || s_ble.delete_bonds_pending ||
        s_ble.conn_handle != BLE_HS_CONN_HANDLE_NONE ||
        s_ble.rejecting_conn_handle != BLE_HS_CONN_HANDLE_NONE || ble_gap_adv_active()) {
        xSemaphoreGive(s_ble.mutex);
        return 0;
    }
    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &parameters, buddy_gap_event,
                           NULL);
    xSemaphoreGive(s_ble.mutex);
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

static int buddy_delete_all_peers(void)
{
    ble_addr_t peers[CONFIG_BT_NIMBLE_MAX_BONDS];
    int peer_count = 0;
    int index;
    int first_error = 0;
    int rc;

    rc = ble_store_util_bonded_peers(peers, &peer_count, CONFIG_BT_NIMBLE_MAX_BONDS);
    if (rc != 0) {
        return rc;
    }
    for (index = 0; index < peer_count; ++index) {
        rc = ble_store_util_delete_peer(&peers[index]);
        if (rc != 0 && first_error == 0) {
            first_error = rc;
        }
    }
    return first_error;
}

static void buddy_finish_bond_deletion(struct ble_npl_event *event)
{
    uint16_t conn_handle;
    bool reset_scheduled;
    bool advertise;
    int rc;

    (void)event;

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    conn_handle = s_ble.conn_handle != BLE_HS_CONN_HANDLE_NONE
                      ? s_ble.conn_handle
                      : s_ble.rejecting_conn_handle;
    reset_scheduled = s_ble.reset_scheduled;
    xSemaphoreGive(s_ble.mutex);

    if (conn_handle != BLE_HS_CONN_HANDLE_NONE || reset_scheduled) {
        if (!reset_scheduled) {
            rc = ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            if (rc != 0 && rc != BLE_HS_EALREADY) {
                buddy_schedule_termination_recovery(rc);
            }
        }
        return;
    }

    rc = buddy_delete_all_peers();

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    s_ble.delete_bonds_result = rc;
    if (rc == 0) {
        s_ble.delete_bonds_pending = false;
    }
    advertise = rc == 0 && s_ble.start_requested && s_ble.host_synced &&
                s_ble.conn_handle == BLE_HS_CONN_HANDLE_NONE;
    xSemaphoreGive(s_ble.mutex);

    if (rc != 0) {
        ESP_LOGE(s_tag, "Failed to delete BLE bonds: %d", rc);
    }
    if (advertise) {
        rc = buddy_start_advertising();
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
        s_ble.encrypted = false;
        s_ble.secure = false;
        buddy_line_init(&s_ble.rx);
        emit_disconnect = true;
    }
    if (s_ble.rejecting_conn_handle == conn_handle) {
        s_ble.rejecting_conn_handle = BLE_HS_CONN_HANDLE_NONE;
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
        int rc = buddy_start_advertising();
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
            return buddy_start_advertising();
        }

        xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
        if (!s_ble.start_requested || s_ble.delete_bonds_pending ||
            s_ble.conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            int rc;

            s_ble.rejecting_conn_handle = event->connect.conn_handle;
            xSemaphoreGive(s_ble.mutex);
            rc = ble_gap_terminate(event->connect.conn_handle, BLE_ERR_CONN_LIMIT);
            if (rc != 0) {
                buddy_schedule_termination_recovery(rc);
            }
            return rc;
        }
        s_ble.conn_handle = event->connect.conn_handle;
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
        return buddy_start_advertising();

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
    s_ble.rejecting_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    s_ble.encrypted = false;
    s_ble.secure = false;
    s_ble.host_synced = false;
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
    delete_bonds = s_ble.delete_bonds_pending;
    xSemaphoreGive(s_ble.mutex);

    if (delete_bonds) {
        buddy_schedule_bond_work();
        return;
    }

    rc = buddy_start_advertising();
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

esp_err_t buddy_ble_init(const buddy_ble_config_t *config)
{
    uint8_t mac[6];
    esp_err_t err;
    int rc;

    if (config == NULL || config->event_cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_ble.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ble.mutex = xSemaphoreCreateMutex();
    if (s_ble.mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    s_ble.event_cb = config->event_cb;
    s_ble.event_context = config->event_context;
    buddy_line_init(&s_ble.rx);
    ble_npl_event_init(&s_delete_bonds_event, buddy_finish_bond_deletion, NULL);
    ble_npl_event_init(&s_termination_recovery_event, buddy_recover_termination, NULL);

    err = esp_read_mac(mac, ESP_MAC_BT);
    if (err != ESP_OK) {
        vSemaphoreDelete(s_ble.mutex);
        s_ble.mutex = NULL;
        return err;
    }
    (void)snprintf(s_device_name, sizeof(s_device_name), "Claude-%02X%02X%02X", mac[3], mac[4],
                   mac[5]);

    err = nimble_port_init();
    if (err != ESP_OK) {
        vSemaphoreDelete(s_ble.mutex);
        s_ble.mutex = NULL;
        return err;
    }

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
    if (rc != 0) {
        (void)nimble_port_deinit();
        vSemaphoreDelete(s_ble.mutex);
        s_ble.mutex = NULL;
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
        int rc = buddy_start_advertising();
        return rc == 0 ? ESP_OK : buddy_ble_error(rc);
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
    s_ble.encrypted = false;
    s_ble.secure = false;
    conn_handle = s_ble.conn_handle != BLE_HS_CONN_HANDLE_NONE
                      ? s_ble.conn_handle
                      : s_ble.rejecting_conn_handle;
    xSemaphoreGive(s_ble.mutex);

    if (ble_gap_adv_active()) {
        rc = ble_gap_adv_stop();
    }
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
    bool host_running;
    int delete_result;
    int rc = 0;

    if (!s_ble.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    s_ble.delete_bonds_pending = true;
    s_ble.encrypted = false;
    s_ble.secure = false;
    s_ble.delete_bonds_result = 0;
    conn_handle = s_ble.conn_handle != BLE_HS_CONN_HANDLE_NONE
                      ? s_ble.conn_handle
                      : s_ble.rejecting_conn_handle;
    host_running = s_ble.host_running;
    xSemaphoreGive(s_ble.mutex);

    if (ble_gap_adv_active()) {
        rc = ble_gap_adv_stop();
    }

    if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        int terminate_rc = ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        if (rc == 0) {
            rc = terminate_rc;
        }
        if (terminate_rc != 0 && host_running) {
            buddy_schedule_bond_work();
        }
        return rc == 0 ? ESP_OK : buddy_ble_error(rc);
    }

    if (host_running) {
        buddy_schedule_bond_work();
        return rc == 0 ? ESP_OK : buddy_ble_error(rc);
    }

    buddy_finish_bond_deletion(NULL);
    xSemaphoreTake(s_ble.mutex, portMAX_DELAY);
    delete_result = s_ble.delete_bonds_result;
    xSemaphoreGive(s_ble.mutex);
    if (rc == 0) {
        rc = delete_result;
    }
    return rc == 0 ? ESP_OK : buddy_ble_error(rc);
}

#endif
