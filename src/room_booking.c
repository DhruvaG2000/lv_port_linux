#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/reboot.h>
#include "room_booking.h"
#include "lvgl/lvgl.h"

#define ROOM_NAME     "LV 4F C1"
#define SLOT_COUNT    24
#define SLOT_START_H  8
#define ROW_H         48
#define LIST_ROWS     3
#define STATE_FILE    "/root/.room_booking_state"

static const char * const ADJ_ROOMS[] = {
    "LV 4F C2", "LV 4F B1", "LV 4F B2", "LV 4F D1"
};

static bool       booked[SLOT_COUNT];
static int        sel = 0;

static lv_obj_t  *idle_scr;
static lv_obj_t  *booking_scr;
static lv_obj_t  *slot_rows[SLOT_COUNT];
static lv_obj_t  *sel_label;
static lv_obj_t  *adj_label;
static lv_obj_t  *adj_panel;
static lv_obj_t  *idle_slot_label;
static lv_obj_t  *idle_status_box;
static lv_obj_t  *idle_status_label;

static lv_group_t *idle_group;
static lv_group_t *booking_group;
static lv_group_t *poweroff_group;

static lv_obj_t  *bat_label;
static lv_obj_t  *book_room_btn;
static lv_obj_t  *poweroff_dlg;

static void save_state(void);

static void slot_range_str(int idx, char *buf, size_t len)
{
    int s = SLOT_START_H * 60 + idx * 30;
    int e = s + 30;
    lv_snprintf(buf, len, "%d:%02d - %d:%02d",
                s / 60, s % 60, e / 60, e % 60);
}

static void time_str(int idx, char *buf, size_t len)
{
    int m = SLOT_START_H * 60 + idx * 30;
    lv_snprintf(buf, len, "%d:%02d", m / 60, m % 60);
}

static int get_current_slot(void)
{
    return (11 * 60 - SLOT_START_H * 60) / 30;
}

static void refresh_idle_status(void)
{
    int slot = get_current_slot();
    char buf[32];
    slot_range_str(slot, buf, sizeof(buf));
    lv_label_set_text(idle_slot_label, buf);
    if(booked[slot]) {
        lv_label_set_text(idle_status_label, "BOOKED");
        lv_obj_set_style_bg_color(idle_status_box, lv_color_black(), 0);
        lv_obj_set_style_text_color(idle_status_label, lv_color_white(), 0);
    }
    else {
        lv_label_set_text(idle_status_label, "AVAILABLE");
        lv_obj_set_style_bg_color(idle_status_box, lv_color_white(), 0);
        lv_obj_set_style_text_color(idle_status_label, lv_color_black(), 0);
    }
}

static void idle_timer_cb(lv_timer_t *t) { (void)t; refresh_idle_status(); }

