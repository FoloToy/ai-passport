#include "buddy_ui.h"

#include <stdio.h>

#include "buddy_character.h"
#include "lvgl.h"

#define BUDDY_UI_WIDTH 240
#define BUDDY_UI_HEIGHT 320

static lv_obj_t *s_screen;
static lv_obj_t *s_status_page;
static lv_obj_t *s_status_bar;
static lv_obj_t *s_character_label;
static lv_obj_t *s_summary_label;
static lv_obj_t *s_hint_label;
static lv_obj_t *s_transcript_page;
static lv_obj_t *s_transcript_label;
static lv_obj_t *s_settings_page;
static lv_obj_t *s_approval_page;
static lv_obj_t *s_approval_tool_label;
static lv_obj_t *s_approval_hint_label;
static lv_obj_t *s_approval_action_label;
static lv_obj_t *s_passkey_page;
static lv_obj_t *s_passkey_label;
static lv_obj_t *s_confirmation_page;
static buddy_character_t s_character = BUDDY_CHARACTER_SLEEP;

static void buddy_ui_label_style(lv_obj_t *label, const lv_font_t *font)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_black(), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

static lv_obj_t *buddy_ui_page_create(void)
{
    lv_obj_t *page = lv_obj_create(s_screen);

    lv_obj_set_pos(page, 0, 0);
    lv_obj_set_size(page, BUDDY_UI_WIDTH, BUDDY_UI_HEIGHT);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_set_style_bg_color(page, lv_color_white(), 0);
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    return page;
}

static lv_obj_t *buddy_ui_text(lv_obj_t *parent, int x, int y, int width,
                               const lv_font_t *font, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    buddy_ui_label_style(label, font);
    lv_label_set_text(label, text);
    return label;
}

static lv_obj_t *buddy_ui_hint_box(lv_obj_t *parent, int x, int y, int width, int height,
                                   lv_obj_t **label_out)
{
    lv_obj_t *box = lv_obj_create(parent);

    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, width, height);
    lv_obj_set_scroll_dir(box, LV_DIR_VER);
    lv_obj_set_style_pad_all(box, 4, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    *label_out = buddy_ui_text(box, 0, 0, width - 8, &lv_font_montserrat_14, "");
    return box;
}

