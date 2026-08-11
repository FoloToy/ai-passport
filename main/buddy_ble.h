#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define BUDDY_NUS_SERVICE_UUID16_FIRST 0x0001
#define BUDDY_NUS_RX_UUID16_FIRST 0x0002
#define BUDDY_NUS_TX_UUID16_FIRST 0x0003

#define BUDDY_NUS_SERVICE_UUID_BYTES                                                       \
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, \
        0x40, 0x6e
#define BUDDY_NUS_RX_UUID_BYTES                                                            \
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, \
        0x40, 0x6e
#define BUDDY_NUS_TX_UUID_BYTES                                                            \
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, \
        0x40, 0x6e

#define BUDDY_BLE_TX_CHUNK_MAX 244U

typedef enum {
    BUDDY_BLE_EVENT_CONNECTED,
    BUDDY_BLE_EVENT_DISCONNECTED,
    BUDDY_BLE_EVENT_PASSKEY,
    BUDDY_BLE_EVENT_ENCRYPTION,
    BUDDY_BLE_EVENT_RX_LINE,
} buddy_ble_event_type_t;

typedef struct {
    buddy_ble_event_type_t type;
    union {
        struct {
            uint16_t conn_handle;
        } connected;
        struct {
            int reason;
        } disconnected;
        struct {
            uint32_t value;
        } passkey;
        struct {
            bool encrypted;
            bool authenticated;
            bool bonded;
            int status;
        } encryption;
        struct {
            const char *data;
            size_t length;
        } rx_line;
    } data;
} buddy_ble_event_t;

/* Callbacks run on the NimBLE host task. The event and RX line storage remain
 * valid only until the callback returns; consumers should copy or queue them. */
typedef void (*buddy_ble_event_cb_t)(const buddy_ble_event_t *event, void *context);

typedef struct {
    buddy_ble_event_cb_t event_cb;
    void *event_context;
} buddy_ble_config_t;

size_t buddy_ble_tx_fragment_size(uint16_t mtu);
bool buddy_ble_link_is_secure(bool encrypted, bool authenticated, bool bonded, uint8_t key_size);

esp_err_t buddy_ble_init(const buddy_ble_config_t *config);
esp_err_t buddy_ble_start(void);
esp_err_t buddy_ble_stop(void);
esp_err_t buddy_ble_send(const char *data, size_t length);
bool buddy_ble_is_connected(void);
bool buddy_ble_is_encrypted(void);
/* With a running host, ESP_OK means the deletion request was accepted.
 * Advertising remains suppressed until every stored bond has been deleted. */
esp_err_t buddy_ble_delete_bonds(void);
