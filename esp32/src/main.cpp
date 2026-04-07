/* ESP32-ATT-C-LVGL8
 *
 * 26.5 GHz Attenuator Controller with LVGL8 UI and WebGui 
 * on ESP32 Cheap Yellow Display (CYD)
 * 
 * Version 0.5 - 2026-03-17 OE5RNL & OE5NVL
 * 
 * Based on the LVGL8 example from ESP32-Cheap-Yellow-Display
 * https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
 * 
 * Hardware: ESP32-2432S028 (CYD) with ILI9341 display and XPT2046 touch
 * Display: 320x240 RGB565
 * Touch: XPT2046 on separate VSPI bus
 */

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>

Preferences prefs;

// ----------------------------
// UI: Tab Menu
// ----------------------------
LV_FONT_DECLARE(lv_font_digits_72);

/* Gesamtmaximum und Maximalwert je Ziffer (0 = Stelle deaktiviert) */
#define DIGIT_MAX_VAL  110   /* maximaler Gesamtwert in dB             */
#define DIGIT_MAX_0      1   /* Hunderter: 0 .. DIGIT_MAX_0           */
#define DIGIT_MAX_1      9   /* Zehner:    0 .. DIGIT_MAX_1           */
#define DIGIT_MAX_2      0   /* Einer:     0 = nicht wählbar          */

#include "webserver.h"

int32_t db_value = 0;
static lv_obj_t * digit_labels[3];  /* 0=hundreds, 1=tens, 2=ones */
static lv_obj_t * digit_cursor;     /* underline indicator */
static const uint8_t digit_max[3] = { DIGIT_MAX_0, DIGIT_MAX_1, DIGIT_MAX_2 };
int selected_digit = (DIGIT_MAX_2 > 0) ? 2 : (DIGIT_MAX_1 > 0) ? 1 : 0;
static lv_obj_t * tabview;

/* Default values for the preset buttons (3 x 3) */
int32_t default_values[DEFAULT_BUTTON_COUNT] = {20, 40, 60, 10, 30, 50, 70, 80, 90};
static lv_obj_t * default_labels[DEFAULT_BUTTON_COUNT];

/* Keyboard edit state */
static lv_obj_t * kb = NULL;
static lv_obj_t * ta = NULL;
static int editing_index = -1;
static bool long_press_active = false;
bool autoset = true;
static lv_obj_t * btn_set = NULL;
static lv_obj_t * ae_switch = NULL;

/* Attenuator GPIO mapping (active LOW)
 * Pads: 10 dB, 20 dB, 40 dB (A), 40 dB (B)  →  max 110 dB in 10 dB steps */
#define ATT_GPIO_10DB    4
#define ATT_GPIO_20DB   16
#define ATT_GPIO_40DB_A 17
#define ATT_GPIO_40DB_B 35
static int32_t last_att_db = -1;

/* WiFi mode: 0=off, 1=AP, 2=Client */
uint8_t wifi_mode_setting = 2; /* default: Client */
static lv_obj_t * wifi_radio_btns = NULL;
lv_obj_t * ip_label = NULL;
lv_obj_t * auto_set_label = NULL;  /* Label für AUTO-Set Status */

static void kb_close(void)
{
    if(kb) { lv_obj_del(kb); kb = NULL; }
    if(ta) { lv_obj_del(ta); ta = NULL; }
    editing_index = -1;
}

static void ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_READY) {
        const char * txt = lv_textarea_get_text(ta);
        int32_t val = atoi(txt);
        if(val < 0) val = 0;
        if(val > DIGIT_MAX_VAL) val = DIGIT_MAX_VAL;  /* Obergrenze */
        /* Gesperrte Stellen auf 0 runden (z.B. Einer gesperrt → 33 → 30) */
        if(DIGIT_MAX_2 == 0) val = (val / 10) * 10;
        if(DIGIT_MAX_1 == 0) val = (val / 100) * 100;
        if(editing_index >= 0 && editing_index < DEFAULT_BUTTON_COUNT) {
            default_values[editing_index] = val;
            lv_label_set_text_fmt(default_labels[editing_index], "%d dB", val);
            char key[8];
            snprintf(key, sizeof(key), "def%d", editing_index);
            prefs.putInt(key, val);
            ws_broadcast_def(editing_index);
        }
        kb_close();
        update_config_value(val);
        ws_broadcast_val();
    }
    else if(code == LV_EVENT_CANCEL) {
        kb_close();
    }
}

