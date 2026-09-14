// main/demo.h —— 每个演示页实现的统一接口。
// 新增演示页 = 实现 enter/exit/key，慢服务按需实现 start/stop，再注册到 DEMOS[]。
#pragma once

#include "bsp_button.h"

typedef struct {
    const char *name;
    void (*enter)(void);                          // 持 LVGL 锁创建并载入页面
    void (*exit)(void);                           // lifecycle stop 成功后,持 LVGL 锁删除页面
    void (*key)(bsp_btn_t btn, bsp_btn_ev_t ev);  // lifecycle task 调用;函数自行缩短 LVGL 锁范围
    esp_err_t (*start)(void);                     // 可选:页面创建后,不持 LVGL 锁启动慢服务
    esp_err_t (*stop)(void);                      // 可选:删页面前,不持 LVGL 锁停止 producer
} demo_entry_t;

// 各演示页(定义在各自的 .c 里)
void demo_display_enter(void); void demo_display_exit(void);
void demo_display_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_button_enter(void);  void demo_button_exit(void);
void demo_button_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_audio_enter(void);   void demo_audio_exit(void);
void demo_audio_key(bsp_btn_t btn, bsp_btn_ev_t ev);
esp_err_t demo_audio_start(void); esp_err_t demo_audio_stop(void);

void demo_battery_enter(void); void demo_battery_exit(void);
void demo_battery_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_wifi_enter(void);    void demo_wifi_exit(void);
void demo_wifi_key(bsp_btn_t btn, bsp_btn_ev_t ev);
esp_err_t demo_wifi_start(void); esp_err_t demo_wifi_stop(void);

void demo_ble_enter(void);     void demo_ble_exit(void);
void demo_ble_key(bsp_btn_t btn, bsp_btn_ev_t ev);
esp_err_t demo_ble_start(void); esp_err_t demo_ble_stop(void);

void demo_low_power_enter(void); void demo_low_power_exit(void);
void demo_low_power_key(bsp_btn_t btn, bsp_btn_ev_t ev);
esp_err_t demo_low_power_start(void); esp_err_t demo_low_power_stop(void);

void demo_jinqiangua_enter(void); void demo_jinqiangua_exit(void);
void demo_jinqiangua_key(bsp_btn_t btn, bsp_btn_ev_t ev);

