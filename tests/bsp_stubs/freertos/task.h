// Minimal FreeRTOS stub for the host tests: only the delay used by
// bsp_button_prepare_deep_sleep() to let the pull-up settle. The test that
// includes the real BSP implementation provides vTaskDelay().
#pragma once

#include <stdint.h>

typedef uint32_t TickType_t;

#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

void vTaskDelay(TickType_t ticks);
