// main/main.c — TreaAIPassport 应用入口。
// 替代 baseline hardware-test demo, 完整重设计的 AI 穿戴伙伴。
#include "treaai_app.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app_main";

void app_main(void)
{
    ESP_LOGI(TAG, "=== TreaAIPassport 启动 ===");

    esp_err_t e = treaai_app_init();
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "应用初始化失败: %s", esp_err_to_name(e));
        // 初始化失败屏幕可能已经亮起来了; 不做太多事, 打日志等 reset
        return;
    }

    // 主循环: UI tick + 状态同步 (不阻塞, 每 500ms 调一次)
    for (;;) {
        treaai_app_tick();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
