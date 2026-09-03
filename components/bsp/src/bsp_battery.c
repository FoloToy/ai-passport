// components/bsp/src/bsp_battery.c
// CW2017 电量计驱动。
#include "bsp_battery.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>

static const char *TAG = "bsp_batt";

#define CW_REG_VERSION   0x00   // 版本号,上电应答即代表芯片在位
#define CW_REG_VCELL_H   0x02   // 14bit 电压,V(uV) = raw * 312.5
#define CW_REG_SOC_H     0x04   // 高字节 = 整数百分比;低字节(0x05)= 1/256 %
#define CW_REG_CONFIG    0x08   // 0xF0=睡眠 / 0x30=复位态 / 0x00=正常
#define CW_REG_SOC_ALERT 0x0B   // bit7=profile UPDATE_FLAG;bit6:0=SOC 告警阈值
#define CW_REG_PROFILE   0x10   // 80 字节电池 profile 起始地址

#define CW_CONFIG_ACTIVE  0x00
#define CW_CONFIG_RESTART 0x30
#define CW_CONFIG_SLEEP   0xF0
#define CW_UPDATE_FLAG    0x80
#define CW_PROFILE_SIZE   80

static const uint8_t s_battery_profile[CW_PROFILE_SIZE] = {
    0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xAD, 0xC7, 0xC8, 0xCA, 0xBD, 0xB1, 0xC1, 0x94,
    0x88, 0xD1, 0xBD, 0x97, 0x88, 0x66, 0x56, 0x4A,
    0x3F, 0x33, 0x26, 0x5C, 0x37, 0xD1, 0x27, 0xD8,
    0xCC, 0xB7, 0xCF, 0xB3, 0xB2, 0xAE, 0xA6, 0x9E,
    0x99, 0x97, 0x9B, 0x86, 0x47, 0x1E, 0x17, 0x26,
    0x49, 0x96, 0xD9, 0xE1, 0xDD, 0xDC, 0xD4, 0x59,
    0x00, 0x00, 0x90, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5C,
};

_Static_assert(sizeof(s_battery_profile) == CW_PROFILE_SIZE,
               "CW2017 battery profile must contain exactly 80 bytes");

static i2c_master_dev_handle_t s_dev;

static int cw_read(uint8_t reg, uint8_t *buf, size_t n) {
    if (!s_dev) return -1;
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, n, 100) == ESP_OK ? 0 : -1;
}

static int cw_write(uint8_t reg, uint8_t val) {
    if (!s_dev) return -1;
    uint8_t b[2] = { reg, val };
    return i2c_master_transmit(s_dev, b, 2, 100) == ESP_OK ? 0 : -1;
}

// CONFIG 的低 4 bit 为保留位。复位、睡眠和激活均按芯片规定的时序切换。
static int cw_enter_sleep(void) {
    if (cw_write(CW_REG_CONFIG, CW_CONFIG_RESTART) != 0) return -1;
    vTaskDelay(pdMS_TO_TICKS(20));
    if (cw_write(CW_REG_CONFIG, CW_CONFIG_SLEEP) != 0) return -1;
    vTaskDelay(pdMS_TO_TICKS(10));
    return 0;
}

static int cw_enter_active(void) {
    if (cw_write(CW_REG_CONFIG, CW_CONFIG_RESTART) != 0) return -1;
    vTaskDelay(pdMS_TO_TICKS(20));
    if (cw_write(CW_REG_CONFIG, CW_CONFIG_ACTIVE) != 0) return -1;
    vTaskDelay(pdMS_TO_TICKS(10));
    return 0;
}

// 同时检查 UPDATE_FLAG 和 80 字节内容，避免只凭标志误用旧电芯参数。
static int cw_profile_matches(bool *matches) {
    uint8_t val = 0;
    *matches = false;

    if (cw_read(CW_REG_SOC_ALERT, &val, 1) != 0) return -1;
    if ((val & CW_UPDATE_FLAG) == 0) return 0;

    for (size_t i = 0; i < CW_PROFILE_SIZE; i++) {
        if (cw_read((uint8_t)(CW_REG_PROFILE + i), &val, 1) != 0) return -1;
        if (val != s_battery_profile[i]) return 0;
    }

    *matches = true;
    return 0;
}