static void buddy_ui_hide_pages(void)
{
    if (s_status_page != NULL) {
        lv_obj_add_flag(s_status_page, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_transcript_page != NULL) {
        lv_obj_add_flag(s_transcript_page, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_settings_page != NULL) {
        lv_obj_add_flag(s_settings_page, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_approval_page != NULL) {
        lv_obj_add_flag(s_approval_page, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_passkey_page != NULL) {
        lv_obj_add_flag(s_passkey_page, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_confirmation_page != NULL) {
        lv_obj_add_flag(s_confirmation_page, LV_OBJ_FLAG_HIDDEN);
    }
}

static void buddy_ui_show(lv_obj_t *page)
{
    buddy_ui_hide_pages();
    lv_obj_remove_flag(page, LV_OBJ_FLAG_HIDDEN);
}

static void buddy_ui_create_status(void)
{
    if (s_status_page != NULL) {
        return;
    }
    s_status_page = buddy_ui_page_create();
    s_status_bar = buddy_ui_text(s_status_page, 8, 8, 224, &lv_font_montserrat_14, "Offline");
    s_character_label = buddy_ui_text(s_status_page, 24, 55, 192, &lv_font_montserrat_20, "");
    s_summary_label = buddy_ui_text(s_status_page, 12, 190, 216, &lv_font_montserrat_14, "");
    (void)buddy_ui_hint_box(s_status_page, 12, 245, 216, 62, &s_hint_label);
}

static void buddy_ui_create_transcript(void)
{
    if (s_transcript_page != NULL) {
        return;
    }
    s_transcript_page = buddy_ui_page_create();
    (void)buddy_ui_text(s_transcript_page, 12, 12, 216, &lv_font_montserrat_20, "Transcript");
    (void)buddy_ui_hint_box(s_transcript_page, 12, 52, 216, 252, &s_transcript_label);
}

static void buddy_ui_create_settings(void)
{
    if (s_settings_page != NULL) {
        return;
    }
    s_settings_page = buddy_ui_page_create();
    (void)buddy_ui_text(s_settings_page, 12, 18, 216, &lv_font_montserrat_20, "Settings");
    (void)buddy_ui_text(s_settings_page, 12, 70, 216, &lv_font_montserrat_14,
                        "OK Save\nDOWN Back");
}

static void buddy_ui_create_approval(void)
{
    if (s_approval_page != NULL) {
        return;
    }
    s_approval_page = buddy_ui_page_create();
    (void)buddy_ui_text(s_approval_page, 12, 12, 216, &lv_font_montserrat_20, "Approval");
    s_approval_tool_label = buddy_ui_text(s_approval_page, 12, 52, 216,
                                           &lv_font_montserrat_14, "");
    (void)buddy_ui_hint_box(s_approval_page, 12, 84, 216, 136, &s_approval_hint_label);
    s_approval_action_label = buddy_ui_text(s_approval_page, 12, 240, 216,
                                             &lv_font_montserrat_14,
                                             "OK Approve once\nDOWN Deny");
}

static void buddy_ui_create_passkey(void)
{
    if (s_passkey_page != NULL) {
        return;
    }
    s_passkey_page = buddy_ui_page_create();
    (void)buddy_ui_text(s_passkey_page, 12, 28, 216, &lv_font_montserrat_20, "Pairing code");
    s_passkey_label = buddy_ui_text(s_passkey_page, 12, 100, 216,
                                     &lv_font_montserrat_20, "000000");
    (void)buddy_ui_text(s_passkey_page, 12, 188, 216, &lv_font_montserrat_14,
                        "Confirm this code on Claude Desktop.");
}

static void buddy_ui_create_confirmation(void)
{
    if (s_confirmation_page != NULL) {
        return;
    }
    s_confirmation_page = buddy_ui_page_create();
    (void)buddy_ui_text(s_confirmation_page, 12, 25, 216, &lv_font_montserrat_20,
                        "Confirm action");
    (void)buddy_ui_text(s_confirmation_page, 12, 90, 216, &lv_font_montserrat_14,
                        "Unpair or factory reset?\n\nOK Confirm\nDOWN Cancel");
}

static const char *buddy_ui_connection_text(const buddy_ui_snapshot_t *snapshot)
{
    if (snapshot->heartbeat_stale || snapshot->connection == BUDDY_CONNECTION_OFFLINE) {
        return "Offline";
    }
    if (snapshot->connection == BUDDY_CONNECTION_PAIRING) {
        return "Pairing";
    }
    if (snapshot->connection == BUDDY_CONNECTION_CONFIRMING) {
        return "Confirming";
    }
    return "Connected";
}

static void buddy_ui_render_status(const buddy_ui_snapshot_t *snapshot)
{
    char status[BUDDY_MESSAGE_MAX + 16];
    char summary[128];
    const char *hint = snapshot->message[0] != '\0' ? snapshot->message : snapshot->entries[0];

    (void)snprintf(status, sizeof(status), "%s  %s", buddy_ui_connection_text(snapshot),
                   snapshot->time);
    (void)snprintf(summary, sizeof(summary), "%s%s%s\n%u running  %llu tokens",
                   snapshot->name[0] != '\0' ? snapshot->name : "Claude Buddy",
                   snapshot->owner[0] != '\0' ? " / " : "",
                   snapshot->owner,
                   snapshot->running, (unsigned long long)snapshot->tokens);
    lv_label_set_text(s_status_bar, status);
    lv_label_set_text(s_summary_label, summary);
    lv_label_set_text(s_hint_label, hint[0] != '\0' ? hint : "Waiting for Claude Desktop.");
    lv_label_set_text(s_character_label, buddy_character_frame(s_character, 0));
    buddy_ui_show(s_status_page);
}

void buddy_ui_init(void)
{
    if (s_screen != NULL) {
        return;
    }
    s_screen = lv_obj_create(NULL);
    lv_obj_set_size(s_screen, BUDDY_UI_WIDTH, BUDDY_UI_HEIGHT);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_screen, 0, 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_bg_color(s_screen, lv_color_white(), 0);
    lv_screen_load(s_screen);
}

void buddy_ui_render(const buddy_ui_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }
    buddy_ui_init();
    s_character = snapshot->character;

    if (snapshot->confirmation_pending || snapshot->connection == BUDDY_CONNECTION_CONFIRMING) {
        buddy_ui_create_confirmation();
        buddy_ui_show(s_confirmation_page);
        return;
    }
    if (snapshot->prompt_id[0] != '\0') {
        buddy_ui_create_approval();
        lv_label_set_text_fmt(s_approval_tool_label, "Tool: %s", snapshot->prompt_tool);
        lv_label_set_text(s_approval_hint_label, snapshot->prompt_hint);
        lv_label_set_text(s_approval_action_label, snapshot->approval_locked ? "Sending..." :
                          "OK Approve once\nDOWN Deny");
        buddy_ui_show(s_approval_page);
        return;
    }
    if (snapshot->page == BUDDY_PAGE_SETTINGS) {
        buddy_ui_create_settings();
        buddy_ui_show(s_settings_page);
        return;
    }
    if (snapshot->entries[1][0] != '\0' || snapshot->entries[2][0] != '\0' ||
        snapshot->entries[3][0] != '\0') {
        char transcript[BUDDY_ENTRY_COUNT * BUDDY_ENTRY_MAX + BUDDY_ENTRY_COUNT];

        buddy_ui_create_transcript();
        (void)snprintf(transcript, sizeof(transcript), "%s\n%s\n%s\n%s",
                       snapshot->entries[0], snapshot->entries[1], snapshot->entries[2],
                       snapshot->entries[3]);
        lv_label_set_text(s_transcript_label, transcript);
        buddy_ui_show(s_transcript_page);
        return;
    }
    buddy_ui_create_status();
    buddy_ui_render_status(snapshot);
}

void buddy_ui_show_passkey(uint32_t passkey)
{
    buddy_ui_init();
    buddy_ui_create_passkey();
    lv_label_set_text_fmt(s_passkey_label, "%06lu", (unsigned long)passkey);
    buddy_ui_show(s_passkey_page);
}

void buddy_ui_tick(uint64_t elapsed_ms)
{
    if (s_character_label != NULL && !lv_obj_has_flag(s_status_page, LV_OBJ_FLAG_HIDDEN)) {
        lv_label_set_text(s_character_label, buddy_character_frame(s_character, elapsed_ms));
    }
}
