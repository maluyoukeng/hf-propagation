#include "hf_app_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "lvgl.h"

LV_FONT_DECLARE(hf_font_14)
LV_FONT_DECLARE(hf_font_16)
LV_FONT_DECLARE(hf_font_20)
LV_FONT_DECLARE(hf_font_28)

#define COL_BG 0x070C16
#define COL_CARD 0x0E1826
#define COL_CARD_ALT 0x0B1420
#define COL_LINE 0x1D2D40
#define COL_TEXT 0xEAF4F8
#define COL_DIM 0x7E95A9
#define COL_CYAN 0x36D6E8
#define COL_AMBER 0xF4B942
#define COL_GREEN 0x35D07F
#define COL_RED 0xE45B4E
#define COL_SELECT 0x143149

static lv_obj_t *s_screen;
static hf_page_t s_page = (hf_page_t)-1;

static lv_obj_t *s_battery_label;
static lv_obj_t *s_battery_fill;

static lv_obj_t *s_muf_value;
static lv_obj_t *s_rating_badge;
static lv_obj_t *s_rating_label;
static lv_obj_t *s_summary_label;
static lv_obj_t *s_band_row[PROP_BAND_COUNT];
static lv_obj_t *s_band_name[PROP_BAND_COUNT];
static lv_obj_t *s_band_freq[PROP_BAND_COUNT];
static lv_obj_t *s_band_rating[PROP_BAND_COUNT];
static lv_obj_t *s_band_score[PROP_BAND_COUNT];
static lv_obj_t *s_band_bar[PROP_BAND_COUNT][5];

static lv_obj_t *s_detail_band_label;
static lv_obj_t *s_detail_arc;
static lv_obj_t *s_detail_score;
static lv_obj_t *s_detail_rating;
static lv_obj_t *s_detail_window;
static lv_obj_t *s_detail_daylight;
static lv_obj_t *s_detail_distance;
static lv_obj_t *s_detail_bearing;
static lv_obj_t *s_detail_hint;

static lv_obj_t *s_menu_row[5];

static lv_obj_t *s_setting_row[APP_SETTING_COUNT];
static lv_obj_t *s_setting_value[APP_SETTING_COUNT];
static lv_obj_t *s_setting_location;
static lv_obj_t *s_settings_hint;

static lv_obj_t *s_hint_label;
static lv_obj_t *s_about_card;
static bool s_mem_logged;

static lv_obj_t *s_wifi_state;
static lv_obj_t *s_wifi_ap_ssid;
static lv_obj_t *s_wifi_ap_password;
static lv_obj_t *s_wifi_url;
static lv_obj_t *s_wifi_saved;
static lv_obj_t *s_wifi_hint;

static lv_obj_t *s_data_state;
static lv_obj_t *s_data_ssid;
static lv_obj_t *s_data_result;
static lv_obj_t *s_data_issued;
static lv_obj_t *s_data_location;
static lv_obj_t *s_data_hint;

static const char *const MENU_TITLES[5] = {
    "传播预测", "参数设置", "使用说明", "WiFi 设置", "数据更新",
};
static const char *const MENU_SUBTITLES[5] = {
    "总览与波段建议", "站点、指数与时间", "模型与操作方法",
    "配置路由器 WiFi", "NOAA 实时指数",
};

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    return label;
}

static lv_obj_t *make_box(lv_obj_t *parent, int x, int y, int w, int h,
                          uint32_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    return obj;
}

