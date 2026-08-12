#include "buddy_ui.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "buddy_i4.h"
#include "buddy_sprite.h"
#include "buddy_text_layout.h"
#include "lvgl.h"

#define UI_W 240
#define UI_H 320
#define COL_BG lv_color_hex(0x080A0C)
#define COL_INK lv_color_hex(0xF7E9D7)
#define COL_DIM lv_color_hex(0x8B8178)
#define COL_LINE lv_color_hex(0x39332F)
#define COL_ORANGE lv_color_hex(0xE17B52)
#define COL_RED lv_color_hex(0xEF4B38)
#define COL_GREEN lv_color_hex(0x64C987)
#define COL_YELLOW lv_color_hex(0xF1C75B)
#define COL_BLUE lv_color_hex(0x72A7D8)

static lv_obj_t *s_screen;
static lv_obj_t *s_canvas;
static LV_ATTRIBUTE_MEM_ALIGN uint8_t s_canvas_buffer[
    LV_DRAW_BUF_SIZE(UI_W, UI_H, LV_COLOR_FORMAT_I4)];
static buddy_ui_snapshot_t s_snapshot;
static bool s_have_snapshot;
static uint32_t s_tick;
static uint64_t s_elapsed_ms;
static int s_scroll;
static buddy_i4_surface_t s_surface;

#define I4_PALETTE_BYTES (16U * sizeof(lv_color32_t))

static const uint32_t s_palette_rgb[] = {0x080A0C, 0xF7E9D7, 0x8B8178, 0x39332F,
    0xE17B52, 0xEF4B38, 0x64C987, 0xF1C75B, 0x72A7D8, 0xFFFFFF,
    0xD97757, 0xA96349, 0xB48EAD, 0x81A1C1, 0xEBCB8B, 0x151719};

