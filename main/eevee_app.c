#include "eevee_app.h"
#include "eevee_config.h"
#include "eevee_client.h"
#include "eevee_badge_ui.h"
#include "eevee_font.h"
#include "eevee_prov.h"
#include "bsp_battery.h"
#include "bsp_display.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "eevee_app";

#define WIFI_MAX_RETRIES 5
#define SCREEN_OFF_TIMEOUT_SEC 60

typedef enum {
    CMD_SYNC_ALL = 1,
    CMD_POLL_STATUS,
    CMD_TASK_OFFSET_CHANGE,
    CMD_TASK_APPROVE,
    CMD_TASK_REJECT,
    CMD_RECORD_REPORT,
} eevee_cmd_type_t;

typedef struct {
    eevee_cmd_type_t type;
    int int_val;
} eevee_cmd_t;

static eevee_config_t s_cfg;
static eevee_profile_t s_profile;
static eevee_task_t s_curr_task;
static eevee_record_t s_curr_record;

static volatile bool s_wifi_connected = false;
static volatile bool s_screen_on = true;
static uint32_t s_idle_seconds = 0;
static int s_wifi_retry_count = 0;
static int s_curr_task_offset = 0;
static QueueHandle_t s_cmd_queue = NULL;
static TaskHandle_t s_worker_handle = NULL;
static TaskHandle_t s_console_handle = NULL;

static void eevee_app_enter_prov_mode(void);
static void eevee_app_exit_prov_mode(void);

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            if (eevee_config_has_wifi(&s_cfg)) {
                ESP_LOGI(TAG, "Wi-Fi started, connecting to '%s'...", s_cfg.wifi_ssid);
                s_wifi_retry_count = 0;
                esp_wifi_connect();
            } else {
                ESP_LOGI(TAG, "Wi-Fi credentials not configured. Device running in offline badge mode.");
                ESP_LOGI(TAG, "Type 'wifi <SSID> <PASSWORD>' in serial console to connect.");
            }
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            s_wifi_connected = false;
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_update_status(false, bsp_battery_soc());
                eevee_badge_ui_update_settings(s_cfg.wifi_ssid, false, s_cfg.base_url, s_cfg.pat_token[0] != '\0');
                bsp_lvgl_unlock();
            }
            if (eevee_config_has_wifi(&s_cfg) && s_wifi_retry_count < WIFI_MAX_RETRIES) {
                s_wifi_retry_count++;
                ESP_LOGW(TAG, "Wi-Fi disconnected, reconnecting (%d/%d)...",
                         s_wifi_retry_count, WIFI_MAX_RETRIES);
                vTaskDelay(pdMS_TO_TICKS(1500));
                esp_wifi_connect();
            } else {
                ESP_LOGW(TAG, "Wi-Fi offline. Press OK on badge to retry, or use console.");
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Wi-Fi Connected! IP: " IPSTR, IP2STR(&evt->ip_info.ip));
        s_wifi_connected = true;
        s_wifi_retry_count = 0;
        if (bsp_lvgl_lock(250)) {
            eevee_badge_ui_update_status(true, bsp_battery_soc());
            eevee_badge_ui_update_settings(s_cfg.wifi_ssid, true, s_cfg.base_url, s_cfg.pat_token[0] != '\0');
            eevee_badge_ui_show_toast("网络已连接", 0x10B981);
            bsp_lvgl_unlock();
        }
        // 连上网立即触发全量同步
        eevee_cmd_t cmd = { .type = CMD_SYNC_ALL };
        xQueueSend(s_cmd_queue, &cmd, 0);
    }
}

static void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&init_cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                         &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                         &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_cfg;
    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
    strncpy((char *)wifi_cfg.sta.ssid, s_cfg.wifi_ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, s_cfg.wifi_pass, sizeof(wifi_cfg.sta.password) - 1);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();
}

