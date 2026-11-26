// main/kl8_result.h —— 快乐8 开奖结果:从福彩官网拉取最近若干期并缓存。
// 快乐选号是 1-80 选号,与福彩「快乐8」同池;每期开出 20 个号。
// 与 ssq_result 同构:独立任务拉取,完成后回调通知界面。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define KL8_ISSUE_MAX 10
#define KL8_NUM_COUNT 20

typedef struct {
    char code[16];              // 期号,如 "2026314"
    char date[16];              // 开奖日期,如 "2026-11-24"
    uint8_t nums[KL8_NUM_COUNT]; // 20 个开奖号,升序
} kl8_issue_t;

typedef enum {
    KL8_STATE_IDLE = 0,
    KL8_STATE_LOADING,
    KL8_STATE_OK,
    KL8_STATE_NO_WIFI,
    KL8_STATE_FAIL,
} kl8_state_t;

// 初始化并创建拉取任务。on_update 在数据变化后从拉取任务上下文调用,
// 实现方需自行加锁(UI 侧用 bsp_lvgl_lock)。
void kl8_result_init(void (*on_update)(void));

// 请求一次拉取(非阻塞,重复请求会合并)。
void kl8_result_request(void);

kl8_state_t kl8_result_state(void);
int kl8_result_count(void);
const kl8_issue_t *kl8_result_at(int index);

// 判断某个号码是否在本期开奖号里(UI 高亮自选号用)。
bool kl8_issue_has(const kl8_issue_t *issue, uint8_t num);