static void refresh_row(int idx)
{
    lv_obj_t *row = slot_rows[idx];
    bool is_sel   = (idx == sel);
    lv_color_t bg = is_sel ? lv_color_black() : lv_color_white();
    lv_color_t fg = is_sel ? lv_color_white() : lv_color_black();
    lv_obj_set_style_bg_color(row, bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_t *tlbl = lv_obj_get_child(row, 0);
    lv_obj_t *slbl = lv_obj_get_child(row, 1);
    lv_obj_set_style_text_color(tlbl, fg, 0);
    lv_obj_set_style_text_color(slbl, fg, 0);
    lv_label_set_text(slbl, booked[idx] ? "BOOKED" : "FREE");
}

static void refresh_sel_label(void)
{
    char buf[16];
    time_str(sel, buf, sizeof(buf));
    char full[28];
    lv_snprintf(full, sizeof(full), "Slot: %s", buf);
    lv_label_set_text(sel_label, full);
}

static void show_adj(bool visible)
{
    if(visible) {
        lv_obj_clear_flag(adj_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(adj_panel, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(adj_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(adj_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_focus_style_dark(lv_obj_t *obj)
{
    lv_obj_set_style_border_color(obj, lv_color_white(), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(obj, 3, LV_STATE_FOCUSED);
    lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_FULL, LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, LV_STATE_FOCUSED);
}

static void set_focus_style_light(lv_obj_t *obj)
{
    lv_obj_set_style_border_color(obj, lv_color_black(), LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(obj, 4, LV_STATE_FOCUSED);
    lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_FULL, LV_STATE_FOCUSED);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, LV_STATE_FOCUSED);
}

static void set_active_group(lv_group_t *group)
{
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while(indev != NULL) {
        lv_indev_type_t type = lv_indev_get_type(indev);
        if(type == LV_INDEV_TYPE_KEYPAD || type == LV_INDEV_TYPE_ENCODER) {
            lv_indev_set_group(indev, group);
        }
        indev = lv_indev_get_next(indev);
    }
    lv_group_set_default(group);
}

static void slot_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    sel = idx;
    booked[sel] = !booked[sel];
    refresh_row(sel);
    refresh_sel_label();
    show_adj(booked[sel]);
    save_state();
}

static void slot_focused_cb(lv_event_t *e)
{
    int idx  = (int)(intptr_t)lv_event_get_user_data(e);
    int prev = sel;
    sel = idx;
    if(prev != idx) {
        refresh_row(prev);
        refresh_row(sel);
        refresh_sel_label();
        show_adj(booked[sel]);
    }
    lv_obj_scroll_to_view(slot_rows[sel], LV_ANIM_ON);
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    set_active_group(idle_group);
    refresh_idle_status();
    lv_screen_load(idle_scr);
}

static void booking_esc_cb(lv_event_t *e)
{
    if(lv_event_get_key(e) == LV_KEY_ESC) back_cb(NULL);
}

static void book_room_cb(lv_event_t *e)
{
    (void)e;
    set_active_group(booking_group);
    lv_group_focus_obj(slot_rows[sel]);
    lv_screen_load(booking_scr);
}

static lv_obj_t *make_btn(lv_obj_t *parent, const char *text,
                           lv_color_t bg, lv_color_t fg,
                           int32_t w, int32_t h, const lv_font_t *font,
                           lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, lv_color_black(), 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    if(cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, fg, 0);
    lv_obj_center(lbl);
    return btn;
}

static lv_obj_t *make_sep(lv_obj_t *parent, int32_t w, int32_t y_ofs)
{
    lv_obj_t *sep = lv_obj_create(parent);
    lv_obj_set_size(sep, w, 3);
    lv_obj_set_style_bg_color(sep, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, y_ofs);
    return sep;
}

static void save_state(void)
{
    FILE *f = fopen(STATE_FILE, "w");
    if(!f) return;
    for(int i = 0; i < SLOT_COUNT; i++)
        fputc(booked[i] ? '1' : '0', f);
    fputc('\n', f);
    fclose(f);
}

static void load_state(void)
{
    FILE *f = fopen(STATE_FILE, "r");
    if(!f) {
        save_state();
        return;
    }
    char buf[SLOT_COUNT + 4];
    if(fgets(buf, (int)sizeof(buf), f)) {
        for(int i = 0; i < SLOT_COUNT && buf[i] != '\0' && buf[i] != '\n'; i++)
            booked[i] = (buf[i] == '1');
    }
    fclose(f);
}

static int read_bat_pct(void)
{
    static const char * const paths[] = {
        "/sys/class/power_supply/battery/capacity",
        "/sys/class/power_supply/BAT0/capacity",
        "/sys/class/power_supply/BAT/capacity",
        "/sys/class/power_supply/axp20x-battery/capacity",
        NULL
    };
    for(int i = 0; paths[i]; i++) {
        FILE *f = fopen(paths[i], "r");
        if(!f) continue;
        int pct = -1;
        fscanf(f, "%d", &pct);
        fclose(f);
        if(pct >= 0 && pct <= 100) return pct;
    }
    return -1;
}

static void bat_timer_cb(lv_timer_t *t)
{
    (void)t;
    if(!bat_label) return;
    int pct = read_bat_pct();
    char buf[12];
    if(pct < 0) {
        lv_label_set_text(bat_label, "AC");
    } else {
        snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(bat_label, buf);
    }
}

static void poweroff_close_async(void *param)
{
    (void)param;
    lv_obj_del(poweroff_dlg);
    poweroff_dlg = NULL;
    lv_group_del(poweroff_group);
    poweroff_group = NULL;
    set_active_group(idle_group);
}


static void poweroff_yes_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_del(poweroff_dlg);
    poweroff_dlg = NULL;
    lv_group_del(poweroff_group);
    poweroff_group = NULL;
    set_active_group(idle_group);
    lv_refr_now(lv_display_get_default());
    usleep(1200000);
    LV_LOG_USER("poweroff: syncing...");
    sync();
    LV_LOG_USER("poweroff: calling reboot(RB_POWER_OFF)");
    if(reboot(RB_POWER_OFF) < 0) {
        LV_LOG_USER("poweroff: reboot() failed, trying /sbin/poweroff");
        system("/sbin/poweroff");
    }
}

static void poweroff_key_cb(lv_event_t *e)
{
    if(lv_event_get_key(e) == LV_KEY_ESC)
        lv_async_call(poweroff_close_async, NULL);
}

static void show_poweroff_dlg(void)
{
    if(poweroff_dlg) return;

    poweroff_dlg = lv_obj_create(idle_scr);
    lv_obj_set_size(poweroff_dlg, 260, 120);
    lv_obj_set_style_bg_color(poweroff_dlg, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(poweroff_dlg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(poweroff_dlg, lv_color_black(), 0);
    lv_obj_set_style_border_width(poweroff_dlg, 2, 0);
    lv_obj_set_style_radius(poweroff_dlg, 4, 0);
    lv_obj_set_style_shadow_width(poweroff_dlg, 0, 0);
    lv_obj_set_style_pad_all(poweroff_dlg, 0, 0);
    lv_obj_clear_flag(poweroff_dlg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(poweroff_dlg);

    lv_obj_t *msg = lv_label_create(poweroff_dlg);
    lv_label_set_text(msg, "Done?");
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);
    lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 14);

    lv_obj_t *confirm = make_btn(poweroff_dlg, "Confirm",
                                 lv_color_black(), lv_color_white(),
                                 140, 44, &lv_font_montserrat_18, poweroff_yes_cb);
    lv_obj_align(confirm, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_add_event_cb(confirm, poweroff_key_cb, LV_EVENT_KEY, NULL);

    poweroff_group = lv_group_create();
    lv_group_add_obj(poweroff_group, confirm);
    set_active_group(poweroff_group);
}

static void idle_key_cb(lv_event_t *e)
{
    if(lv_event_get_key(e) == LV_KEY_ESC)
        show_poweroff_dlg();
}

static void build_idle_screen(void)
{
    idle_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(idle_scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(idle_scr, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(idle_scr);
    lv_label_set_text(title, ROOM_NAME);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    make_sep(idle_scr, 380, 54);

    idle_slot_label = lv_label_create(idle_scr);
    lv_label_set_text(idle_slot_label, "");
    lv_obj_set_style_text_font(idle_slot_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(idle_slot_label, lv_color_black(), 0);
    lv_obj_align(idle_slot_label, LV_ALIGN_TOP_MID, 0, 66);

    idle_status_box = lv_obj_create(idle_scr);
    lv_obj_set_size(idle_status_box, 340, 70);
    lv_obj_set_style_radius(idle_status_box, 0, 0);
    lv_obj_set_style_border_color(idle_status_box, lv_color_black(), 0);
    lv_obj_set_style_border_width(idle_status_box, 3, 0);
    lv_obj_set_style_bg_color(idle_status_box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(idle_status_box, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(idle_status_box, 0, 0);
    lv_obj_clear_flag(idle_status_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(idle_status_box, LV_ALIGN_TOP_MID, 0, 98);

    idle_status_label = lv_label_create(idle_status_box);
    lv_label_set_text(idle_status_label, "");
    lv_obj_set_style_text_font(idle_status_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(idle_status_label, lv_color_black(), 0);
    lv_obj_center(idle_status_label);

    make_sep(idle_scr, 380, 178);

    lv_obj_t *btn = make_btn(idle_scr, "BOOK ROOM",
                              lv_color_black(), lv_color_white(),
                              240, 78, &lv_font_montserrat_22,
                              book_room_cb);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 190);

    lv_obj_add_event_cb(btn, idle_key_cb, LV_EVENT_KEY, NULL);
    book_room_btn = btn;

    bat_label = lv_label_create(idle_scr);
    lv_label_set_text(bat_label, "--");
    lv_obj_set_style_text_font(bat_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bat_label, lv_color_black(), 0);
    lv_obj_align(bat_label, LV_ALIGN_BOTTOM_RIGHT, -8, -8);

    idle_group = lv_group_create();
    lv_group_add_obj(idle_group, btn);
    set_focus_style_dark(btn);
}

static void build_booking_screen(void)
{
#define LIST_Y      55
#define LIST_H      (ROW_H * LIST_ROWS)
#define BAR_Y       (LIST_Y + LIST_H + 6)
#define ADJ_LABEL_Y (BAR_Y + 42 + 4)
#define ADJ_PANEL_Y (ADJ_LABEL_Y + 18)

    booking_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(booking_scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(booking_scr, LV_OPA_COVER, 0);

    booking_group = lv_group_create();

    lv_obj_t *title = lv_label_create(booking_scr);
    lv_label_set_text(title, ROOM_NAME);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 12, 10);

    lv_obj_t *back = make_btn(booking_scr, "< Back",
                               lv_color_white(), lv_color_black(),
                               90, 40, &lv_font_montserrat_16, back_cb);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -10, 6);

    make_sep(booking_scr, 380, 52);

    lv_obj_t *list = lv_obj_create(booking_scr);
    lv_obj_set_size(list, 380, LIST_H);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(list, lv_color_black(), 0);
    lv_obj_set_style_border_width(list, 2, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_layout(list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, LIST_Y);

    char tbuf[12];
    for(int i = 0; i < SLOT_COUNT; i++) {
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_set_size(row, lv_pct(100), ROW_H);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_bg_color(row, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_border_color(row, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
        lv_obj_set_style_pad_left(row, 12, 0);
        lv_obj_set_style_pad_right(row, 12, 0);
        lv_obj_set_style_pad_top(row, 0, 0);
        lv_obj_set_style_pad_bottom(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_layout(row, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, slot_click_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        lv_obj_add_event_cb(row, slot_focused_cb, LV_EVENT_FOCUSED,
                            (void *)(intptr_t)i);
        slot_rows[i] = row;
        lv_group_add_obj(booking_group, row);
        set_focus_style_dark(row);
        lv_obj_add_event_cb(row, booking_esc_cb, LV_EVENT_KEY, NULL);

        time_str(i, tbuf, sizeof(tbuf));
        lv_obj_t *t = lv_label_create(row);
        lv_label_set_text(t, tbuf);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(t, lv_color_black(), 0);

        lv_obj_t *s = lv_label_create(row);
        lv_label_set_text(s, "FREE");
        lv_obj_set_style_text_font(s, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(s, lv_color_black(), 0);
    }

    make_sep(booking_scr, 380, LIST_Y + LIST_H);

    lv_obj_t *bar = lv_obj_create(booking_scr);
    lv_obj_set_size(bar, 380, 42);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_layout(bar, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, BAR_Y);

    sel_label = lv_label_create(bar);
    lv_label_set_text(sel_label, "Slot: 8:00");
    lv_obj_set_style_text_font(sel_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(sel_label, lv_color_black(), 0);

    adj_label = lv_label_create(booking_scr);
    lv_label_set_text(adj_label, "Alternate free rooms:");
    lv_obj_set_style_text_font(adj_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(adj_label, lv_color_black(), 0);
    lv_obj_align(adj_label, LV_ALIGN_TOP_LEFT, 12, ADJ_LABEL_Y);
    lv_obj_add_flag(adj_label, LV_OBJ_FLAG_HIDDEN);

    adj_panel = lv_obj_create(booking_scr);
    lv_obj_set_size(adj_panel, 380, 35);
    lv_obj_set_style_bg_opa(adj_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(adj_panel, 0, 0);
    lv_obj_set_style_pad_all(adj_panel, 0, 0);
    lv_obj_set_layout(adj_panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(adj_panel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(adj_panel, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(adj_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(adj_panel, LV_ALIGN_TOP_MID, 0, ADJ_PANEL_Y);
    lv_obj_add_flag(adj_panel, LV_OBJ_FLAG_HIDDEN);

    for(int i = 0; i < 4; i++) {
        lv_obj_t *adj_btn = make_btn(adj_panel, ADJ_ROOMS[i],
                                     lv_color_white(), lv_color_black(),
                                     86, 30, &lv_font_montserrat_12, NULL);
        lv_group_add_obj(booking_group, adj_btn);
        set_focus_style_light(adj_btn);
        lv_obj_add_event_cb(adj_btn, booking_esc_cb, LV_EVENT_KEY, NULL);
    }

    lv_group_add_obj(booking_group, back);
    set_focus_style_light(back);
    lv_obj_add_event_cb(back, booking_esc_cb, LV_EVENT_KEY, NULL);

    refresh_row(0);

#undef LIST_Y
#undef LIST_H
#undef BAR_Y
#undef ADJ_LABEL_Y
#undef ADJ_PANEL_Y
}

void room_booking_ui_create(void)
{
    load_state();
    build_idle_screen();
    build_booking_screen();
    refresh_idle_status();
    lv_timer_create(idle_timer_cb, 10000, NULL);
    lv_timer_create(bat_timer_cb, 30000, NULL);
    bat_timer_cb(NULL);
    set_active_group(idle_group);
    lv_screen_load(idle_scr);
}