static void do_sync_all(void)
{
    if (!s_wifi_connected) return;
    ESP_LOGI(TAG, "Starting full sync from %s...", s_cfg.base_url);

    // 1. 同步个人卡面资料与名片地址 (/settings/profile)
    if (eevee_client_fetch_me(&s_cfg, &s_profile)) {
        if (bsp_lvgl_lock(250)) {
            eevee_badge_ui_update_profile(&s_profile);
            char web_url[128];
            eevee_config_get_web_url(&s_cfg, web_url, sizeof(web_url));
            char user_card_url[256];
            snprintf(user_card_url, sizeof(user_card_url), "%s/settings/profile", web_url);
            eevee_badge_ui_update_qr_url(user_card_url);
            bsp_lvgl_unlock();
        }
    }

    // 2. 同步待处理工作流
    if (eevee_client_fetch_tasks(&s_cfg, s_curr_task_offset, &s_curr_task)) {
        if (bsp_lvgl_lock(250)) {
            eevee_badge_ui_update_task(&s_curr_task);
            bsp_lvgl_unlock();
        }
    }

    // 3. 同步业务记录
    if (eevee_client_fetch_latest_record(&s_cfg, s_cfg.app_id, &s_curr_record)) {
        if (bsp_lvgl_lock(250)) {
            eevee_badge_ui_update_record(&s_curr_record);
            bsp_lvgl_unlock();
        }
    }

    if (bsp_lvgl_lock(250)) {
        eevee_badge_ui_update_status(s_wifi_connected, bsp_battery_soc());
        eevee_badge_ui_update_settings(s_cfg.wifi_ssid, s_wifi_connected, s_cfg.base_url, s_cfg.pat_token[0] != '\0');
        bsp_lvgl_unlock();
    }
}

static void do_poll_status(void)
{
    if (!s_wifi_connected) return;

    // 仅轮询待处理工作流程
    if (eevee_client_fetch_tasks(&s_cfg, s_curr_task_offset, &s_curr_task)) {
        if (bsp_lvgl_lock(250)) {
            eevee_badge_ui_update_task(&s_curr_task);
            eevee_badge_ui_update_status(s_wifi_connected, bsp_battery_soc());
            bsp_lvgl_unlock();
        }
    }
}

