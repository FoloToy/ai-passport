// main/passport_app.h —— 应用入口：接管菜单循环、按键分发、屏幕切换、省电。
//
// 线程模型（实现文档 7.1 节）：
//   按键回调（button 任务）→ 只投递 key_queue
//   网络任务 → 只投递 ui_queue
//   LVGL 任务内的 dispatch 定时器 = 唯一 UI 上下文，统一消费两个队列
#pragma once

// BSP（显示/LVGL）与 passport_store 初始化完成后由 app_main 调用。
void passport_app_start(void);

// ---- 上游 demo 菜单（main.c 提供；设置页长按 OK 5 秒的隐藏入口） ----
void legacy_menu_enter(void);
// 返回 true 表示请求退出 demo 菜单、回到创智 Passport 主界面
bool legacy_menu_handle_key(int btn, int ev);
