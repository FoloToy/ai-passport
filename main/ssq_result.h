// main/ssq_result.h —— 双色球开奖结果:从福彩官网拉取最近若干期并缓存。
// 拉取在独立任务里跑,不阻塞 UI;完成后通过回调通知界面刷新。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SSQ_ISSUE_MAX 10

typedef struct {
    char code[16];    // 期号,如 "2026135"
    char date[16];    // 开奖日期,如 "2026-11-24"
    uint8_t red[6];   // 红球(升序)
    uint8_t blue;     // 蓝球
} ssq_issue_t;

typedef enum {
    SSQ_STATE_IDLE = 0,   // 尚未请求
    SSQ_STATE_LOADING,    // 拉取中
    SSQ_STATE_OK,         // 有数据
    SSQ_STATE_NO_WIFI,    // 未配置凭据或没连上
    SSQ_STATE_FAIL,       // 请求或解析失败
} ssq_state_t;

// 初始化并创建拉取任务。on_update 在数据变化后从拉取任务上下文调用,
// 实现方需自行加锁(UI 侧用 bsp_lvgl_lock)。
void ssq_result_init(void (*on_update)(void));

// 请求一次拉取(非阻塞,重复请求会合并)。
void ssq_result_request(void);

// 当前状态与缓存内容。count 为 0 时 at() 返回 NULL;index 0 为最新一期。
ssq_state_t ssq_result_state(void);
int ssq_result_count(void);
const ssq_issue_t *ssq_result_at(int index);