static void btn_long_press_cb(lv_event_t * e)
{
    long_press_active = true;
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    editing_index = idx;

    lv_obj_t * btn = lv_event_get_target(e);
    lv_area_t btn_area;
    lv_obj_get_coords(btn, &btn_area);

    ta = lv_textarea_create(lv_scr_act());
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 3);
    lv_obj_set_width(ta, lv_obj_get_width(btn));
    lv_obj_set_height(ta, lv_obj_get_height(btn));
    lv_obj_set_pos(ta, btn_area.x1, btn_area.y1);
    char val_buf[8];
    snprintf(val_buf, sizeof(val_buf), "%d", (int)default_values[idx]);
    lv_textarea_set_text(ta, val_buf);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_CANCEL, NULL);

    kb = lv_keyboard_create(lv_scr_act());
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(kb, ta);
    if(idx >= 6) {
        lv_obj_set_size(kb, lv_pct(100), 120);
        lv_obj_align(kb, LV_ALIGN_TOP_MID, 0, 0);
    }

    lv_obj_add_state(ta, LV_STATE_FOCUSED);
    lv_obj_clear_state(ta, LV_STATE_FOCUS_KEY);
}

/* Update the 3 digit labels from db_value */
static void update_digit_labels(void)
{
    int v = db_value;
    char d[2] = {0, 0};
    d[0] = '0' + (v / 100) % 10;
    lv_label_set_text(digit_labels[0], d);
    d[0] = '0' + (v / 10) % 10;
    lv_label_set_text(digit_labels[1], d);
    d[0] = '0' + v % 10;
    lv_label_set_text(digit_labels[2], d);
}

/* Move cursor under selected digit */
static void update_cursor(void)
{
    if(digit_cursor && digit_labels[selected_digit]) {
        lv_obj_align_to(digit_cursor, digit_labels[selected_digit],
                        LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    }
}

/* Digit click handler */
static void digit_click_cb(lv_event_t * e)
{
    if(long_press_active) {
        long_press_active = false;
        return;
    }
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if(digit_max[idx] == 0) return;  /* Stelle deaktiviert */
    selected_digit = idx;
    update_cursor();
    ws_broadcast_seldigit(idx);
    
    /* Sende Digit-Auswahl an Pico über Serial */
    Serial.printf("SEL%d\n", idx);
}

/* Digit long-press: open numeric keyboard for direct value entry */
static void digit_long_press_cb(lv_event_t * e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if(digit_max[idx] == 0) return;  /* Stelle deaktiviert */
    long_press_active = true;
    editing_index = -1;  /* not editing a default button */

    ta = lv_textarea_create(lv_scr_act());
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 3);
    lv_obj_set_width(ta, 120);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 30);
    lv_textarea_set_text(ta, "");
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_CANCEL, NULL);

    kb = lv_keyboard_create(lv_scr_act());
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(kb, ta);

    lv_obj_add_state(ta, LV_STATE_FOCUSED);
    lv_obj_clear_state(ta, LV_STATE_FOCUS_KEY);
}

/* Set attenuator GPIOs from db_value (10 dB steps, ones digit ignored) */
void apply_attenuation(void)
{
    int32_t att = (db_value / 10) * 10;
    if(att > 110) att = 110;
    if(att == last_att_db) return;   /* ones digit changed only → do nothing */
    last_att_db = att;

    int32_t rem = att;
    bool a40a = (rem >= 40); if(a40a) rem -= 40;
    bool a40b = (rem >= 40); if(a40b) rem -= 40;
    bool a20  = (rem >= 20); if(a20)  rem -= 20;
    bool a10  = (rem >= 10);

    digitalWrite(ATT_GPIO_10DB,   a10  ? LOW : HIGH);
    digitalWrite(ATT_GPIO_20DB,   a20  ? LOW : HIGH);
    digitalWrite(ATT_GPIO_40DB_A, a40a ? LOW : HIGH);
    digitalWrite(ATT_GPIO_40DB_B, a40b ? LOW : HIGH);

    // Serial.printf("ATT: %d dB  [10=%d 20=%d 40a=%d 40b=%d]\n",
    //               (int)att, !a10, !a20, !a40a, !a40b);
    
    /* Send to Pico via Serial */
    Serial.printf("%ddB\n", (int)att);
}