static lv_obj_t *make_card(lv_obj_t *parent, int x, int y, int w, int h,
                           uint32_t color, int radius)
{
    lv_obj_t *card = make_box(parent, x, y, w, h, color, radius);
    lv_obj_set_style_border_color(card, lv_color_hex(COL_LINE), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    return card;
}

static void build_top_bar(lv_obj_t *screen, const char *title,
                          const char *subtitle)
{
    lv_obj_t *title_label = make_label(screen, title, &hf_font_20, COL_TEXT);
    lv_obj_set_pos(title_label, 24, 6);

    lv_obj_t *sub_label = make_label(screen, subtitle, &hf_font_14, COL_DIM);
    lv_obj_set_pos(sub_label, 24, 30);

    s_battery_label = make_label(screen, "--", &hf_font_14, COL_DIM);
    lv_obj_set_pos(s_battery_label, 158, 8);
    lv_obj_set_width(s_battery_label, 40);
    lv_obj_set_style_text_align(s_battery_label, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_t *body = make_box(screen, 204, 10, 24, 12, COL_BG, 2);
    lv_obj_set_style_border_color(body, lv_color_hex(COL_DIM), 0);
    lv_obj_set_style_border_width(body, 1, 0);
    make_box(screen, 228, 14, 3, 5, COL_DIM, 1);
    s_battery_fill = make_box(body, 2, 2, 20, 8, COL_GREEN, 1);

    make_box(screen, 0, 46, 240, 1, COL_LINE, 0);
}

static lv_obj_t *build_page(hf_page_t page, const hf_view_t *view)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    s_battery_label = NULL;
    s_battery_fill = NULL;
    s_about_card = NULL;

    if (page == HF_PAGE_OVERVIEW) {
        build_top_bar(screen, "短波传播", "HF PROP  离线传播预测");

        lv_obj_t *hero = make_card(screen, 12, 52, 216, 82, COL_CARD, 12);
        lv_obj_set_style_bg_grad_color(hero, lv_color_hex(0x0A1320), 0);
        lv_obj_set_style_bg_grad_dir(hero, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_color(hero, lv_color_hex(0x245064), 0);

        lv_obj_t *muf_caption = make_label(hero, "最高可用频率  MUF", &hf_font_14,
                                           COL_DIM);
        lv_obj_set_pos(muf_caption, 12, 8);

        s_muf_value = make_label(hero, "--", &hf_font_28, COL_TEXT);
        lv_obj_set_pos(s_muf_value, 10, 24);

        lv_obj_t *muf_unit = make_label(hero, "MHz", &hf_font_14, COL_CYAN);
        lv_obj_set_pos(muf_unit, 84, 39);

        s_rating_badge = make_box(hero, 158, 14, 46, 30, COL_SELECT, 9);
        s_rating_label = make_label(s_rating_badge, "--", &hf_font_16, COL_TEXT);
        lv_obj_center(s_rating_label);

        s_summary_label = make_label(hero, "SFI --   Kp -   UTC --:--",
                                     &hf_font_14, COL_DIM);
        lv_obj_set_pos(s_summary_label, 12, 58);

        for (int i = 0; i < PROP_BAND_COUNT; i++) {
            lv_obj_t *row = make_box(screen, 12, 140 + i * 19, 216, 18,
                                     COL_CARD_ALT, 5);
            lv_obj_set_style_border_width(row, 1, 0);
            lv_obj_set_style_border_color(row, lv_color_hex(COL_CARD_ALT), 0);
            s_band_row[i] = row;

            s_band_name[i] = make_label(row, prop_band_name(i), &hf_font_14,
                                        COL_TEXT);
            lv_obj_set_pos(s_band_name[i], 8, 1);

            s_band_freq[i] = make_label(row, "--", &hf_font_14, COL_DIM);
            lv_obj_set_pos(s_band_freq[i], 44, 1);
            lv_obj_set_width(s_band_freq[i], 38);

            for (int bar = 0; bar < 5; bar++) {
                s_band_bar[i][bar] = make_box(row, 90 + bar * 11, 5, 8, 8,
                                              COL_LINE, 2);
            }

            s_band_rating[i] = make_label(row, "--", &hf_font_14, COL_DIM);
            lv_obj_set_pos(s_band_rating[i], 148, 1);
            lv_obj_set_width(s_band_rating[i], 36);

            s_band_score[i] = make_label(row, "--", &hf_font_14, COL_DIM);
            lv_obj_set_pos(s_band_score[i], 184, 1);
            lv_obj_set_width(s_band_score[i], 26);
            lv_obj_set_style_text_align(s_band_score[i], LV_TEXT_ALIGN_RIGHT, 0);
        }

        s_hint_label = make_label(screen, "上/下选择  OK详情  长按菜单",
                                  &hf_font_14, COL_DIM);
        lv_obj_set_pos(s_hint_label, 12, 298);
    } else if (page == HF_PAGE_MENU) {
        build_top_bar(screen, "主菜单", "HF PROP  功能导航");

        for (int i = 0; i < 5; i++) {
            lv_obj_t *row = make_card(screen, 12, 48 + i * 46, 216, 44,
                                      COL_CARD, 10);
            s_menu_row[i] = row;
            lv_obj_t *title = make_label(row, MENU_TITLES[i], &hf_font_16,
                                         COL_TEXT);
            lv_obj_set_pos(title, 14, 3);
            lv_obj_t *sub = make_label(row, MENU_SUBTITLES[i], &hf_font_14,
                                       COL_DIM);
            lv_obj_set_pos(sub, 14, 23);
        }

        s_hint_label = make_label(screen, "上/下选择  OK进入  长按返回",
                                  &hf_font_14, COL_DIM);
        lv_obj_set_pos(s_hint_label, 12, 298);
    } else if (page == HF_PAGE_DETAIL) {
        build_top_bar(screen, "波段详情", "传播可靠性估算");

        s_detail_band_label = make_label(screen, "--", &hf_font_14, COL_CYAN);
        lv_obj_set_pos(s_detail_band_label, 158, 29);
        lv_obj_set_width(s_detail_band_label, 70);
        lv_obj_set_style_text_align(s_detail_band_label, LV_TEXT_ALIGN_RIGHT, 0);

        s_detail_arc = lv_arc_create(screen);
        lv_obj_set_size(s_detail_arc, 128, 128);
        lv_obj_set_pos(s_detail_arc, 56, 54);
        lv_arc_set_rotation(s_detail_arc, 135);
        lv_arc_set_bg_angles(s_detail_arc, 0, 270);
        lv_arc_set_range(s_detail_arc, 0, 100);
        lv_obj_remove_style(s_detail_arc, NULL, LV_PART_KNOB);
        lv_obj_remove_flag(s_detail_arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(s_detail_arc, 9, LV_PART_MAIN);
        lv_obj_set_style_arc_color(s_detail_arc, lv_color_hex(COL_LINE),
                                   LV_PART_MAIN);
        lv_obj_set_style_arc_width(s_detail_arc, 9, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(s_detail_arc, true, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(s_detail_arc, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_detail_arc, 0, LV_PART_MAIN);

        s_detail_score = make_label(screen, "--", &hf_font_28, COL_TEXT);
        lv_obj_set_pos(s_detail_score, 56, 100);
        lv_obj_set_width(s_detail_score, 128);
        lv_obj_set_style_text_align(s_detail_score, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t *caption = make_label(screen, "可靠性", &hf_font_14, COL_DIM);
        lv_obj_set_pos(caption, 56, 133);
        lv_obj_set_width(caption, 128);
        lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);

        s_detail_rating = make_label(screen, "--", &hf_font_16, COL_TEXT);
        lv_obj_set_pos(s_detail_rating, 56, 151);
        lv_obj_set_width(s_detail_rating, 128);
        lv_obj_set_style_text_align(s_detail_rating, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t *info = make_card(screen, 12, 190, 216, 92, COL_CARD, 10);
        const char *labels[4] = {"最佳时段", "昼间路径", "距离", "方位"};
        lv_obj_t **values[4] = {
            &s_detail_window, &s_detail_daylight, &s_detail_distance,
            &s_detail_bearing,
        };
        for (int i = 0; i < 4; i++) {
            lv_obj_t *label = make_label(info, labels[i], &hf_font_14, COL_DIM);
            lv_obj_set_pos(label, 12, 7 + i * 21);
            lv_obj_t *value = make_label(info, "--", &hf_font_14, COL_TEXT);
            lv_obj_set_pos(value, 86, 7 + i * 21);
            lv_obj_set_width(value, 118);
            lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
            *values[i] = value;
        }

        s_detail_hint = make_label(screen, "上/下切换波段  长按返回",
                                   &hf_font_14, COL_DIM);
        lv_obj_set_pos(s_detail_hint, 12, 298);
    } else if (page == HF_PAGE_SETTINGS) {
        build_top_bar(screen, "参数设置", "手动输入太阳与地磁指数");

        const char *labels[APP_SETTING_COUNT] = {
            "站点", "目标", "时区", "太阳通量 SFI", "Kp 指数",
            "UTC 时间", "日期",
        };
        for (int i = 0; i < APP_SETTING_COUNT; i++) {
            lv_obj_t *row = make_box(screen, 12, 52 + i * 28, 216, 27,
                                     COL_CARD_ALT, 5);
            lv_obj_set_style_border_width(row, 1, 0);
            lv_obj_set_style_border_color(row, lv_color_hex(COL_CARD_ALT), 0);
            s_setting_row[i] = row;

            lv_obj_t *label = make_label(row, labels[i], &hf_font_14, COL_DIM);
            lv_obj_set_pos(label, 8, 4);

            lv_obj_t *value = make_label(row, "--", &hf_font_16, COL_TEXT);
            lv_obj_set_pos(value, 112, 3);
            lv_obj_set_width(value, 96);
            lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
            s_setting_value[i] = value;
        }

        lv_obj_t *location_row = make_box(screen, 12, 52 + APP_SETTING_COUNT * 28,
                                          216, 27, COL_CARD_ALT, 5);
        lv_obj_set_style_border_width(location_row, 1, 0);
        lv_obj_set_style_border_color(location_row, lv_color_hex(COL_CARD_ALT), 0);
        lv_obj_t *location_label = make_label(location_row, "定位",
                                              &hf_font_14, COL_DIM);
        lv_obj_set_pos(location_label, 8, 4);
        s_setting_location = make_label(location_row, "未定位",
                                        &hf_font_14, COL_TEXT);
        lv_obj_set_pos(s_setting_location, 112, 4);
        lv_obj_set_width(s_setting_location, 96);
        lv_obj_set_style_text_align(s_setting_location, LV_TEXT_ALIGN_RIGHT, 0);

        s_settings_hint = make_label(screen, "上/下选择  OK编辑  长按保存",
                                     &hf_font_14, COL_DIM);
        lv_obj_set_pos(s_settings_hint, 12, 298);
    } else if (page == HF_PAGE_ABOUT) {
        build_top_bar(screen, "使用说明", "内置模型与操作");

        lv_obj_t *card = make_card(screen, 12, 52, 216, 238, COL_CARD, 10);
        s_about_card = card;
        lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(card, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_t *text = make_label(card,
            "离线短波传播估算\n\n"
            "应用结合太阳通量 SFI、Kp 指数、昼夜路径与站点距离，"
            "估算 MUF/LUF 及各波段可用性。\n\n"
            "1. 在参数设置中填写当天 SFI 与 Kp。\n"
            "2. 选择发射站点和目标区域。\n"
            "3. 总览查看最佳波段，进入详情查看时段。\n\n"
            "预测仅供业余无线电通联规划参考，不替代专业软件或实测结果。\n\n"
            "上/下选择，短按 OK 进入或确认，长按 OK 返回。",
            &hf_font_14, COL_TEXT);
        lv_obj_set_pos(text, 10, 8);
        lv_obj_set_width(text, 194);
        lv_obj_set_height(text, LV_SIZE_CONTENT);
        lv_label_set_long_mode(text, LV_LABEL_LONG_MODE_WRAP);

        s_hint_label = make_label(screen, "上/下滚动  OK返回", &hf_font_14,
                                  COL_DIM);
        lv_obj_set_pos(s_hint_label, 12, 298);
    } else if (page == HF_PAGE_WIFI) {
        build_top_bar(screen, "WiFi 设置", "设备热点配置路由器");

        lv_obj_t *card = make_card(screen, 12, 52, 216, 236, COL_CARD, 10);

        s_wifi_state = make_label(card, "--", &hf_font_16, COL_TEXT);
        lv_obj_set_pos(s_wifi_state, 12, 8);
        lv_obj_set_width(s_wifi_state, 192);

        s_wifi_ap_ssid = make_label(card, "--", &hf_font_14, COL_TEXT);
        lv_obj_set_pos(s_wifi_ap_ssid, 12, 36);
        lv_obj_set_width(s_wifi_ap_ssid, 192);

        s_wifi_ap_password = make_label(card, "--", &hf_font_14, COL_TEXT);
        lv_obj_set_pos(s_wifi_ap_password, 12, 56);
        lv_obj_set_width(s_wifi_ap_password, 192);

        s_wifi_url = make_label(card, "--", &hf_font_14, COL_CYAN);
        lv_obj_set_pos(s_wifi_url, 12, 76);
        lv_obj_set_width(s_wifi_url, 192);

        s_wifi_saved = make_label(card, "路由器 WiFi  未配置", &hf_font_14,
                                  COL_DIM);
        lv_obj_set_pos(s_wifi_saved, 12, 100);
        lv_obj_set_width(s_wifi_saved, 192);
        lv_obj_set_height(s_wifi_saved, 34);
        lv_label_set_long_mode(s_wifi_saved, LV_LABEL_LONG_MODE_WRAP);

        lv_obj_t *steps = make_label(card,
            "1. 连接上方设备热点\n"
            "2. 浏览器打开 192.168.4.1\n"
            "3. 填写路由器 WiFi 并保存",
            &hf_font_14, COL_TEXT);
        lv_obj_set_pos(steps, 12, 140);
        lv_obj_set_width(steps, 192);
        lv_obj_set_height(steps, 52);

        s_wifi_hint = make_label(screen, "OK 开启热点  长按返回",
                                 &hf_font_14, COL_DIM);
        lv_obj_set_pos(s_wifi_hint, 12, 298);
    } else {
        build_top_bar(screen, "数据更新", "NOAA SWPC 实时指数");

        lv_obj_t *card = make_card(screen, 12, 52, 216, 236, COL_CARD, 10);

        s_data_state = make_label(card, "--", &hf_font_16, COL_TEXT);
        lv_obj_set_pos(s_data_state, 12, 8);
        lv_obj_set_width(s_data_state, 192);

        s_data_ssid = make_label(card, "WiFi  --", &hf_font_14, COL_DIM);
        lv_obj_set_pos(s_data_ssid, 12, 36);
        lv_obj_set_width(s_data_ssid, 192);

        s_data_result = make_label(card, "SFI --   Kp -   A --",
                                   &hf_font_16, COL_TEXT);
        lv_obj_set_pos(s_data_result, 12, 64);
        lv_obj_set_width(s_data_result, 192);

        s_data_issued = make_label(card, "发布时间  --", &hf_font_14, COL_DIM);
        lv_obj_set_pos(s_data_issued, 12, 96);
        lv_obj_set_width(s_data_issued, 192);
        lv_obj_set_height(s_data_issued, 34);
        lv_label_set_long_mode(s_data_issued, LV_LABEL_LONG_MODE_WRAP);

        s_data_location = make_label(card, "位置  --", &hf_font_14, COL_DIM);
        lv_obj_set_pos(s_data_location, 12, 130);
        lv_obj_set_width(s_data_location, 192);

        lv_obj_t *note = make_label(card,
            "更新成功后会自动写入 SFI 和 Kp，并重新计算各波段传播。",
            &hf_font_14, COL_TEXT);
        lv_obj_set_pos(note, 12, 158);
        lv_obj_set_width(note, 192);
        lv_obj_set_height(note, 64);
        lv_label_set_long_mode(note, LV_LABEL_LONG_MODE_WRAP);

        s_data_hint = make_label(screen, "OK 更新数据  长按返回",
                                 &hf_font_14, COL_DIM);
        lv_obj_set_pos(s_data_hint, 12, 298);
    }

    (void)view;
    return screen;
}

static void set_battery(const hf_view_t *view)
{
    if (!s_battery_label || !s_battery_fill) return;
    char text[16];
    if (view->battery_percent < 0) {
        lv_label_set_text(s_battery_label, "--");
        lv_obj_add_flag(s_battery_fill, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const int percent = view->battery_percent > 100 ? 100 : view->battery_percent;
    snprintf(text, sizeof(text), "%d%%", percent);
    lv_label_set_text(s_battery_label, text);
    lv_obj_remove_flag(s_battery_fill, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(s_battery_fill, percent * 20 / 100);
    lv_obj_set_style_bg_color(s_battery_fill,
        lv_color_hex(percent >= 50 ? COL_GREEN : (percent >= 20 ? COL_AMBER : COL_RED)),
        0);
}

static void update_overview(const hf_view_t *view)
{
    char text[64];
    const prop_forecast_t *forecast = &view->forecast;

    snprintf(text, sizeof(text), "%.1f", (double)forecast->conditions.muf_mhz);
    lv_label_set_text(s_muf_value, text);

    const uint32_t color = prop_rating_color(forecast->overall_rating);
    lv_label_set_text(s_rating_label, prop_rating_text(forecast->overall_rating));
    lv_obj_set_style_text_color(s_rating_label, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(s_rating_badge, lv_color_hex(COL_SELECT), 0);
    lv_obj_set_style_border_color(s_rating_badge, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(s_rating_badge, 1, 0);

    const int64_t local_epoch =
        view->epoch + (int64_t)view->settings.utc_offset_min * 60;
    prop_utc_t local;
    prop_utc_from_epoch(local_epoch, &local);
    snprintf(text, sizeof(text), "SFI %u   Kp %u   本地 %02u:%02u",
             (unsigned)view->settings.sfi, (unsigned)view->settings.kp,
             (unsigned)local.hour, (unsigned)local.minute);
    lv_label_set_text(s_summary_label, text);

    for (int i = 0; i < PROP_BAND_COUNT; i++) {
        const prop_band_t *band = &forecast->bands[i];
        snprintf(text, sizeof(text), "%.1f", (double)band->frequency_mhz);
        lv_label_set_text(s_band_freq[i], text);
        lv_label_set_text(s_band_rating[i], prop_rating_text(band->rating));
        lv_obj_set_style_text_color(s_band_rating[i],
                                    lv_color_hex(prop_rating_color(band->rating)),
                                    0);

        snprintf(text, sizeof(text), "%d", band->score);
        lv_label_set_text(s_band_score[i], text);

        const int lit = (band->score + 19) / 20;
        for (int bar = 0; bar < 5; bar++) {
            lv_obj_set_style_bg_color(s_band_bar[i][bar],
                lv_color_hex(bar < lit ? prop_rating_color(band->rating) : COL_LINE),
                0);
        }

        const bool selected = (i == view->band_index);
        lv_obj_set_style_bg_color(s_band_row[i],
            lv_color_hex(selected ? COL_SELECT : COL_CARD_ALT), 0);
        lv_obj_set_style_border_color(s_band_row[i],
            lv_color_hex(selected ? COL_CYAN : COL_CARD_ALT), 0);
        lv_obj_set_style_border_width(s_band_row[i], selected ? 1 : 0, 0);
    }
}

static void update_detail(const hf_view_t *view)
{
    char text[64];
    const int band = view->detail_band;
    const prop_band_t *data = &view->forecast.bands[band];
    const uint32_t color = prop_rating_color(data->rating);

    snprintf(text, sizeof(text), "%s %.1f", prop_band_name(band),
             (double)data->frequency_mhz);
    lv_label_set_text(s_detail_band_label, text);

    lv_arc_set_value(s_detail_arc, data->score);
    lv_obj_set_style_arc_color(s_detail_arc, lv_color_hex(color),
                               LV_PART_INDICATOR);
    snprintf(text, sizeof(text), "%d", data->score);
    lv_label_set_text(s_detail_score, text);
    lv_obj_set_style_text_color(s_detail_score, lv_color_hex(color), 0);
    lv_label_set_text(s_detail_rating, prop_rating_text(data->rating));
    lv_obj_set_style_text_color(s_detail_rating, lv_color_hex(color), 0);

    const int start = view->window.start_minute;
    if (view->window.end_minute >= 1440) {
        snprintf(text, sizeof(text), "%02d:%02d-24:00 UTC",
                 start / 60, start % 60);
    } else {
        const int end = view->window.end_minute;
        snprintf(text, sizeof(text), "%02d:%02d-%02d:%02d UTC",
                 start / 60, start % 60, end / 60, end % 60);
    }
    lv_label_set_text(s_detail_window, text);

    snprintf(text, sizeof(text), "%d%%",
             (int)(view->forecast.conditions.daylight_fraction * 100.0f + 0.5f));
    lv_label_set_text(s_detail_daylight, text);

    snprintf(text, sizeof(text), "%d km",
             (int)(view->forecast.conditions.distance_km + 0.5f));
    lv_label_set_text(s_detail_distance, text);

    snprintf(text, sizeof(text), "%d°",
             (int)(view->forecast.conditions.bearing_deg + 0.5f));
    lv_label_set_text(s_detail_bearing, text);
}

static void update_menu(const hf_view_t *view)
{
    for (int i = 0; i < 5; i++) {
        const bool selected = (i == view->menu_index);
        lv_obj_set_style_bg_color(s_menu_row[i],
            lv_color_hex(selected ? COL_SELECT : COL_CARD), 0);
        lv_obj_set_style_border_color(s_menu_row[i],
            lv_color_hex(selected ? COL_CYAN : COL_LINE), 0);
    }
}

static void update_wifi_page(const hf_view_t *view)
{
    const app_wifi_status_t *status = &view->wifi;
    char text[128];

    snprintf(text, sizeof(text), "热点  %s", status->ap_ssid);
    lv_label_set_text(s_wifi_ap_ssid, text);
    snprintf(text, sizeof(text), "密码  %s", status->ap_password);
    lv_label_set_text(s_wifi_ap_password, text);
    lv_label_set_text(s_wifi_url, "网页  http://192.168.4.1");
    if (status->has_config) {
        snprintf(text, sizeof(text), "路由器 WiFi  %s", status->router_ssid);
    } else {
        snprintf(text, sizeof(text), "路由器 WiFi  未配置");
    }
    lv_label_set_text(s_wifi_saved, text);

    const char *state_text = "按 OK 开启配置热点";
    uint32_t color = COL_DIM;
    switch (status->state) {
    case APP_WIFI_PORTAL_STARTING:
        state_text = "正在开启热点...";
        color = COL_AMBER;
        break;
    case APP_WIFI_PORTAL_READY:
        state_text = "热点已开启，等待配置";
        color = COL_GREEN;
        break;
    case APP_WIFI_CONFIG_SAVED:
        state_text = "WiFi 已保存";
        color = COL_GREEN;
        break;
    case APP_WIFI_FAILED:
        state_text = "操作失败，请重试";
        color = COL_RED;
        break;
    case APP_WIFI_CONNECTING:
    case APP_WIFI_LOCATING:
    case APP_WIFI_FETCHING:
    case APP_WIFI_SUCCESS:
        state_text = "请进入数据更新";
        color = COL_CYAN;
        break;
    default:
        break;
    }
    lv_label_set_text(s_wifi_state, state_text);
    lv_obj_set_style_text_color(s_wifi_state, lv_color_hex(color), 0);

    const char *hint = "OK 开启热点  长按返回";
    if (status->state == APP_WIFI_PORTAL_STARTING) hint = "请稍候...";
    else if (status->state == APP_WIFI_PORTAL_READY) hint = "OK 停止热点  长按返回";
    else if (status->state == APP_WIFI_CONNECTING ||
             status->state == APP_WIFI_LOCATING ||
             status->state == APP_WIFI_FETCHING) hint = "正在更新，请稍候...";
    lv_label_set_text(s_wifi_hint, hint);
}

static void update_data_page(const hf_view_t *view)
{
    const app_wifi_status_t *status = &view->wifi;
    char text[128];

    if (status->has_config) {
        snprintf(text, sizeof(text), "WiFi  %s", status->router_ssid);
    } else {
        snprintf(text, sizeof(text), "WiFi  未配置");
    }
    lv_label_set_text(s_data_ssid, text);

    if (status->sfi > 0) {
        snprintf(text, sizeof(text), "SFI %d   Kp %d   A %d",
                 status->sfi, status->kp, status->a_index);
    } else {
        snprintf(text, sizeof(text), "SFI --   Kp -   A --");
    }
    lv_label_set_text(s_data_result, text);

    if (status->issued[0]) {
        snprintf(text, sizeof(text), "发布时间  %s", status->issued);
    } else {
        snprintf(text, sizeof(text), "发布时间  --");
    }
    lv_label_set_text(s_data_issued, text);

    if (view->settings.has_location) {
        const char lat_dir = view->settings.latitude >= 0.0f ? 'N' : 'S';
        const char lon_dir = view->settings.longitude >= 0.0f ? 'E' : 'W';
        snprintf(text, sizeof(text), "位置  %.1f%c %.1f%c",
                 (double)fabsf(view->settings.latitude), lat_dir,
                 (double)fabsf(view->settings.longitude), lon_dir);
    } else {
        snprintf(text, sizeof(text), "位置  --");
    }
    lv_label_set_text(s_data_location, text);

    const char *state_text = status->has_config ? "按 OK 更新数据" : "请先设置 WiFi";
    uint32_t color = COL_DIM;
    switch (status->state) {
    case APP_WIFI_CONNECTING:
        state_text = "正在连接 WiFi...";
        color = COL_AMBER;
        break;
    case APP_WIFI_LOCATING:
        state_text = "正在获取网络位置...";
        color = COL_CYAN;
        break;
    case APP_WIFI_FETCHING:
        state_text = "正在获取传播数据...";
        color = COL_CYAN;
        break;
    case APP_WIFI_SUCCESS:
        state_text = "数据已更新";
        color = COL_GREEN;
        break;
    case APP_WIFI_FAILED:
        state_text = status->has_config ? "更新失败，请重试" : "请先设置 WiFi";
        color = COL_RED;
        break;
    case APP_WIFI_CONFIG_SAVED:
        state_text = "按 OK 更新数据";
        color = COL_GREEN;
        break;
    case APP_WIFI_PORTAL_STARTING:
    case APP_WIFI_PORTAL_READY:
        state_text = "请先完成 WiFi 设置";
        color = COL_AMBER;
        break;
    default:
        break;
    }
    lv_label_set_text(s_data_state, state_text);
    lv_obj_set_style_text_color(s_data_state, lv_color_hex(color), 0);

    const char *hint = "OK 更新数据  长按返回";
    if (!status->has_config) hint = "请先进入 WiFi 设置";
    else if (status->state == APP_WIFI_CONNECTING ||
             status->state == APP_WIFI_LOCATING ||
             status->state == APP_WIFI_FETCHING) hint = "正在更新，请稍候...";
    else if (status->state == APP_WIFI_SUCCESS) hint = "OK 再次更新  长按返回";
    else if (status->state == APP_WIFI_FAILED) hint = "OK 重试  长按返回";
    lv_label_set_text(s_data_hint, hint);
}

static void update_settings(const hf_view_t *view)
{
    char text[48];
    prop_utc_t utc;
    prop_utc_from_epoch(view->epoch, &utc);

    const int offset = view->settings.utc_offset_min;
    const char sign = offset < 0 ? '-' : '+';
    const int abs_offset = offset < 0 ? -offset : offset;

    lv_label_set_text(s_setting_value[APP_SETTING_STATION],
                      PROP_STATIONS[view->settings.station].name);
    lv_label_set_text(s_setting_value[APP_SETTING_TARGET],
                      PROP_TARGETS[view->settings.target].name);
    snprintf(text, sizeof(text), "UTC%c%d:%02d", sign, abs_offset / 60,
             abs_offset % 60);
    lv_label_set_text(s_setting_value[APP_SETTING_UTC_OFFSET], text);
    snprintf(text, sizeof(text), "%u", (unsigned)view->settings.sfi);
    lv_label_set_text(s_setting_value[APP_SETTING_SFI], text);
    snprintf(text, sizeof(text), "%u", (unsigned)view->settings.kp);
    lv_label_set_text(s_setting_value[APP_SETTING_KP], text);

    if (view->settings.has_location) {
        const char lat_dir = view->settings.latitude >= 0.0f ? 'N' : 'S';
        const char lon_dir = view->settings.longitude >= 0.0f ? 'E' : 'W';
        snprintf(text, sizeof(text), "%.1f%c %.1f%c",
                 (double)fabsf(view->settings.latitude), lat_dir,
                 (double)fabsf(view->settings.longitude), lon_dir);
    } else {
        snprintf(text, sizeof(text), "未定位");
    }
    lv_label_set_text(s_setting_location, text);
    snprintf(text, sizeof(text), "%02u:%02u", (unsigned)utc.hour,
             (unsigned)utc.minute);
    lv_label_set_text(s_setting_value[APP_SETTING_TIME], text);
    snprintf(text, sizeof(text), "%02u-%02u", (unsigned)utc.month,
             (unsigned)utc.day);
    lv_label_set_text(s_setting_value[APP_SETTING_DATE], text);

    for (int i = 0; i < APP_SETTING_COUNT; i++) {
        const bool selected = (i == view->settings_index);
        const bool editing = selected && view->editing;
        lv_obj_set_style_bg_color(s_setting_row[i],
            lv_color_hex(selected ? COL_SELECT : COL_CARD_ALT), 0);
        lv_obj_set_style_border_color(s_setting_row[i],
            lv_color_hex(editing ? COL_AMBER : (selected ? COL_CYAN : COL_CARD_ALT)),
            0);
        lv_obj_set_style_border_width(s_setting_row[i], selected ? 1 : 0, 0);
        lv_obj_set_style_text_color(s_setting_value[i],
            lv_color_hex(editing ? COL_AMBER : COL_TEXT), 0);
    }

    if (view->editing) {
        lv_label_set_text(s_settings_hint, "上/下调整  OK确认  长按取消");
    } else if (view->saved_notice) {
        lv_label_set_text(s_settings_hint, "已保存");
    } else if (view->settings.station == PROP_STATION_AUTO &&
               !view->settings.has_location) {
        lv_label_set_text(s_settings_hint, "自动定位：联网更新时获取");
    } else if (view->settings.station == PROP_STATION_AUTO) {
        lv_label_set_text(s_settings_hint, "自动定位：更新数据时刷新");
    } else {
        lv_label_set_text(s_settings_hint, "上/下选择  OK编辑  长按保存");
    }
}

void hf_ui_render(const hf_view_t *view)
{
    if (!view) return;

    if (view->page != s_page || !s_screen) {
        lv_obj_t *old = s_screen;
        s_page = view->page;
        s_screen = build_page(view->page, view);
        lv_screen_load(s_screen);
        if (old) lv_obj_delete(old);
        if (!s_mem_logged) {
            lv_mem_monitor_t monitor;
            lv_mem_monitor(&monitor);
            ESP_LOGI("hf_ui", "LVGL pool free=%u frag=%u%%",
                     (unsigned)monitor.free_size, (unsigned)monitor.frag_pct);
            s_mem_logged = true;
        }
    }

    set_battery(view);

    switch (view->page) {
    case HF_PAGE_OVERVIEW:
        update_overview(view);
        break;
    case HF_PAGE_DETAIL:
        update_detail(view);
        break;
    case HF_PAGE_MENU:
        update_menu(view);
        break;
    case HF_PAGE_SETTINGS:
        update_settings(view);
        break;
    case HF_PAGE_WIFI:
        update_wifi_page(view);
        break;
    case HF_PAGE_DATA:
        update_data_page(view);
        break;
    default:
        break;
    }
}

void hf_ui_scroll(int dy)
{
    if (!s_about_card || s_page != HF_PAGE_ABOUT) return;
    lv_obj_scroll_by(s_about_card, 0, dy, LV_ANIM_ON);
}