static void eevee_worker_task(void *pvParam)
{
    (void)pvParam;
    int tick_count = 0;

    for (;;) {
        eevee_cmd_t cmd;
        if (xQueueReceive(s_cmd_queue, &cmd, pdMS_TO_TICKS(1000)) == pdPASS) {
            switch (cmd.type) {
            case CMD_SYNC_ALL:
                do_sync_all();
                break;
            case CMD_POLL_STATUS:
                do_poll_status();
                break;
            case CMD_TASK_OFFSET_CHANGE:
                s_curr_task_offset = cmd.int_val;
                if (s_wifi_connected && eevee_client_fetch_tasks(&s_cfg, s_curr_task_offset, &s_curr_task)) {
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_update_task(&s_curr_task);
                        bsp_lvgl_unlock();
                    }
                }
                break;
            case CMD_TASK_APPROVE:
                if (!s_wifi_connected) {
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast("离线无法审批", 0xEF4444);
                        bsp_lvgl_unlock();
                    }
                    break;
                }
                if (s_curr_task.has_task && s_curr_task.primary_action_id[0] != '\0') {
                    bool ok = eevee_client_execute_task_action(
                        &s_cfg, s_curr_task.app_id, s_curr_task.record_id,
                        s_curr_task.primary_action_id, "工牌物理按键同意");
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast(ok ? "已同意审批" : "审批失败",
                                                  ok ? 0x10B981 : 0xEF4444);
                        bsp_lvgl_unlock();
                    }
                    if (ok) {
                        vTaskDelay(pdMS_TO_TICKS(500));
                        do_poll_status();
                    }
                }
                break;
            case CMD_TASK_REJECT:
                if (!s_wifi_connected) {
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast("离线无法驳回", 0xEF4444);
                        bsp_lvgl_unlock();
                    }
                    break;
                }
                if (s_curr_task.has_task) {
                    const char *action = s_curr_task.reject_action_id[0] ?
                                         s_curr_task.reject_action_id : "act_reject";
                    bool ok = eevee_client_execute_task_action(
                        &s_cfg, s_curr_task.app_id, s_curr_task.record_id,
                        action, "工牌物理按键驳回");
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast(ok ? "已驳回修改" : "驳回失败",
                                                  ok ? 0xF59E0B : 0xEF4444);
                        bsp_lvgl_unlock();
                    }
                    if (ok) {
                        vTaskDelay(pdMS_TO_TICKS(500));
                        do_poll_status();
                    }
                }
                break;
            case CMD_RECORD_REPORT:
                if (!s_wifi_connected) {
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast("离线无法打卡", 0xEF4444);
                        bsp_lvgl_unlock();
                    }
                    break;
                }
                {
                    int mv = bsp_battery_mv();
                    float volt = (mv > 2500 && mv < 4500) ? (mv / 1000.0f) : 3.84f;
                    bool ok = eevee_client_report_record(&s_cfg, s_cfg.app_id, 26.0f, 60.0f, volt);
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast(ok ? "打卡上报成功" : "上报失败",
                                                  ok ? 0x10B981 : 0xEF4444);
                        bsp_lvgl_unlock();
                    }
                    if (ok) {
                        vTaskDelay(pdMS_TO_TICKS(500));
                        if (eevee_client_fetch_latest_record(&s_cfg, s_cfg.app_id, &s_curr_record)) {
                            if (bsp_lvgl_lock(250)) {
                                eevee_badge_ui_update_record(&s_curr_record);
                                bsp_lvgl_unlock();
                            }
                        }
                    }
                }
                break;
            default:
                break;
            }
        }

        // 仅在亮屏且非配网模态时计算超时息屏
        if (s_screen_on && !eevee_badge_ui_is_prov_view_visible()) {
            s_idle_seconds++;
            if (s_idle_seconds >= SCREEN_OFF_TIMEOUT_SEC) {
                s_screen_on = false;
                bsp_display_backlight(0);
                ESP_LOGI(TAG, "Screen OFF (idle %d s). Backlight off and periodic polling stopped.",
                         SCREEN_OFF_TIMEOUT_SEC);
            }
        }

        // 仅在亮屏状态下更新电量状态栏与周期轮询待办流程
        if (s_screen_on) {
            if (bsp_lvgl_lock(100)) {
                eevee_badge_ui_update_status(s_wifi_connected, bsp_battery_soc());
                bsp_lvgl_unlock();
            }

            tick_count++;
            if (tick_count >= s_cfg.poll_interval_sec) {
                tick_count = 0;
                do_poll_status();
            }
        }
    }
}

