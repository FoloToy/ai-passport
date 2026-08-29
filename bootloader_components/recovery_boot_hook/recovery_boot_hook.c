// Bootloader hook shared by every AI Passport community firmware derivative.
// Holding the UP-key ADC GPIO low for five seconds boots the factory-installed
// permanent Recovery image. This project never owns or writes that image.
#include "bootloader_common.h"
#include "bootloader_config.h"
#include "bootloader_utility.h"
#include "esp_log.h"

#define RECOVERY_BUTTON_GPIO  0
#define RECOVERY_HOLD_SECONDS 5
#define RECOVERY_OFFSET       0x700000
#define RECOVERY_SIZE         0x100000

void bootloader_hooks_include(void)
{
}

void bootloader_after_init(void)
{
    if (bootloader_common_check_long_hold_gpio(
            RECOVERY_BUTTON_GPIO, RECOVERY_HOLD_SECONDS) != GPIO_LONG_HOLD) {
        return;
    }

    ESP_LOGI("recovery_boot", "UP held: booting permanent recovery at 0x%x",
             RECOVERY_OFFSET);
    bootloader_state_t state = { 0 };
    state.factory.offset = RECOVERY_OFFSET;
    state.factory.size = RECOVERY_SIZE;
    bootloader_utility_load_boot_image(&state, FACTORY_INDEX);
}