static void btn_up_cb(lv_event_t * e)
{
    if(digit_max[selected_digit] == 0) return;
    int multiplier = (selected_digit == 0) ? 100 : (selected_digit == 1) ? 10 : 1;
    db_value += multiplier;
    if(db_value > DIGIT_MAX_VAL) db_value = DIGIT_MAX_VAL;
    update_digit_labels();
    prefs.putInt("cval", db_value);
    ws_broadcast_val();
    if(autoset) apply_attenuation();
}

static void btn_down_cb(lv_event_t * e)
{
    if(digit_max[selected_digit] == 0) return;
    int multiplier = (selected_digit == 0) ? 100 : (selected_digit == 1) ? 10 : 1;
    db_value -= multiplier;
    if(db_value < 0) db_value = 0;
    update_digit_labels();
    prefs.putInt("cval", db_value);
    ws_broadcast_val();
    if(autoset) apply_attenuation();
}

static void config_create(lv_obj_t * parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* Dark tab background */
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    /* Title */
    lv_obj_t * title = lv_label_create(parent);
    lv_label_set_text(title, "26.5 GHz Attenuator");
    static lv_style_t style_title;
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_24);
    lv_style_set_text_color(&style_title, lv_color_hex(0x60d0ff));
    lv_obj_add_style(title, &style_title, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, -5);

    /* Large number display – 3 individual clickable digit labels */
    static lv_style_t style_big;
    lv_style_init(&style_big);
    lv_style_set_text_font(&style_big, &lv_font_digits_72);
    lv_style_set_text_color(&style_big, lv_color_white());

    /* Container for the 3 digits (no background, no border) */
    lv_obj_t * digit_cont = lv_obj_create(parent);
    lv_obj_set_size(digit_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(digit_cont, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_flex_flow(digit_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(digit_cont, 2, 0);
    lv_obj_set_style_pad_all(digit_cont, 0, 0);
    lv_obj_set_style_bg_opa(digit_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(digit_cont, LV_OPA_TRANSP, 0);

    int digits[3] = { (db_value / 100) % 10, (db_value / 10) % 10, db_value % 10 };
    for(int i = 0; i < 3; i++) {
        digit_labels[i] = lv_label_create(digit_cont);
        lv_obj_add_style(digit_labels[i], &style_big, 0);
        lv_obj_set_width(digit_labels[i], 42);
        lv_obj_set_style_text_align(digit_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        char d[2] = { (char)('0' + digits[i]), 0 };
        lv_label_set_text(digit_labels[i], d);
        if(digit_max[i] > 0) {
            lv_obj_add_flag(digit_labels[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(digit_labels[i], digit_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
            lv_obj_add_event_cb(digit_labels[i], digit_long_press_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
        }
    }

    /* Cursor line under selected digit */
    digit_cursor = lv_obj_create(parent);
    lv_obj_set_size(digit_cursor, 38, 3);
    lv_obj_set_style_bg_color(digit_cursor, lv_color_hex(0x60d0ff), 0);
    lv_obj_set_style_bg_opa(digit_cursor, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(digit_cursor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(digit_cursor, 0, 0);
    lv_obj_add_flag(digit_cursor, LV_OBJ_FLAG_FLOATING);
    update_cursor();

    /* Unit label "dB", smaller font */
    lv_obj_t * unit_label = lv_label_create(parent);
    lv_label_set_text(unit_label, "dB");
    static lv_style_t style_unit;
    lv_style_init(&style_unit);
    lv_style_set_text_font(&style_unit, &lv_font_montserrat_24);
    lv_style_set_text_color(&style_unit, lv_color_hex(0xcccccc));
    lv_obj_add_style(unit_label, &style_unit, 0);
    lv_obj_align_to(unit_label, digit_cont, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, 8);
    
    /* AUTO-Set Status Label (kleine Schrift unter den Digits) */
    auto_set_label = lv_label_create(parent);
    lv_label_set_text(auto_set_label, "AUTO: OFF");
    lv_obj_set_style_text_font(auto_set_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(auto_set_label, lv_color_hex(0xffa500), 0);
    lv_obj_align_to(auto_set_label, digit_cont, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);

    /* Container for buttons, right-aligned */
    lv_obj_t * btn_cont = lv_obj_create(parent);
    lv_obj_set_size(btn_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(btn_cont, LV_ALIGN_RIGHT_MID, 8, 15);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btn_cont, 10, 0);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(btn_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_font(btn_cont, &lv_font_montserrat_24, 0);

    /* Button style: #0f3460 bg, white text */
    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, lv_color_hex(0x1a5090));
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_text_color(&style_btn, lv_color_white());
    lv_style_set_radius(&style_btn, 6);

    lv_obj_t * btn_up = lv_btn_create(btn_cont);
    lv_obj_set_width(btn_up, 88);
    lv_obj_add_style(btn_up, &style_btn, 0);
    lv_obj_add_event_cb(btn_up, btn_up_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_up = lv_label_create(btn_up);
    lv_label_set_text(lbl_up, "UP");
    lv_obj_center(lbl_up);

    lv_obj_t * btn_down = lv_btn_create(btn_cont);
    lv_obj_set_width(btn_down, 88);
    lv_obj_add_style(btn_down, &style_btn, 0);
    lv_obj_add_event_cb(btn_down, btn_down_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_down = lv_label_create(btn_down);
    lv_label_set_text(lbl_down, "Down");
    lv_obj_center(lbl_down);

    btn_set = lv_btn_create(btn_cont);
    lv_obj_set_width(btn_set, 88);
    lv_obj_add_style(btn_set, &style_btn, 0);
    lv_obj_add_event_cb(btn_set, [](lv_event_t * e) {
        apply_attenuation();
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_set = lv_label_create(btn_set);
    lv_label_set_text(lbl_set, "Set");
    lv_obj_center(lbl_set);
    if(autoset) lv_obj_add_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
}

void update_config_value(int32_t val)
{
    db_value = val;
    update_digit_labels();
    prefs.putInt("cval", db_value);
    lv_tabview_set_act(tabview, 0, LV_ANIM_OFF);
    if(autoset) apply_attenuation();
    /* Do NOT call ws_broadcast_val() here – callers handle it */
}

static void btn_default_cb(lv_event_t * e)
{
    if(long_press_active) {
        long_press_active = false;
        return;
    }
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    update_config_value(default_values[idx]);
    ws_broadcast_val();
    ws_broadcast_active_def(idx);
}

static void defaults_create(lv_obj_t * parent)
{
    /* Dark background */
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(parent, 10, 0);
    lv_obj_set_style_pad_row(parent, 10, 0);
    lv_obj_set_style_pad_top(parent, 20, 0);
    lv_obj_set_style_pad_bottom(parent, 0, 0);
    lv_obj_set_style_pad_left(parent, 10, 0);
    lv_obj_set_style_pad_right(parent, 10, 0);
    lv_obj_set_style_text_font(parent, &lv_font_montserrat_24, 0);

    /* Default button style */
    static lv_style_t style_def;
    lv_style_init(&style_def);
    lv_style_set_bg_color(&style_def, lv_color_hex(0x1a5090));
    lv_style_set_bg_opa(&style_def, LV_OPA_COVER);
    lv_style_set_text_color(&style_def, lv_color_white());
    lv_style_set_radius(&style_def, 6);

    for(int i = 0; i < DEFAULT_BUTTON_COUNT; i++) {
        lv_obj_t * btn = lv_btn_create(parent);
        lv_obj_set_width(btn, 85);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_add_style(btn, &style_def, 0);
        lv_obj_add_event_cb(btn, btn_default_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, btn_long_press_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
        default_labels[i] = lv_label_create(btn);
        lv_label_set_text_fmt(default_labels[i], "%d dB", default_values[i]);
        lv_obj_center(default_labels[i]);
    }
}

static void help_create(lv_obj_t * parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* Dark background */
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    lv_obj_t * label = lv_label_create(parent);
    lv_label_set_text(label, "Auto-Set");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, 10);

    lv_obj_t * sw = lv_switch_create(parent);
    ae_switch = sw;
    lv_obj_align_to(sw, label, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x1a5090), 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    if(autoset) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, [](lv_event_t * e) {
        autoset = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        prefs.putBool("ae", autoset);
        
        /* Update Label im Main-Tab */
        if(auto_set_label) {
            lv_label_set_text(auto_set_label, autoset ? "AUTO: ON" : "AUTO: OFF");
        }
        
        /* Update SET-Button Sichtbarkeit */
        if(btn_set) {
            if(autoset) lv_obj_add_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_clear_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
        }
        
        /* Sende Status an Pico über Serial */
        Serial.print("AUTO:");
        Serial.println(autoset ? "ON" : "OFF");
        
        ws_broadcast_ae();
    }, LV_EVENT_VALUE_CHANGED, NULL);

    /* WiFi mode radio buttons */
    lv_obj_t * wlabel = lv_label_create(parent);
    lv_label_set_text(wlabel, "WLAN");
    lv_obj_set_style_text_font(wlabel, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(wlabel, lv_color_white(), 0);
    lv_obj_align(wlabel, LV_ALIGN_TOP_LEFT, 10, 55);

    static const char * wifi_opts[] = {"Aus", "AP", "Client", ""};
    wifi_radio_btns = lv_btnmatrix_create(parent);
    lv_btnmatrix_set_map(wifi_radio_btns, wifi_opts);
    lv_btnmatrix_set_btn_ctrl_all(wifi_radio_btns, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(wifi_radio_btns, true);
    lv_btnmatrix_set_btn_ctrl(wifi_radio_btns, wifi_mode_setting, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_set_size(wifi_radio_btns, 300, 67);
    lv_obj_align(wifi_radio_btns, LV_ALIGN_TOP_LEFT, 10, 85);
    lv_obj_set_style_text_font(wifi_radio_btns, &lv_font_montserrat_24, 0);
    lv_obj_set_style_bg_color(wifi_radio_btns, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(wifi_radio_btns, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(wifi_radio_btns, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(wifi_radio_btns, lv_color_hex(0x888888), 0);
    lv_obj_set_style_bg_color(wifi_radio_btns, lv_color_hex(0x1a5090), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(wifi_radio_btns, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_border_opa(wifi_radio_btns, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_text_color(wifi_radio_btns, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(wifi_radio_btns, lv_color_hex(0x008000), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_add_event_cb(wifi_radio_btns, [](lv_event_t * e) {
        uint32_t id = lv_btnmatrix_get_selected_btn(lv_event_get_target(e));
        if(id > 2) return;
        wifi_mode_setting = (uint8_t)id;
        prefs.putUChar("wmode", wifi_mode_setting);
        apply_wifi_mode();
    }, LV_EVENT_VALUE_CHANGED, NULL);

    /* IP address label */
    ip_label = lv_label_create(parent);
    lv_label_set_text(ip_label, "");
    lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ip_label, lv_color_hex(0x60d0ff), 0);
    lv_obj_align(ip_label, LV_ALIGN_TOP_LEFT, 10, 145);
}

/* Called from webserver.h when web client changes default_values */
void web_update_defaults(void)
{
    for(int i = 0; i < DEFAULT_BUTTON_COUNT; i++) {
        if(default_labels[i]) {
            lv_label_set_text_fmt(default_labels[i], "%d dB", default_values[i]);
        }
    }
}

/* Called from webserver.h when web client changes autoset */
void web_update_ae(void)
{
    if(ae_switch) {
        if(autoset) lv_obj_add_state(ae_switch, LV_STATE_CHECKED);
        else          lv_obj_clear_state(ae_switch, LV_STATE_CHECKED);
    }
    if(auto_set_label) {
        lv_label_set_text(auto_set_label, autoset ? "AUTO: ON" : "AUTO: OFF");
    }
    if(btn_set) {
        if(autoset) lv_obj_add_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_clear_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
    }
    /* Sende Status an Pico über Serial */
    Serial.print("AUTO:");
    Serial.println(autoset ? "ON" : "OFF");
}

/* Called from webserver.h when web client changes selected digit */
void web_update_seldigit(void)
{
    update_cursor();
}

static void create_ui(void)
{
    /* Dark background matching web: #1a1a2e */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

    tabview = lv_tabview_create(lv_scr_act(), LV_DIR_BOTTOM, 40);
    lv_obj_set_style_bg_color(tabview, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(tabview, LV_OPA_COVER, 0);

    /* Style tab buttons: dark navy bg, accent color when checked */
    lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_24, 0);
    lv_obj_set_style_bg_color(tab_btns, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(0x888888), 0);
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x1a5090), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(tab_btns, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_t * t1 = lv_tabview_add_tab(tabview, "Main");
    lv_obj_t * t2 = lv_tabview_add_tab(tabview, "Presets");
    lv_obj_t * t3 = lv_tabview_add_tab(tabview, "Config");

    config_create(t1);
    defaults_create(t2);
    help_create(t3);

    /* Disable horizontal swipe navigation between tabs */
    lv_obj_clear_flag(lv_tabview_get_content(tabview), LV_OBJ_FLAG_SCROLLABLE);
}

// ----------------------------
// Touch Screen pins
// ----------------------------
// The CYD touch uses some non-default SPI pins
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

uint16_t touchScreenMinimumX = 200, touchScreenMaximumX = 3700;
uint16_t touchScreenMinimumY = 240, touchScreenMaximumY = 3800;

// ----------------------------
// Display
// ----------------------------
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

#if LV_USE_LOG != 0
void my_print(const char *buf)
{
    Serial.printf(buf);
    Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

/* Read the touchpad */
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    if (ts.touched())
    {
        TS_Point p = ts.getPoint();
        // Basic auto calibration so it doesn't go out of range
        if (p.x < touchScreenMinimumX) touchScreenMinimumX = p.x;
        if (p.x > touchScreenMaximumX) touchScreenMaximumX = p.x;
        if (p.y < touchScreenMinimumY) touchScreenMinimumY = p.y;
        if (p.y > touchScreenMaximumY) touchScreenMaximumY = p.y;
        // Map to pixel position
        data->point.x = map(p.x, touchScreenMinimumX, touchScreenMaximumX, 1, screenWidth);
        data->point.y = map(p.y, touchScreenMinimumY, touchScreenMaximumY, 1, screenHeight);
        data->state = LV_INDEV_STATE_PR;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

void setup()
{
    /* Serial for Pico communication on GPIO3 (RX) / GPIO1 (TX) - Hardware UART0 */
    Serial.begin(115200);
    delay(200);
    
    // Serial2 nicht mehr verwenden - wir nutzen Serial direkt
    // Serial2.begin(115200, SERIAL_8N1, 3, 1);  // RX=3, TX=1
    // Serial2.setRxBufferSize(256);
    
    // Serial.println("Serial initialized on GPIO3 (RX) and GPIO1 (TX)");  // Kein Debug mehr über USB

    /* Attenuator GPIO init */
    pinMode(ATT_GPIO_10DB,   OUTPUT);
    pinMode(ATT_GPIO_20DB,   OUTPUT);
    pinMode(ATT_GPIO_40DB_A, OUTPUT);
    pinMode(ATT_GPIO_40DB_B, OUTPUT);
    digitalWrite(ATT_GPIO_10DB,   HIGH);
    digitalWrite(ATT_GPIO_20DB,   HIGH);
    digitalWrite(ATT_GPIO_40DB_A, HIGH);
    digitalWrite(ATT_GPIO_40DB_B, HIGH);

    /* Load saved default values from NVS */
    prefs.begin("att", false);
    for(int i = 0; i < DEFAULT_BUTTON_COUNT; i++) {
        char key[8];
        snprintf(key, sizeof(key), "def%d", i);
        default_values[i] = prefs.getInt(key, default_values[i]);
    }
    db_value = prefs.getInt("cval", db_value);
    autoset = prefs.getBool("ae", true);
    wifi_mode_setting = prefs.getUChar("wmode", 2);

    /* Restore last attenuation setting */
    apply_attenuation();

    // String info = "LVGL version ";
    // info += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
    // Serial.println(info);

    lv_init();

#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
#endif

    // Start second SPI bus for touchscreen
    mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(mySpi);
    ts.setRotation(3); // Landscape, 180° gedreht

    tft.begin();
    tft.setRotation(3); // Landscape, 180° gedreht

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    // Initialize the display
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Initialize the input device driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // Create custom UI
    create_ui();

    // Render initial display before WiFi blocks
    lv_timer_handler();
    delay(50);
    lv_timer_handler();

    // Start Webserver
    webserver_setup();

    // Serial.println("Setup done");
}

void loop()
{
    lv_timer_handler();
    webserver_loop();
    
    /* Empfange Werte vom Pico über Serial (UART0) */
    static String serial_buffer = "";
    while(Serial.available()) {
        char c = Serial.read();
        if(c == '\n' || c == '\r') {
            if(serial_buffer.length() > 0) {
                String input = serial_buffer;
                serial_buffer = "";  /* Clear buffer */
                input.trim();
                
                /* Check für DIGIT-Befehl (Toggle selected_digit) */
                if(input.equalsIgnoreCase("DIGIT")) {
                    /* Toggle zwischen Hunderter (0) und Zehner (1) Stelle */
                    selected_digit = (selected_digit == 0) ? 1 : 0;
                    update_cursor();
                    ws_broadcast_seldigit(selected_digit);
                    Serial.println("DIGIT command received - toggled digit selection");
                    
                    /* Sende neue Auswahl zurück an Pico */
                    Serial.printf("SEL%d\n", selected_digit);
                }
                /* Check für AUTO-Set Status vom Pico */
                else if(input.startsWith("AUTO:")) {
                    /* Format: "AUTO:ON" oder "AUTO:OFF" */
                    bool new_autoset = input.endsWith("ON");
                    autoset = new_autoset;
                    prefs.putBool("ae", autoset);
                    
                    /* Update Label im Main-Tab */
                    if(auto_set_label) {
                        lv_label_set_text(auto_set_label, input.c_str());
                    }
                    
                    /* Update Switch im Config-Tab */
                    if(ae_switch) {
                        if(autoset) lv_obj_add_state(ae_switch, LV_STATE_CHECKED);
                        else lv_obj_clear_state(ae_switch, LV_STATE_CHECKED);
                    }
                    
                    /* Update SET-Button Sichtbarkeit */
                    if(btn_set) {
                        if(autoset) lv_obj_add_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
                        else lv_obj_clear_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
                    }
                    
                    ws_broadcast_ae();
                    Serial.println("AUTO-Set status received: " + input);
                }
                else {
                    /* Parse number: accept plain number or "xxdB" format */
                    input.toLowerCase();
                    int dbPos = input.indexOf("db");
                    if(dbPos > 0) {
                        input = input.substring(0, dbPos);  /* Remove "db" suffix */
                    }
                    input.trim();

                    bool numeric = input.length() > 0;
                    for(uint32_t i = 0; i < input.length(); i++) {
                        if(!isDigit(input[i])) {
                            numeric = false;
                            break;
                        }
                    }
                    if(!numeric) {
                        continue;  /* Ignore non-numeric serial frames */
                    }

                    int val = input.toInt();
                
                    /* Validiere und aktualisiere Wert */
                    if(val >= 0 && val <= DIGIT_MAX_VAL) {
                        /* Runde auf erlaubte Schritte */
                        if(DIGIT_MAX_2 == 0) val = (val / 10) * 10;  /* 10 dB Schritte */
                        if(DIGIT_MAX_1 == 0) val = (val / 100) * 100;  /* 100 dB Schritte */
                        
                        /* Aktualisiere nur Display, NICHT Relais (Pico hat bereits gesetzt) */
                        db_value = val;
                        update_digit_labels();
                        last_att_db = (val / 10) * 10;  /* Synchronisiere last_att_db */
                        prefs.putInt("cval", db_value);
                        
                        /* Display sofort rendern */
                        lv_timer_handler();
                        
                        ws_broadcast_val();  /* Informiere WebGUI */
                    }
                }
            }
        }
        else if(c >= 32 && c < 127) {  /* Printable ASCII */
            serial_buffer += c;
        }
    }
    
    delay(5);
}