static uint8_t color_index(lv_color_t color)
{
    uint32_t rgb = lv_color_to_int(color);
    uint32_t best_distance = UINT32_MAX;
    uint8_t best = 0;
    uint8_t i;
    for (i = 0; i < sizeof(s_palette_rgb) / sizeof(s_palette_rgb[0]); ++i) {
        int dr = (int)((rgb >> 16) & 0xffU) - (int)((s_palette_rgb[i] >> 16) & 0xffU);
        int dg = (int)((rgb >> 8) & 0xffU) - (int)((s_palette_rgb[i] >> 8) & 0xffU);
        int db = (int)(rgb & 0xffU) - (int)(s_palette_rgb[i] & 0xffU);
        uint32_t distance = (uint32_t)(dr * dr + dg * dg + db * db);
        if (distance < best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    return best;
}

static void pixel(int x, int y, uint8_t index)
{
    if ((unsigned)x >= UI_W || (unsigned)y >= UI_H) return;
    buddy_i4_set_pixel(s_canvas_buffer + I4_PALETTE_BYTES, UI_W,
                       (uint16_t)x, (uint16_t)y, index);
}

static int line_width(const lv_font_t *font, const char *start, size_t length, int spacing)
{
    int result = 0;
    size_t i;
    for (i = 0; i < length; ++i) {
        lv_font_glyph_dsc_t glyph;
        unsigned char c = (unsigned char)start[i];
        if (c >= 0x80U) continue;
        if (lv_font_get_glyph_dsc(font, &glyph, c, 0)) result += glyph.adv_w + spacing;
    }
    return result > 0 ? result - spacing : 0;
}

typedef struct {
    const lv_font_t *font;
    int spacing;
} text_measure_context_t;

static unsigned measure_text(const char *value, size_t length, void *context)
{
    const text_measure_context_t *measure = context;
    return (unsigned)line_width(measure->font, value, length, measure->spacing);
}

static void glyph(int x, int y, uint8_t index, const lv_font_t *font, unsigned char c)
{
    lv_font_glyph_dsc_t dsc;
    const uint8_t *bitmap;
    unsigned row;
    unsigned col;
    if (!lv_font_get_glyph_dsc(font, &dsc, c, 0) || dsc.box_w == 0 || dsc.box_h == 0) return;
    /* UNSCII is generated as immutable plain A1 data, but LVGL 9.5 does not set
     * lv_font_t.static_bitmap on these built-ins. Request the raw bitmap from
     * the font backend directly instead of the guarded convenience wrapper. */
    dsc.req_raw_bitmap = 1;
    bitmap = dsc.resolved_font->get_glyph_bitmap(&dsc, NULL);
    if (!bitmap || dsc.format != LV_FONT_GLYPH_FORMAT_A1) return;
    y += font->line_height - font->base_line - dsc.box_h - dsc.ofs_y;
    x += dsc.ofs_x;
    for (row = 0; row < dsc.box_h; ++row) {
        for (col = 0; col < dsc.box_w; ++col) {
            uint32_t bit = row * dsc.box_w + col;
            if ((bitmap[bit >> 3] & (0x80U >> (bit & 7U))) != 0) pixel(x + col, y + row, index);
        }
    }
}

static void text(lv_layer_t *layer, int x, int y, int width, lv_color_t color,
                 const char *value, bool large, lv_text_align_t align)
{
    const lv_font_t *font = large ? &lv_font_unscii_16 : &lv_font_unscii_8;
    int spacing = large ? 2 : 0;
    int line_step = font->line_height + (large ? 2 : 1);
    uint8_t index = color_index(color);
    const char *cursor = value;
    (void)layer;
    while (*cursor && y < UI_H) {
        const char *end = strchr(cursor, '\n');
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        size_t fit = length;
        int measured;
        while (fit > 0 && line_width(font, cursor, fit, spacing) > width) --fit;
        measured = line_width(font, cursor, fit, spacing);
        int pen = x;
        size_t i;
        if (align == LV_TEXT_ALIGN_CENTER) pen += (width - measured) / 2;
        else if (align == LV_TEXT_ALIGN_RIGHT) pen += width - measured;
        for (i = 0; i < fit; ++i) {
            lv_font_glyph_dsc_t dsc;
            unsigned char c = (unsigned char)cursor[i];
            if (c >= 0x80U) continue;
            glyph(pen, y, index, font, c);
            if (lv_font_get_glyph_dsc(font, &dsc, c, 0)) pen += dsc.adv_w + spacing;
        }
        y += line_step;
        if (fit < length) cursor += fit;
        else cursor = end ? end + 1 : cursor + length;
    }
}

static void wrapped_text(lv_layer_t *layer, int x, int y, int width, lv_color_t color,
                         const char *value, unsigned max_lines)
{
    char wrapped[384];
    text_measure_context_t measure = {.font = &lv_font_unscii_8, .spacing = 0};
    (void)buddy_text_wrap(value, wrapped, sizeof(wrapped), (unsigned)width, max_lines,
                          measure_text, &measure);
    text(layer, x, y, width, color, wrapped, false, LV_TEXT_ALIGN_LEFT);
}

static void box(lv_layer_t *layer, int x, int y, int w, int h, lv_color_t fill,
                lv_color_t border, int border_width, int radius)
{
    uint8_t fill_index = color_index(fill);
    uint8_t border_index = color_index(border);
    int px;
    int py;
    (void)layer;
    (void)radius;
    for (py = 0; py < h; ++py) {
        for (px = 0; px < w; ++px) {
            bool edge = px < border_width || py < border_width ||
                        px >= w - border_width || py >= h - border_width;
            pixel(x + px, y + py, edge ? border_index : fill_index);
        }
    }
}

static void rule(lv_layer_t *layer, int x, int y, int w, lv_color_t color)
{
    box(layer, x, y, w, 1, color, color, 0, 0);
}

static uint8_t art_state(buddy_character_t state)
{
    switch (state) {
    case BUDDY_CHARACTER_SLEEP: return 0;
    case BUDDY_CHARACTER_BUSY: return 2;
    case BUDDY_CHARACTER_ATTENTION:
    case BUDDY_CHARACTER_PAIRING:
    case BUDDY_CHARACTER_CONFIRMATION: return 3;
    case BUDDY_CHARACTER_CELEBRATE: return 4;
    case BUDDY_CHARACTER_DIZZY: return 5;
    case BUDDY_CHARACTER_HEART: return 6;
    default: return 1;
    }
}

static void draw_buddy(lv_layer_t *layer, const buddy_ui_snapshot_t *s, bool peek)
{
    buddy_i4_clip_t clip = {.x = 0, .y = BUDDY_UI_STAGE_Y,
                            .w = UI_W, .h = BUDDY_UI_STAGE_H};
    buddy_sprite_bounds_t bounds;
    int x = 88;
    int y = peek ? 72 : 65;
    (void)layer;
    if (buddy_sprite_bounds(s->species, art_state(s->character), s_tick, &bounds)) {
        x = (UI_W - bounds.w) / 2 - bounds.x;
    }
    buddy_sprite_render(&s_surface, &clip, s->species, art_state(s->character),
                        s_tick, x, y);
}

static void draw_status_bar(lv_layer_t *layer, const buddy_ui_snapshot_t *s)
{
    char left[32];
    char right[32];
    uint64_t age_ms = s_elapsed_ms >= s->time_received_ms ? s_elapsed_ms - s->time_received_ms : 0;
    time_t epoch = (time_t)(s->epoch_seconds + s->timezone_offset_seconds + age_ms / 1000U);
    struct tm tm_value;

    snprintf(left, sizeof(left), "%s", s->ble_connected ? (s->ble_encrypted ? "BLE+" : "BLE") : "CLAUDE");
    if (s->epoch_seconds > 0 && gmtime_r(&epoch, &tm_value) != NULL) {
        snprintf(right, sizeof(right), "%02d:%02d", tm_value.tm_hour, tm_value.tm_min);
    } else {
        snprintf(right, sizeof(right), "%s", s->heartbeat_stale ? "SLEEP" : "LIVE");
    }
    text(layer, 8, 7, 100, s->ble_connected ? COL_GREEN : COL_DIM, left, false, LV_TEXT_ALIGN_LEFT);
    text(layer, 132, 7, 100, COL_DIM, right, false, LV_TEXT_ALIGN_RIGHT);
    rule(layer, 8, 25, 224, COL_LINE);
}

static void draw_home(lv_layer_t *layer, const buddy_ui_snapshot_t *s)
{
    char caption[176];
    text(layer, 10, 35, 220, COL_ORANGE, buddy_sprite_name(s->species), false,
         LV_TEXT_ALIGN_CENTER);
    draw_buddy(layer, s, false);
    rule(layer, 18, BUDDY_UI_INFO_Y, 204, COL_LINE);
    snprintf(caption, sizeof(caption), "%s", s->message[0] ? s->message :
             (s->ble_connected ? "WAITING FOR CLAUDE" : "OPEN CLAUDE DESKTOP TO PAIR"));
    wrapped_text(layer, 18, 174, 204, s->heartbeat_stale ? COL_DIM : COL_INK,
                 caption, 8);
    text(layer, 8, 300, 224, COL_DIM, "UP  SCREENS       HOLD OK  MENU", false, LV_TEXT_ALIGN_CENTER);
}

static void draw_heart(lv_layer_t *layer, int x, int y, bool on)
{
    lv_color_t c = on ? COL_RED : COL_LINE;
    box(layer, x + 2, y, 4, 4, c, c, 0, 0);
    box(layer, x + 8, y, 4, 4, c, c, 0, 0);
    box(layer, x, y + 3, 14, 5, c, c, 0, 0);
    box(layer, x + 3, y + 8, 8, 3, c, c, 0, 0);
    box(layer, x + 6, y + 11, 2, 2, c, c, 0, 0);
}

static void draw_pet(lv_layer_t *layer, const buddy_ui_snapshot_t *s)
{
    char value[64];
    unsigned i;
    uint64_t level = s->tokens / 50000ULL;
    text(layer, 9, 35, 222, COL_ORANGE, buddy_sprite_name(s->species), false, LV_TEXT_ALIGN_CENTER);
    draw_buddy(layer, s, true);
    rule(layer, 12, BUDDY_UI_INFO_Y, 216, COL_LINE);
    text(layer, 16, 170, 62, COL_DIM, "MOOD", false, LV_TEXT_ALIGN_LEFT);
    for (i = 0; i < 4; ++i) draw_heart(layer, 84 + (int)i * 25, 168, !s->heartbeat_stale || i < 2);
    snprintf(value, sizeof(value), "LV %llu", (unsigned long long)level);
    box(layer, 184, 166, 42, 19, COL_ORANGE, COL_ORANGE, 0, 3);
    text(layer, 186, 171, 38, COL_BG, value, false, LV_TEXT_ALIGN_CENTER);
    text(layer, 16, 200, 75, COL_DIM, "TOKENS", false, LV_TEXT_ALIGN_LEFT);
    snprintf(value, sizeof(value), "%llu", (unsigned long long)s->tokens);
    text(layer, 92, 200, 132, COL_INK, value, false, LV_TEXT_ALIGN_RIGHT);
    text(layer, 16, 222, 75, COL_DIM, "TODAY", false, LV_TEXT_ALIGN_LEFT);
    snprintf(value, sizeof(value), "%llu", (unsigned long long)s->tokens_today);
    text(layer, 92, 222, 132, COL_INK, value, false, LV_TEXT_ALIGN_RIGHT);
    text(layer, 16, 248, 75, COL_DIM, "ENERGY", false, LV_TEXT_ALIGN_LEFT);
    for (i = 0; i < 8; ++i) {
        bool on = !s->heartbeat_stale && i < 6;
        box(layer, 93 + (int)i * 16, 248, 11, 8, on ? COL_YELLOW : COL_LINE,
            on ? COL_YELLOW : COL_LINE, 0, 1);
    }
    text(layer, 8, 300, 224, COL_DIM, "DOWN  DETAILS        HOLD OK  MENU", false, LV_TEXT_ALIGN_CENTER);
}

static void draw_info(lv_layer_t *layer, const buddy_ui_snapshot_t *s)
{
    static const char *const titles[] = {"ABOUT", "BUTTONS", "CLAUDE", "DEVICE", "BLUETOOTH", "CREDITS"};
    char body[512];
    char page[16];
    unsigned p = s->info_page < 6 ? s->info_page : 0;
    text(layer, 14, 38, 180, COL_ORANGE, titles[p], true, LV_TEXT_ALIGN_LEFT);
    snprintf(page, sizeof(page), "%u / 6", p + 1);
    text(layer, 174, 43, 52, COL_DIM, page, false, LV_TEXT_ALIGN_RIGHT);
    rule(layer, 14, 66, 212, COL_LINE);
    switch (p) {
    case 0: snprintf(body, sizeof(body), "A TINY COMPANION FOR\nCLAUDE DESKTOP.\n\nIT SLEEPS, WORKS AND\nCELEBRATES BESIDE YOU.\n\nAPPROVE TOOL REQUESTS\nRIGHT FROM THE DEVICE."); break;
    case 1: snprintf(body, sizeof(body), "UP     NEXT SCREEN\nDOWN   NEXT PAGE / DENY\nOK     APPROVE / CHANGE\nHOLD   OPEN MENU"); break;
    case 2: snprintf(body, sizeof(body), "SESSIONS     %u\nRUNNING      %u\nWAITING      %u\n\nTOKENS       %llu", s->total, s->running, s->waiting, (unsigned long long)s->tokens); break;
    case 3: snprintf(body, sizeof(body), "NAME\n%s\n\nOWNER\n%s\n\nDISPLAY     240 X 320", s->name[0] ? s->name : "CLAUDE BUDDY", s->owner[0] ? s->owner : "-"); break;
    case 4: snprintf(body, sizeof(body), "%s\n\n%s\n%s\n\nPAIR IN CLAUDE DESKTOP\nDEVELOPER > HARDWARE BUDDY", s->name[0] ? s->name : "CLAUDE-BUDDY", s->ble_connected ? "CONNECTED" : "ADVERTISING", s->ble_encrypted ? "ENCRYPTED" : "NOT ENCRYPTED"); break;
    default: snprintf(body, sizeof(body), "CLAUDE DESKTOP BUDDY\nBY FELIX RIESEBERG\n\nESP32-C3 HARDWARE PORT\nFOR TRAE CARD BSP\n\nAPACHE-2.0"); break;
    }
    wrapped_text(layer, 16, 82 - s_scroll, 208, COL_INK, body, 18);
    text(layer, 8, 300, 224, COL_DIM, "DOWN  NEXT PAGE      HOLD OK  MENU", false, LV_TEXT_ALIGN_CENTER);
}

static void draw_list(lv_layer_t *layer, const char *title, const char *const *items,
                      unsigned count, unsigned selected, const buddy_ui_snapshot_t *s)
{
    unsigned first = selected > 5 ? selected - 5 : 0;
    unsigned i;
    text(layer, 14, 34, 212, COL_ORANGE, title, true, LV_TEXT_ALIGN_LEFT);
    rule(layer, 14, 62, 212, COL_LINE);
    for (i = first; i < count && i < first + 7; ++i) {
        int y = 76 + (int)(i - first) * 29;
        bool active = i == selected;
        char row[64];
        const char *suffix = "";
        char value[12];
        if (!s->reset_open && i == BUDDY_SETTINGS_BRIGHTNESS) { snprintf(value, sizeof(value), "%u/4", s->brightness_level); suffix = value; }
        else if (!s->reset_open && i == BUDDY_SETTINGS_BLE) suffix = s->ble_enabled ? "ON" : "OFF";
        else if (!s->reset_open && i == BUDDY_SETTINGS_TRANSCRIPT) suffix = s->transcript_enabled ? "ON" : "OFF";
        else if (!s->reset_open && i == BUDDY_SETTINGS_ASCII_PET) suffix = buddy_sprite_name(s->species);
        snprintf(row, sizeof(row), "%s", items[i]);
        if (active) box(layer, 12, y - 7, 216, 24, COL_ORANGE, COL_ORANGE, 0, 3);
        text(layer, 20, y, 142, active ? COL_BG : COL_INK, row, false, LV_TEXT_ALIGN_LEFT);
        text(layer, 158, y, 62, active ? COL_BG : COL_DIM, suffix, false, LV_TEXT_ALIGN_RIGHT);
    }
    text(layer, 8, 300, 224, COL_DIM, "UP/DOWN  SELECT       OK  CHANGE", false, LV_TEXT_ALIGN_CENTER);
}

static void draw_settings(lv_layer_t *layer, const buddy_ui_snapshot_t *s)
{
    static const char *const settings[] = {"BRIGHTNESS", "SOUND", "BLUETOOTH", "WIFI", "LED", "TRANSCRIPT", "CLOCK ROT", "BUDDY", "RESET", "BACK"};
    static const char *const reset[] = {"DELETE CHARACTER", "FACTORY RESET", "UNPAIR", "BACK"};
    draw_list(layer, s->reset_open ? "RESET" : "SETTINGS", s->reset_open ? reset : settings,
              s->reset_open ? BUDDY_RESET_COUNT : BUDDY_SETTINGS_COUNT,
              s->reset_open ? s->reset_selection : s->settings_selection, s);
}

static void panel(lv_layer_t *layer, int y, int h, lv_color_t accent, const char *title,
                  const char *body, const char *footer)
{
    box(layer, 10, y, 220, h, lv_color_hex(0x151719), accent, 2, 6);
    box(layer, 10, y, 220, 27, accent, accent, 0, 5);
    text(layer, 18, y + 8, 204, COL_BG, title, false, LV_TEXT_ALIGN_LEFT);
    wrapped_text(layer, 20, y + 42 - s_scroll, 200, COL_INK, body, 5);
    rule(layer, 20, y + h - 34, 200, COL_LINE);
    text(layer, 18, y + h - 23, 204, COL_DIM, footer, false, LV_TEXT_ALIGN_CENTER);
}

static void draw_overlay(lv_layer_t *layer, const buddy_ui_snapshot_t *s)
{
    char body[448];
    int x;
    int y;
    buddy_overlay_kind_t overlay = buddy_overlay_select(s->confirmation_pending,
                                                        s->passkey_visible,
                                                        s->prompt_id[0] != '\0',
                                                        s->menu_open);
    if (overlay != BUDDY_OVERLAY_NONE) {
        for (y = BUDDY_UI_STATUS_H; y < BUDDY_UI_ACTION_Y; ++y) {
            for (x = (y & 1); x < UI_W; x += 2) {
                uint8_t current = buddy_i4_get_pixel(s_surface.pixels, UI_W,
                                                     (uint16_t)x, (uint16_t)y);
                if (current != 0) buddy_i4_set_pixel(s_surface.pixels, UI_W,
                                                          (uint16_t)x, (uint16_t)y,
                                                          15);
            }
        }
    }
    if (overlay == BUDDY_OVERLAY_CONFIRMATION) {
        panel(layer, 62, 196, COL_RED, "CONFIRM ACTION",
              s->confirmation == BUDDY_CONFIRM_FACTORY_RESET ? "FACTORY RESET?\n\nSETTINGS AND STATISTICS\nWILL BE ERASED." : "UNPAIR CLAUDE DESKTOP?\n\nTHE SAVED BLUETOOTH BOND\nWILL BE ERASED.",
              "OK  CONFIRM        DOWN  CANCEL");
    } else if (overlay == BUDDY_OVERLAY_PAIRING) {
        snprintf(body, sizeof(body), "ENTER THIS CODE\nIN CLAUDE DESKTOP\n\n       %06lu", (unsigned long)s->passkey);
        panel(layer, 66, 188, COL_BLUE, "BLUETOOTH PAIRING", body, "KEEP THIS SCREEN OPEN");
    } else if (overlay == BUDDY_OVERLAY_APPROVAL) {
        snprintf(body, sizeof(body), "%s\n\n%s", s->prompt_tool, s->prompt_hint);
        panel(layer, 154, 158, s->approval_locked ? COL_DIM : COL_RED, "CLAUDE NEEDS APPROVAL", body,
              s->approval_locked ? (s->permission_delivery == BUDDY_PERMISSION_DELIVERY_FAILED ? "SEND FAILED - TRY AGAIN" : "SENDING...") : "OK  APPROVE        DOWN  DENY");
    } else if (overlay == BUDDY_OVERLAY_MENU) {
        static const char *const menu[] = {"SETTINGS", "TURN OFF", "HELP", "ABOUT", "DEMO", "CLOSE"};
        unsigned i;
        box(layer, 38, 48, 164, 224, lv_color_hex(0x151719), COL_INK, 2, 5);
        text(layer, 52, 61, 136, COL_ORANGE, "MENU", true, LV_TEXT_ALIGN_CENTER);
        rule(layer, 52, 88, 136, COL_LINE);
        for (i = 0; i < BUDDY_MENU_COUNT; ++i) {
            int y = 103 + (int)i * 25;
            bool active = i == (unsigned)s->menu_selection;
            if (active) box(layer, 48, y - 7, 144, 21, COL_ORANGE, COL_ORANGE, 0, 2);
            text(layer, 56, y, 128, active ? COL_BG : COL_INK, menu[i], false, LV_TEXT_ALIGN_CENTER);
        }
    }
}

static void redraw(void)
{
    lv_layer_t *layer = NULL;
    if (!s_canvas || !s_have_snapshot) return;
    memset(s_canvas_buffer + I4_PALETTE_BYTES, 0, sizeof(s_canvas_buffer) - I4_PALETTE_BYTES);
    draw_status_bar(layer, &s_snapshot);
    switch (s_snapshot.page) {
    case BUDDY_PAGE_PET: draw_pet(layer, &s_snapshot); break;
    case BUDDY_PAGE_INFO: draw_info(layer, &s_snapshot); break;
    case BUDDY_PAGE_SETTINGS: draw_settings(layer, &s_snapshot); break;
    default: draw_home(layer, &s_snapshot); break;
    }
    draw_overlay(layer, &s_snapshot);
    lv_obj_invalidate(s_canvas);
}

void buddy_ui_init(void)
{
    unsigned i;
    if (s_screen) return;
    s_screen = lv_obj_create(NULL);
    lv_obj_set_size(s_screen, UI_W, UI_H);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_screen, 0, 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_bg_color(s_screen, COL_BG, 0);
    s_canvas = lv_canvas_create(s_screen);
    lv_canvas_set_buffer(s_canvas, s_canvas_buffer, UI_W, UI_H, LV_COLOR_FORMAT_I4);
    lv_obj_set_pos(s_canvas, 0, 0);
    buddy_i4_surface_init(&s_surface, s_canvas_buffer + I4_PALETTE_BYTES,
                          UI_W, UI_H, UI_W / 2);
    for (i = 0; i < sizeof(s_palette_rgb) / sizeof(s_palette_rgb[0]); ++i)
        lv_canvas_set_palette(s_canvas, i, lv_color_to_32(lv_color_hex(s_palette_rgb[i]), LV_OPA_COVER));
    lv_screen_load(s_screen);
}

void buddy_ui_render(const buddy_ui_snapshot_t *snapshot)
{
    if (!snapshot) return;
    buddy_ui_init();
    s_snapshot = *snapshot;
    s_have_snapshot = true;
    s_scroll = 0;
    redraw();
}

void buddy_ui_show_passkey(uint32_t passkey)
{
    buddy_ui_snapshot_t snapshot = {.passkey_visible = true, .passkey = passkey};
    buddy_ui_render(&snapshot);
}

void buddy_ui_tick(uint64_t elapsed_ms)
{
    uint32_t tick = (uint32_t)(elapsed_ms / 200U);
    s_elapsed_ms = elapsed_ms;
    if (tick != s_tick) {
        s_tick = tick;
        redraw();
    }
}

void buddy_ui_scroll(int delta)
{
    s_scroll += delta;
    if (s_scroll < 0) s_scroll = 0;
    if (s_scroll > 160) s_scroll = 160;
    redraw();
}
