// Exercise the actual CW2017 driver against a byte-addressed fake I2C device.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../components/bsp/src/bsp_battery.c"

static const uint8_t previous_profile[80] = {
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

static uint8_t registers[256], config_writes[8];
static unsigned adds, removes, reads, writes, profile_writes, verified_reads;
static unsigned flag_writes, config_count, delay_ms, soc_polls;
static int fail_read_reg, fail_write_reg, corrupt_read_reg;
static unsigned fail_reads, fail_writes;
static bool missing_device, fail_bus, fail_add;
static int bus_token, device_token;
static unsigned cold_zero_polls; /* 前 N 次 SOC 轮询期间强制 VCELL 读为 0 */

static void reset_device(bool matched, bool flag)
{
    s_dev = NULL; // Simulate a fresh application boot, not a driver deinit API.
    memset(registers, 0, sizeof(registers));
    memset(config_writes, 0, sizeof(config_writes));
    memcpy(registers + CW_REG_PROFILE,
           matched ? s_battery_profile : previous_profile, CW_PROFILE_SIZE);
    registers[CW_REG_VERSION] = 0xA0;
    registers[CW_REG_SOC_ALERT] = (flag ? CW_UPDATE_FLAG : 0) | 7;
    registers[CW_REG_CONFIG] = CW_CONFIG_ACTIVE;
    registers[CW_REG_SOC_H] = 62;
    registers[CW_REG_VCELL_H] = 0x32; // 12800 * 312.5 uV = 4000 mV.
    adds = removes = reads = writes = profile_writes = verified_reads = 0;
    flag_writes = config_count = delay_ms = soc_polls = 0;
    fail_read_reg = fail_write_reg = corrupt_read_reg = -1;
    fail_reads = fail_writes = 0;
    missing_device = fail_bus = fail_add = false;
}

esp_err_t bsp_i2c_init(void) { return fail_bus ? ESP_FAIL : ESP_OK; }
i2c_master_bus_handle_t bsp_i2c_bus(void) { return &bus_token; }

esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,
                                  const i2c_device_config_t *config,
                                  i2c_master_dev_handle_t *device)
{
    assert(bus == &bus_token && !s_dev);
    assert(config->device_address == BSP_I2C_CW2017_ADDR);
    assert(config->dev_addr_length == I2C_ADDR_BIT_LEN_7);
    assert(config->scl_speed_hz == 100000);
    if (fail_add) return ESP_ERR_NO_MEM;
    ++adds;
    *device = &device_token;
    return ESP_OK;
}

esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t device)
{
    assert(device == &device_token && adds == removes + 1);
    ++removes;
    return ESP_OK;
}

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t device,
                              const uint8_t *data, size_t length, int timeout_ms)
{
    assert(device == &device_token && length == 2 && timeout_ms == 100);
    const uint8_t reg = data[0], value = data[1];
    ++writes;
    if (reg == fail_write_reg && fail_writes) {
        --fail_writes;
        return ESP_FAIL;
    }
    if (reg >= CW_REG_PROFILE && reg < CW_REG_PROFILE + CW_PROFILE_SIZE) {
        assert(registers[CW_REG_CONFIG] == CW_CONFIG_SLEEP);
        assert(reg == CW_REG_PROFILE + profile_writes);
        assert(value == s_battery_profile[profile_writes]);
        ++profile_writes;
    } else if (reg == CW_REG_SOC_ALERT) {
        assert(profile_writes == CW_PROFILE_SIZE && verified_reads == CW_PROFILE_SIZE);
        assert(value == (registers[reg] | CW_UPDATE_FLAG));
        ++flag_writes;
    } else if (reg == CW_REG_CONFIG) {
        assert(config_count < sizeof(config_writes));
        config_writes[config_count++] = value;
    }
    registers[reg] = value;
    return ESP_OK;
}

esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t device,
                                    const uint8_t *write, size_t write_length,
                                    uint8_t *read, size_t read_length, int timeout_ms)
{
    assert(device == &device_token && write_length == 1 && timeout_ms == 100);
    const uint8_t reg = write[0];
    assert(read_length > 0 && (size_t)reg + read_length <= sizeof(registers));
    ++reads;
    if (reg == CW_REG_SOC_H && read_length == 1) ++soc_polls;
    if ((missing_device && reg == CW_REG_VERSION) ||
        (reg == fail_read_reg && fail_reads)) {
        if (fail_reads) --fail_reads;
        return ESP_FAIL;
    }
    memcpy(read, registers + reg, read_length);
    if (reg == CW_REG_VCELL_H && soc_polls <= cold_zero_polls && read_length == 2 && cold_zero_polls) {
        read[0] = 0;
        read[1] = 0; /* 冷启动误读:前 N 次轮询电压回 0mV（N=0 时不动真值） */
    }
    if (reg >= CW_REG_PROFILE && reg < CW_REG_PROFILE + CW_PROFILE_SIZE &&
        profile_writes == CW_PROFILE_SIZE && !flag_writes) {
        assert(registers[CW_REG_CONFIG] == CW_CONFIG_SLEEP);
        assert(reg == CW_REG_PROFILE + verified_reads);
        ++verified_reads;
        if (reg == corrupt_read_reg) read[0] ^= 1;
    }
    return ESP_OK;
}

void vTaskDelay(TickType_t ticks) { delay_ms += ticks; }

static void assert_update_complete(void)
{
    static const uint8_t sequence[] = {
        CW_CONFIG_RESTART, CW_CONFIG_SLEEP, CW_CONFIG_RESTART, CW_CONFIG_ACTIVE,
    };
    assert(profile_writes == 80 && verified_reads == 80 && flag_writes == 1);
    assert(config_count == sizeof(sequence));
    assert(memcmp(config_writes, sequence, sizeof(sequence)) == 0);
    assert(memcmp(registers + CW_REG_PROFILE, s_battery_profile, 80) == 0);
    assert(registers[CW_REG_SOC_ALERT] == (CW_UPDATE_FLAG | 7));
    assert(delay_ms == 160 && soc_polls == 1);
    assert(removes == 0 && s_dev != NULL);
}

static void test_update_and_matching_profiles(void)
{
    reset_device(false, true);
    assert(memcmp(previous_profile, s_battery_profile, 80) != 0);
    assert(bsp_battery_init() == ESP_OK);
    assert_update_complete();
    const unsigned previous_reads = reads, previous_writes = writes;
    assert(bsp_battery_init() == ESP_OK);
    assert(reads == previous_reads && writes == previous_writes && adds == 1);

    reset_device(true, true);
    assert(bsp_battery_init() == ESP_OK);
    assert(profile_writes == 0 && writes == 0 && soc_polls == 1);

    reset_device(true, false); // Matching bytes alone must not skip UPDATE_FLAG.
    assert(bsp_battery_init() == ESP_OK);
    assert_update_complete();

    reset_device(true, true); // Check every byte, including the last one.
    registers[CW_REG_PROFILE + CW_PROFILE_SIZE - 1] ^= 1;
    assert(bsp_battery_init() == ESP_OK);
    assert_update_complete();

    reset_device(true, true);
    registers[CW_REG_CONFIG] = CW_CONFIG_SLEEP;
    assert(bsp_battery_init() == ESP_OK);
    assert(profile_writes == 0 && config_count == 2);
    assert(config_writes[0] == CW_CONFIG_RESTART && config_writes[1] == CW_CONFIG_ACTIVE);
    assert(delay_ms == 130);
}

static void test_profile_failures(void)
{
    // Exhaust every byte position: no failed write/readback may activate the gauge.
    for (unsigned index = 0; index < CW_PROFILE_SIZE; ++index) {
        reset_device(false, false);
        fail_write_reg = CW_REG_PROFILE + (int)index;
        fail_writes = 1;
        assert(bsp_battery_init() == ESP_FAIL);
        assert(profile_writes == index && !flag_writes && !soc_polls);
        assert(config_count == 2 && removes == 1 && !s_dev);

        reset_device(false, false);
        corrupt_read_reg = CW_REG_PROFILE + (int)index;
        assert(bsp_battery_init() == ESP_FAIL);
        assert(profile_writes == 80 && verified_reads == index + 1);
        assert(!flag_writes && !soc_polls && config_count == 2);
        assert(removes == 1 && !s_dev);
    }

    reset_device(false, false);
    fail_read_reg = CW_REG_PROFILE + 37;
    fail_reads = 1;
    assert(bsp_battery_init() == ESP_FAIL);
    assert(profile_writes == 80 && verified_reads == 37 && !flag_writes);
    assert(removes == 1 && !s_dev);

    reset_device(false, false);
    fail_write_reg = CW_REG_SOC_ALERT;
    fail_writes = 1;
    assert(bsp_battery_init() == ESP_FAIL);
    assert(profile_writes == 80 && verified_reads == 80 && !flag_writes);
    assert(config_count == 2 && removes == 1 && !s_dev);
}