static void eevee_console_task(void *pvParam)
{
    (void)pvParam;
    char line[256];

    printf("\n");
    printf("===================================================\n");
    printf("   Eevee Smart Badge Console Ready!                \n");
    printf("   Commands available:                             \n");
    printf("     wifi <SSID> <PASSWORD>                        \n");
    printf("     server <URL> [PAT_TOKEN]                      \n");
    printf("     sync                                          \n");
    printf("     status                                        \n");
    printf("===================================================\n\n");

    while (1) {
        if (fgets(line, sizeof(line), stdin)) {
            char *p = line;
            while (*p) {
                if (*p == '\r' || *p == '\n') *p = '\0';
                p++;
            }
            if (line[0] == '\0') continue;

            if (strncmp(line, "wifi ", 5) == 0) {
                char ssid[33] = {0};
                char pass[65] = {0};
                int cnt = sscanf(line + 5, "%32s %64s", ssid, pass);
                if (cnt >= 1) {
                    strncpy(s_cfg.wifi_ssid, ssid, sizeof(s_cfg.wifi_ssid) - 1);
                    if (cnt >= 2) {
                        strncpy(s_cfg.wifi_pass, pass, sizeof(s_cfg.wifi_pass) - 1);
                    } else {
                        s_cfg.wifi_pass[0] = '\0';
                    }
                    eevee_config_save(&s_cfg);
                    printf("[CONSOLE] Wi-Fi credentials saved: SSID='%s'. Connecting...\n", s_cfg.wifi_ssid);

                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast("配置已更新,连接中", 0x2563EB);
                        eevee_badge_ui_update_settings(s_cfg.wifi_ssid, false, s_cfg.base_url, s_cfg.pat_token[0] != '\0');
                        bsp_lvgl_unlock();
                    }

                    wifi_config_t wifi_cfg;
                    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
                    strncpy((char *)wifi_cfg.sta.ssid, s_cfg.wifi_ssid, sizeof(wifi_cfg.sta.ssid) - 1);
                    strncpy((char *)wifi_cfg.sta.password, s_cfg.wifi_pass, sizeof(wifi_cfg.sta.password) - 1);
                    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
                    s_wifi_retry_count = 0;
                    esp_wifi_disconnect();
                    esp_wifi_connect();
                }
            } else if (strncmp(line, "server ", 7) == 0) {
                char url[128] = {0};
                char pat[64] = {0};
                int parsed = sscanf(line + 7, "%127s %63s", url, pat);
                if (parsed >= 1) {
                    strncpy(s_cfg.base_url, url, sizeof(s_cfg.base_url) - 1);
                    if (parsed >= 2) {
                        strncpy(s_cfg.pat_token, pat, sizeof(s_cfg.pat_token) - 1);
                    }
                    eevee_config_save(&s_cfg);
                    printf("[CONSOLE] Server saved: BaseURL='%s', PAT='%s'\n", s_cfg.base_url, s_cfg.pat_token);
                    if (bsp_lvgl_lock(250)) {
                        eevee_badge_ui_show_toast("服务端地址已更新", 0x2563EB);
                        eevee_badge_ui_update_settings(s_cfg.wifi_ssid, s_wifi_connected, s_cfg.base_url, s_cfg.pat_token[0] != '\0');
                        bsp_lvgl_unlock();
                    }
                    if (s_wifi_connected) {
                        eevee_cmd_t cmd = { .type = CMD_SYNC_ALL };
                        xQueueSend(s_cmd_queue, &cmd, 0);
                    }
                }
            } else if (strcmp(line, "sync") == 0) {
                printf("[CONSOLE] Requesting full sync...\n");
                if (s_wifi_connected) {
                    eevee_cmd_t cmd = { .type = CMD_SYNC_ALL };
                    xQueueSend(s_cmd_queue, &cmd, 0);
                } else {
                    printf("[CONSOLE] Error: Wi-Fi not connected, cannot sync.\n");
                }
            } else if (strcmp(line, "prov") == 0) {
                printf("[CONSOLE] Starting SoftAP Web Provisioning...\n");
                eevee_app_enter_prov_mode();
            } else if (strcmp(line, "status") == 0) {
                printf("[CONSOLE] === Eevee Badge Status ===\n");
                printf("  Wi-Fi SSID:    %s\n", s_cfg.wifi_ssid[0] ? s_cfg.wifi_ssid : "<NOT SET>");
                printf("  Wi-Fi Status:  %s\n", s_wifi_connected ? "ONLINE" : "OFFLINE");
                printf("  Server URL:    %s\n", s_cfg.base_url);
                printf("  PAT Token:     %s\n", s_cfg.pat_token[0] ? s_cfg.pat_token : "<NOT SET>");
                printf("  Free Heap:     %lu bytes\n", (unsigned long)esp_get_free_heap_size());
                printf("  Battery SOC:   %d%%\n", bsp_battery_soc());
                printf("===================================\n");
            } else if (strcmp(line, "screen off") == 0) {
                s_screen_on = false;
                bsp_display_backlight(0);
                printf("[CONSOLE] Screen turned OFF. Polling stopped.\n");
            } else if (strcmp(line, "screen on") == 0) {
                s_screen_on = true;
                s_idle_seconds = 0;
                bsp_display_backlight(100);
                printf("[CONSOLE] Screen turned ON. Triggering poll...\n");
                eevee_cmd_t cmd = { .type = CMD_POLL_STATUS };
                xQueueSend(s_cmd_queue, &cmd, 0);
            } else if (strcmp(line, "help") == 0) {
                printf("[CONSOLE] Available commands:\n");
                printf("  wifi <ssid> <pass>   - Set Wi-Fi and connect\n");
                printf("  server <url> [token] - Set Base URL and PAT Token\n");
                printf("  prov                 - Start SoftAP Web Provisioning\n");
                printf("  sync                 - Trigger full sync\n");
                printf("  screen on/off        - Wake up or turn off display\n");
                printf("  status               - Print current status and memory\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void on_prov_success(const eevee_config_t *new_cfg, bool success)
{
    if (success && new_cfg) {
        s_cfg = *new_cfg;
        ESP_LOGI(TAG, "Provisioning succeeded with SSID='%s'", s_cfg.wifi_ssid);
        if (bsp_lvgl_lock(250)) {
            eevee_badge_ui_show_prov_view(false, NULL, NULL);
            eevee_badge_ui_show_toast("配网成功,已连接", 0x10B981);
            eevee_badge_ui_update_settings(s_cfg.wifi_ssid, true, s_cfg.base_url, s_cfg.pat_token[0] != '\0');
            bsp_lvgl_unlock();
        }
        eevee_cmd_t cmd = { .type = CMD_SYNC_ALL };
        xQueueSend(s_cmd_queue, &cmd, 0);
    }
}

static void eevee_app_enter_prov_mode(void)
{
    ESP_LOGI(TAG, "Entering SoftAP Web Provisioning mode...");
    eevee_prov_start(&s_cfg, on_prov_success);
    if (bsp_lvgl_lock(250)) {
        eevee_badge_ui_show_prov_view(true, "Eevee-Badge-Setup", "192.168.4.1");
        bsp_lvgl_unlock();
    }
}

static void eevee_app_exit_prov_mode(void)
{
    ESP_LOGI(TAG, "Exiting Provisioning mode...");
    eevee_prov_stop();
    eevee_config_init(&s_cfg);
    if (bsp_lvgl_lock(250)) {
        eevee_badge_ui_show_prov_view(false, NULL, NULL);
        eevee_badge_ui_show_toast("已退出配网模式", 0x64748B);
        bsp_lvgl_unlock();
    }
    if (eevee_config_has_wifi(&s_cfg) && !s_wifi_connected) {
        wifi_config_t wifi_cfg;
        memset(&wifi_cfg, 0, sizeof(wifi_cfg));
        strncpy((char *)wifi_cfg.sta.ssid, s_cfg.wifi_ssid, sizeof(wifi_cfg.sta.ssid) - 1);
        strncpy((char *)wifi_cfg.sta.password, s_cfg.wifi_pass, sizeof(wifi_cfg.sta.password) - 1);
        esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
        s_wifi_retry_count = 0;
        esp_wifi_connect();
    }
}

void eevee_app_start(void)
{
    ESP_LOGI(TAG, "Starting Eevee Smart Badge Application...");

    eevee_config_init(&s_cfg);
    if (bsp_lvgl_lock(1000)) {
        eevee_font_init();
        eevee_badge_ui_init();
        char web_url[128];
        eevee_config_get_web_url(&s_cfg, web_url, sizeof(web_url));
        char user_card_url[256];
        snprintf(user_card_url, sizeof(user_card_url), "%s/settings/profile", web_url);
        eevee_badge_ui_update_qr_url(user_card_url);
        eevee_badge_ui_update_status(false, bsp_battery_soc());
        eevee_badge_ui_update_settings(s_cfg.wifi_ssid, false, s_cfg.base_url, s_cfg.pat_token[0] != '\0');
        bsp_lvgl_unlock();
    }

    s_cmd_queue = xQueueCreate(10, sizeof(eevee_cmd_t));
    xTaskCreate(eevee_worker_task, "eevee_worker", 4096, NULL, 5, &s_worker_handle);
    xTaskCreate(eevee_console_task, "eevee_console", 3072, NULL, 4, &s_console_handle);

    wifi_init_sta();

    if (!eevee_config_has_wifi(&s_cfg)) {
        ESP_LOGI(TAG, "No Wi-Fi configured. Entering SoftAP Web Provisioning mode...");
        eevee_app_enter_prov_mode();
    }
}

void eevee_app_handle_button(bsp_btn_t btn, bsp_btn_ev_t event)
{
    // 0. 息屏唤醒：如果处于息屏状态，任意键短按或长按均唤醒屏幕并静默更新数据
    if (!s_screen_on) {
        if (event == BSP_BTN_CLICK || event == BSP_BTN_LONG) {
            s_screen_on = true;
            s_idle_seconds = 0;
            bsp_display_backlight(100);
            ESP_LOGI(TAG, "Screen ON (button press). Polling workflow tasks in background...");
            // 开屏立即在后台静默轮询最新流程
            eevee_cmd_t cmd = { .type = CMD_POLL_STATUS };
            xQueueSend(s_cmd_queue, &cmd, 0);
        }
        return; // 唤醒动作拦截本次输入，防止黑暗中误触
    }

    // 活跃输入：重置无操作空闲计时器
    s_idle_seconds = 0;

    // 配网视图处于前台时，短按或长按 OK 都能退出配网
    if (eevee_badge_ui_is_prov_view_visible()) {
        if (btn == BSP_BTN_OK && (event == BSP_BTN_CLICK || event == BSP_BTN_LONG)) {
            eevee_app_exit_prov_mode();
        }
        return;
    }

    eevee_ui_page_t page = eevee_badge_ui_get_page();

    // 1. 长按事件处理
    if (event == BSP_BTN_LONG) {
        if (btn == BSP_BTN_UP) {
            // 任意页面长按 UP：手动立即息屏
            s_screen_on = false;
            bsp_display_backlight(0);
            ESP_LOGI(TAG, "Screen OFF (manual long press UP). Polling stopped.");
            return;
        } else if (btn == BSP_BTN_OK) {
            if (page == EEVEE_PAGE_TASKS) {
                // 待办页面长按 OK：执行驳回
                eevee_cmd_t cmd = { .type = CMD_TASK_REJECT };
                xQueueSend(s_cmd_queue, &cmd, 0);
                return;
            } else if (page == EEVEE_PAGE_BADGE) {
                // 工牌主屏长按 OK：触发全量刷新同步
                if (bsp_lvgl_lock(250)) {
                    eevee_badge_ui_show_toast("正在全量同步...", 0x2563EB);
                    bsp_lvgl_unlock();
                }
                eevee_cmd_t cmd = { .type = CMD_SYNC_ALL };
                xQueueSend(s_cmd_queue, &cmd, 0);
                return;
            }
        } else if (btn == BSP_BTN_DOWN && page == EEVEE_PAGE_BADGE) {
            // 工牌主屏长按 DOWN：快捷启动热点配网模式
            eevee_app_enter_prov_mode();
            return;
        }
        return;
    }

    // 2. 短按事件处理
    if (event != BSP_BTN_CLICK) return;

    if (page == EEVEE_PAGE_BADGE) {
        if (btn == BSP_BTN_DOWN) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_TASKS);
                bsp_lvgl_unlock();
            }
        } else if (btn == BSP_BTN_UP) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_SETTINGS);
                bsp_lvgl_unlock();
            }
        } else if (btn == BSP_BTN_OK) {
            if (s_wifi_connected) {
                if (bsp_lvgl_lock(250)) {
                    eevee_badge_ui_show_toast("正在同步...", 0x2563EB);
                    bsp_lvgl_unlock();
                }
                eevee_cmd_t cmd = { .type = CMD_SYNC_ALL };
                xQueueSend(s_cmd_queue, &cmd, 0);
            } else if (eevee_config_has_wifi(&s_cfg)) {
                if (bsp_lvgl_lock(250)) {
                    eevee_badge_ui_show_toast("正在重连网络...", 0x2563EB);
                    bsp_lvgl_unlock();
                }
                s_wifi_retry_count = 0;
                esp_wifi_connect();
            } else {
                // 未配网时按 OK 自动拉起配网
                eevee_app_enter_prov_mode();
            }
        }
    } else if (page == EEVEE_PAGE_TASKS) {
        if (btn == BSP_BTN_OK) {
            // 短按 OK：执行同意
            eevee_cmd_t cmd = { .type = CMD_TASK_APPROVE };
            xQueueSend(s_cmd_queue, &cmd, 0);
        } else if (btn == BSP_BTN_UP) {
            if (s_curr_task_offset > 0) {
                eevee_cmd_t cmd = {
                    .type = CMD_TASK_OFFSET_CHANGE,
                    .int_val = s_curr_task_offset - 1
                };
                xQueueSend(s_cmd_queue, &cmd, 0);
            } else {
                if (bsp_lvgl_lock(250)) {
                    eevee_badge_ui_set_page(EEVEE_PAGE_BADGE);
                    bsp_lvgl_unlock();
                }
            }
        } else if (btn == BSP_BTN_DOWN) {
            if (s_curr_task.total_count > s_curr_task_offset + 1) {
                eevee_cmd_t cmd = {
                    .type = CMD_TASK_OFFSET_CHANGE,
                    .int_val = s_curr_task_offset + 1
                };
                xQueueSend(s_cmd_queue, &cmd, 0);
            } else {
                if (bsp_lvgl_lock(250)) {
                    eevee_badge_ui_set_page(EEVEE_PAGE_RECORDS);
                    bsp_lvgl_unlock();
                }
            }
        }
    } else if (page == EEVEE_PAGE_RECORDS) {
        if (btn == BSP_BTN_OK) {
            eevee_cmd_t cmd = { .type = CMD_RECORD_REPORT };
            xQueueSend(s_cmd_queue, &cmd, 0);
        } else if (btn == BSP_BTN_UP) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_TASKS);
                bsp_lvgl_unlock();
            }
        } else if (btn == BSP_BTN_DOWN) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_QR);
                bsp_lvgl_unlock();
            }
        }
    } else if (page == EEVEE_PAGE_QR) {
        if (btn == BSP_BTN_UP) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_RECORDS);
                bsp_lvgl_unlock();
            }
        } else if (btn == BSP_BTN_DOWN) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_SETTINGS);
                bsp_lvgl_unlock();
            }
        } else if (btn == BSP_BTN_OK) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_SETTINGS);
                bsp_lvgl_unlock();
            }
        }
    } else if (page == EEVEE_PAGE_SETTINGS) {
        if (btn == BSP_BTN_UP) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_QR);
                bsp_lvgl_unlock();
            }
        } else if (btn == BSP_BTN_DOWN) {
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_set_page(EEVEE_PAGE_BADGE);
                bsp_lvgl_unlock();
            }
        } else if (btn == BSP_BTN_OK) {
            // 设置页面点击 OK：明确启动重新配网功能
            ESP_LOGI(TAG, "User clicked OK on Settings page to re-provision!");
            if (bsp_lvgl_lock(250)) {
                eevee_badge_ui_show_toast("正在开启配网热点", 0x2563EB);
                bsp_lvgl_unlock();
            }
            eevee_app_enter_prov_mode();
        }
    }
}
