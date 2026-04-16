#include <stdbool.h>
#include "room_booking.h"
#include "lvgl/lvgl.h"

#define ROOM_NAME "LV 4F C1"

static bool        is_booked = false;
static lv_obj_t   *status_box;
static lv_obj_t   *status_label;

static void update_status(void)
{
    if(is_booked) {
        lv_obj_set_style_bg_color(status_box, lv_color_black(), 0);
        lv_label_set_text(status_label, "BOOKED");
        lv_obj_set_style_text_color(status_label, lv_color_white(), 0);
    }
    else {
        lv_obj_set_style_bg_color(status_box, lv_color_white(), 0);
        lv_label_set_text(status_label, "AVAILABLE");
        lv_obj_set_style_text_color(status_label, lv_color_black(), 0);
    }
}

static void book_cb(lv_event_t *e)
{
    (void)e;
    is_booked = true;
    update_status();
}

static void unbook_cb(lv_event_t *e)
{
    (void)e;
    is_booked = false;
    update_status();
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text,
                              lv_color_t bg, lv_color_t fg,
                              lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 300, 120);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, lv_color_black(), 0);
    lv_obj_set_style_border_width(btn, 4, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(lbl, fg, 0);
    lv_obj_center(lbl);

    return btn;
}

void room_booking_ui_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, ROOM_NAME);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t *sep = lv_obj_create(scr);
    lv_obj_set_size(sep, 740, 4);
    lv_obj_set_style_bg_color(sep, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_radius(sep, 0, 0);
    lv_obj_set_style_pad_all(sep, 0, 0);
    lv_obj_align(sep, LV_ALIGN_TOP_MID, 0, 106);

    status_box = lv_obj_create(scr);
    lv_obj_set_size(status_box, 560, 130);
    lv_obj_set_style_radius(status_box, 0, 0);
    lv_obj_set_style_border_color(status_box, lv_color_black(), 0);
    lv_obj_set_style_border_width(status_box, 4, 0);
    lv_obj_set_style_bg_color(status_box, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(status_box, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(status_box, 0, 0);
    lv_obj_clear_flag(status_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(status_box, LV_ALIGN_CENTER, 0, -20);

    status_label = lv_label_create(status_box);
    lv_label_set_text(status_label, "AVAILABLE");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(status_label, lv_color_black(), 0);
    lv_obj_center(status_label);

    lv_obj_t *btn_cont = lv_obj_create(scr);
    lv_obj_set_size(btn_cont, 740, 140);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_style_pad_all(btn_cont, 0, 0);
    lv_obj_set_layout(btn_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(btn_cont, LV_ALIGN_BOTTOM_MID, 0, -24);

    make_button(btn_cont, "BOOK",   lv_color_black(), lv_color_white(), book_cb);
    make_button(btn_cont, "UNBOOK", lv_color_white(), lv_color_black(), unbook_cb);
}