static void test_missing_and_recovery(void)
{
    reset_device(true, true);
    missing_device = true;
    assert(bsp_battery_init() == ESP_ERR_NOT_FOUND);
    assert(!writes && removes == 1 && !s_dev);
    assert(bsp_battery_soc() == -1 && bsp_battery_mv() == -1);
    missing_device = false;
    assert(bsp_battery_init() == ESP_OK && adds == 2);

    reset_device(true, true);
    fail_read_reg = CW_REG_PROFILE + 79;
    fail_reads = 1;
    assert(bsp_battery_init() == ESP_FAIL);
    assert(!writes && removes == 1 && !s_dev);
    assert(bsp_battery_init() == ESP_OK && adds == 2 && !writes);

    reset_device(true, true);
    fail_bus = true;
    assert(bsp_battery_init() == ESP_FAIL && !adds && !s_dev);
    fail_bus = false;
    fail_add = true;
    assert(bsp_battery_init() == ESP_ERR_NO_MEM && !adds && !s_dev);
    fail_add = false;
    assert(bsp_battery_init() == ESP_OK);
}

static void test_soc_and_voltage(void)
{
    reset_device(true, true);
    registers[CW_REG_SOC_H] = 101;
    assert(bsp_battery_init() == ESP_ERR_TIMEOUT);
    assert(soc_polls == 50 && delay_ms == 5000 && removes == 1 && !s_dev);
    registers[CW_REG_SOC_H] = 100;
    assert(bsp_battery_init() == ESP_OK);
    assert(bsp_battery_soc() == 100);
    registers[CW_REG_SOC_H] = 0;
    assert(bsp_battery_soc() == 0);
    registers[CW_REG_SOC_H] = 255;
    assert(bsp_battery_soc() == -1);

    reset_device(true, true);
    fail_read_reg = CW_REG_SOC_H;
    fail_reads = 2;
    assert(bsp_battery_init() == ESP_OK);
    assert(soc_polls == 3 && delay_ms == 300);
    assert(bsp_battery_soc() == 62 && bsp_battery_mv() == 4000);
    fail_reads = 1;
    assert(bsp_battery_soc() == -1);
    assert(bsp_battery_soc() == 62);
    fail_read_reg = CW_REG_VCELL_H;
    fail_reads = 1;
    assert(bsp_battery_mv() == -1);
    assert(bsp_battery_mv() == 4000);
    registers[CW_REG_VCELL_H] |= 0xC0;
    assert(bsp_battery_mv() == 4000); // Reserved upper bits do not affect voltage.

    reset_device(true, true);
    fail_read_reg = CW_REG_SOC_H;
    fail_reads = 50;
    assert(bsp_battery_init() == ESP_ERR_TIMEOUT);
    assert(soc_polls == 50 && delay_ms == 5000 && removes == 1 && !s_dev);
}

static void test_cold_start_guard(void)
{
    /* 冷启动前 3 次轮询读回 SOC=0/VCELL=0（假件 override），第 4 次恢复真实值：初始化应继续等到合理读数 */
    reset_device(true, true);
    cold_zero_polls = 3;
    assert(bsp_battery_init() == ESP_OK);
    assert(soc_polls == 4);
    assert(bsp_battery_soc() == 62 && bsp_battery_mv() == 4000);
    cold_zero_polls = 0;

    /* 持续 0mV：不可放行，等待窗口耗尽后按超时失败 */
    reset_device(true, true);
    cold_zero_polls = 1000;
    assert(bsp_battery_init() == ESP_ERR_TIMEOUT);
    assert(soc_polls == 50 && delay_ms == 5000 && removes == 1 && !s_dev);
    cold_zero_polls = 0;
}

int main(void)
{
    test_update_and_matching_profiles();
    test_profile_failures();
    test_missing_and_recovery();
    test_soc_and_voltage();
    test_cold_start_guard();
    puts("CW2017 battery: profile migration/readback, match, faults, retry, SOC/voltage PASS");
    return 0;
}