// Profile 必须在睡眠态逐字节写入，读回校验后置 UPDATE_FLAG，再重启计算。
static int cw_update_profile(void) {
    uint8_t val = 0;
    if (cw_enter_sleep() != 0) return -1;

    for (size_t i = 0; i < CW_PROFILE_SIZE; i++) {
        if (cw_write((uint8_t)(CW_REG_PROFILE + i), s_battery_profile[i]) != 0) {
            ESP_LOGE(TAG, "写 profile 失败:index=%u", (unsigned)i);
            return -1;
        }
    }

    for (size_t i = 0; i < CW_PROFILE_SIZE; i++) {
        if (cw_read((uint8_t)(CW_REG_PROFILE + i), &val, 1) != 0) return -1;
        if (val != s_battery_profile[i]) {
            ESP_LOGE(TAG, "profile 校验失败:index=%u expected=0x%02X actual=0x%02X",
                     (unsigned)i, s_battery_profile[i], val);
            return -1;
        }
    }

    if (cw_read(CW_REG_SOC_ALERT, &val, 1) != 0) return -1;
    if (cw_write(CW_REG_SOC_ALERT, val | CW_UPDATE_FLAG) != 0) return -1;
    return cw_enter_active();
}

// 首次计算期间 SOC 可能暂时大于 100；最多等待 5 秒再判定初始化失败。
static int cw_wait_soc_ready(void) {
    for (int retry = 0; retry < 50; retry++) {
        uint8_t soc = 0;
        vTaskDelay(pdMS_TO_TICKS(100));
        if (cw_read(CW_REG_SOC_H, &soc, 1) == 0 && soc <= 100) return 0;
    }
    return -1;
}

esp_err_t bsp_battery_init(void) {
    if (s_dev) return ESP_OK;

    esp_err_t e = bsp_i2c_init();
    if (e != ESP_OK) return e;

    i2c_device_config_t dc = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BSP_I2C_CW2017_ADDR,
        .scl_speed_hz    = 100000,
    };
    e = i2c_master_bus_add_device(bsp_i2c_bus(), &dc, &s_dev);
    if (e != ESP_OK) { ESP_LOGE(TAG, "添加 I2C 设备失败: %s", esp_err_to_name(e)); return e; }

    uint8_t ver = 0;
    if (cw_read(CW_REG_VERSION, &ver, 1) != 0) {
        ESP_LOGW(TAG, "CW2017 未应答 —— 用 bsp_i2c_scan() 确认 0x%02X 是否在线;"
                      "无电量计的板子可忽略本项", BSP_I2C_CW2017_ADDR);
        i2c_master_bus_rm_device(s_dev);
        s_dev = NULL;
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "检测到 CW2017 VERSION=0x%02X", ver);

    bool profile_matches = false;
    if (cw_profile_matches(&profile_matches) != 0) {
        ESP_LOGE(TAG, "读取电池 profile 失败");
        e = ESP_FAIL;
        goto fail;
    }

    if (!profile_matches) {
        ESP_LOGI(TAG, "写入优特利 520mAh 自定义 profile");
        if (cw_update_profile() != 0) {
            ESP_LOGE(TAG, "自定义 profile 写入失败");
            e = ESP_FAIL;
            goto fail;
        }
    } else {
        uint8_t config = 0;
        if (cw_read(CW_REG_CONFIG, &config, 1) != 0) {
            e = ESP_FAIL;
            goto fail;
        }
        if (config != CW_CONFIG_ACTIVE && cw_enter_active() != 0) {
            e = ESP_FAIL;
            goto fail;
        }
        ESP_LOGI(TAG, "优特利 520mAh profile 已匹配");
    }

    if (cw_wait_soc_ready() != 0) {
        ESP_LOGE(TAG, "等待首次 SOC 计算超时");
        e = ESP_ERR_TIMEOUT;
        goto fail;
    }

    return ESP_OK;

fail:
    i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;
    return e;
}

int bsp_battery_soc(void) {
    uint8_t b[2] = { 0 };
    if (cw_read(CW_REG_SOC_H, b, 2) != 0) return -1;
    int soc = b[0];                       // 高字节即整数百分比
    if (soc > 100) return -1;             // 芯片未就绪时可能读到 0xFF
    return soc;
}

int bsp_battery_mv(void) {
    uint8_t b[2] = { 0 };
    if (cw_read(CW_REG_VCELL_H, b, 2) != 0) return -1;
    uint32_t raw = ((uint32_t)b[0] << 8 | b[1]) & 0x3FFF;   // 14bit
    return (int)((raw * 3125) / 10000);                     // raw * 312.5uV → mV
}
