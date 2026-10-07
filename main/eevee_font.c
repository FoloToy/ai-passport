#include "eevee_font.h"
#include "esp_log.h"

static const char *TAG = "eevee_font";

LV_FONT_DECLARE(eevee_cjk_font_16);

static lv_font_t s_eevee_font;

void eevee_font_init(void)
{
    s_eevee_font = eevee_cjk_font_16;
    s_eevee_font.fallback = &lv_font_montserrat_14;
    ESP_LOGI(TAG, "Static CJK 16 4bpp font ready (~8,000 glyphs, 0 bytes RAM overhead, Montserrat 14 fallback enabled)");
}

const lv_font_t *eevee_font_get(void)
{
    return &s_eevee_font;
}

const lv_font_t *eevee_font_title_get(void)
{
    return &s_eevee_font;
}
