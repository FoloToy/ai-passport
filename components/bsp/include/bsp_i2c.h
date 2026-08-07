// components/bsp/include/bsp_i2c.h
// ES8311(0x18)与 CW2017(0x63)共用一条 I2C 总线。由本模块独立持有总线句柄,
// 两个驱动都向它索取 —— 避免"谁先初始化谁建总线"的隐式顺序依赖。
#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

// 初始化共享总线。幂等:重复调用直接返回 ESP_OK,可在每个驱动的 init 里放心调。
esp_err_t bsp_i2c_init(void);

// 取共享总线句柄。未初始化时返回 NULL。
i2c_master_bus_handle_t bsp_i2c_bus(void);

// 扫描 0x08..0x77 并打印所有应答的设备。排查"芯片是不是没焊好/地址对不对"极有用。
// 直接在正式总线上扫,不要另开临时总线 —— 原因见 bsp_i2c.c 中 bsp_i2c_scan() 的注释。
esp_err_t bsp_i2c_scan(void);
