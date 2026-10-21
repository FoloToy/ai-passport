// main/workbuddy_companion.h
// WorkBuddy 硬件伴侣（独立应用，非 demo 测试页）
//
// 这是 AI PASSPORT 的"真应用"：作为 WorkBuddy 的状态屏 + 审批遥控器。
// 传输层是 USB-CDC 串口（设备插 USB 即 COM4，零配对、零扫描、不断连），
// 协议为换行分隔 JSON。大脑永远是 PC 侧的 WorkBuddy；设备不跑 STT/LLM/TTS。
//
// UI / 交互 / 状态机 / 串口协议均为全新设计（满足二次开发强制重新设计规则）。
#pragma once

#include "esp_err.h"
#include "bsp_button.h"

// 启动慢服务（NimBLE + GATT 注册 + 广播）。不持 LVGL 锁。
esp_err_t workbuddy_companion_start(void);
// 停止并释放慢服务。
esp_err_t workbuddy_companion_stop(void);
// 持 LVGL 锁创建并载入应用页面。
void     workbuddy_companion_enter(void);
// 持 LVGL 锁删除页面并清空对象指针。
void     workbuddy_companion_exit(void);
// 按键事件（来自 button 组件回调，已入队）。
void     workbuddy_companion_key(bsp_btn_t btn, bsp_btn_ev_t ev);
