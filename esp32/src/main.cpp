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

#define RAW_UART_TEST_MODE 0  /* 1 = einfacher ESP32->Pico Rohdaten-Test, 0 = Normalbetrieb */

int32_t db_value = 0;
static lv_obj_t * digit_labels[3];  /* 0=hundreds, 1=tens, 2=ones */
static lv_obj_t * digit_cursor;     /* underline indicator */
uint8_t digit_max[3] = { DIGIT_MAX_0, DIGIT_MAX_1, DIGIT_MAX_2 };
int selected_digit = (DIGIT_MAX_2 > 0) ? 2 : (DIGIT_MAX_1 > 0) ? 1 : 0;

/* — Laufzeit-Attenuatorkonfiguration (wird beim Start vom Pico empfangen) — */
static int         att_relay_count = 4;
static String      att_name_str    = "";
int32_t            att_step        = 10;
int32_t            att_max_val     = DIGIT_MAX_VAL;
static lv_obj_t * tabview;

/* Default values for the preset buttons (3 x 3) */
int32_t default_values[DEFAULT_BUTTON_COUNT] = {20, 40, 60, 10, 30, 50, 70, 80, 90};
static lv_obj_t * default_labels[DEFAULT_BUTTON_COUNT];

/* Keyboard edit state */
static lv_obj_t * kb = NULL;
static lv_obj_t * ta = NULL;
static int editing_index = -1;
static bool long_press_active = false;
int set_mode = 0;  /* 0=Set-Direct, 1=Set-Time, 2=Set-Button */
bool autoset = true;  /* abgeleitet: true wenn set_mode==0 */
static lv_obj_t * btn_set = NULL;
static lv_obj_t * ae_btnmatrix = NULL;
#if RAW_UART_TEST_MODE
static uint32_t raw_uart_test_tx_count = 0;
#endif



/* WiFi mode: 0=off, 2=AP+Client */
uint8_t wifi_mode_setting = 2; /* default: WLAN an */
static lv_obj_t * wifi_switch = NULL;
lv_obj_t * ip_label = NULL;
lv_obj_t * auto_set_label = NULL;  /* Label für AUTO-Set Status */
static lv_obj_t * title_label = NULL;  /* Titel-Label: zeigt ATTNAME */

static lv_obj_t * info_name_label  = NULL;
static lv_obj_t * info_relay_label = NULL;
static lv_obj_t * info_max_label   = NULL;
static lv_obj_t * info_step_label  = NULL;
static lv_obj_t * info_mode_label  = NULL;
static lv_obj_t * test_type_label  = NULL;
static int        att_relay_mode   = 0;   /* 0=BRIDGE, 1=STATIC */

/* Remote-Test-UI */
static lv_obj_t * test_controls    = NULL;
static lv_obj_t * test_relay_lbl   = NULL;
static lv_obj_t * test_state_lbl   = NULL;
static lv_obj_t * test_start_btn   = NULL;
static bool       test_active      = false;
static int        test_sel_idx     = 0;
static int        test_count       = 0;
static int        test_states_arr[16] = {};

/* Untermenü-Navigation */
static lv_obj_t * submenu_btn_cont       = NULL;
static lv_obj_t * submenu_pages[4]       = { NULL, NULL, NULL, NULL };
static lv_obj_t * submenu_back_btn       = NULL;

#if RAW_UART_TEST_MODE
static void send_raw_uart_test_value(void)
{
    raw_uart_test_tx_count++;
    Serial.write((uint8_t)db_value);
    Serial.flush();
}
#endif

static void send_attenuation_command(bool force_apply)
{
#if RAW_UART_TEST_MODE
    LV_UNUSED(force_apply);
    return;
#else
    int32_t att = (db_value / att_step) * att_step;
    if(att > att_max_val) att = att_max_val;
    if(force_apply) {
        Serial.printf("SET:%ddB\n", (int)att);
    }
    else {
        Serial.printf("%ddB\n", (int)att);
    }
#endif
}

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
        if(val > att_max_val) val = att_max_val;  /* Obergrenze */
        /* Gesperrte Stellen auf 0 runden (z.B. Einer gesperrt → 33 → 30) */
        if(digit_max[2] == 0) val = (val / 10) * 10;
        if(digit_max[1] == 0) val = (val / 100) * 100;
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
#if !RAW_UART_TEST_MODE
    Serial.printf("SEL%d\n", idx);
#endif
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

/* Send current attenuation value to Pico via Serial */
void apply_attenuation(void)
{
    send_attenuation_command(false);
}

void apply_attenuation_set(void)
{
    send_attenuation_command(true);
}

void apply_attenuation_timed(void)
{
    int32_t att_v = (db_value / att_step) * att_step;
    if(att_v > att_max_val) att_v = att_max_val;
    Serial.printf("TIMED:%ddB\n", (int)att_v);
}

static void set_config_value(int32_t val, bool apply_auto_command)
{
    db_value = val;
    update_digit_labels();
    lv_tabview_set_act(tabview, 0, LV_ANIM_OFF);
    if(apply_auto_command) apply_attenuation();
}

static void btn_up_cb(lv_event_t * e)
{
    if(digit_max[selected_digit] == 0) return;
    int multiplier = (selected_digit == 0) ? 100 : (selected_digit == 1) ? 10 : (int)att_step;
    db_value += multiplier;
    if(db_value > att_max_val) db_value = att_max_val;
    /* Auf gültigen Step runden */
    db_value = (db_value / att_step) * att_step;
    update_digit_labels();
    ws_broadcast_val();
#if RAW_UART_TEST_MODE
    send_raw_uart_test_value();
#else
    if(set_mode == 0)      apply_attenuation();        /* Set-Direct: sofort */
    else if(set_mode == 1) apply_attenuation_timed();  /* Set-Time:  verzögert */
    /* Set-Button (2): nichts senden – erst beim Set-Klick */
#endif
}

static void btn_down_cb(lv_event_t * e)
{
    if(digit_max[selected_digit] == 0) return;
    int multiplier = (selected_digit == 0) ? 100 : (selected_digit == 1) ? 10 : (int)att_step;
    db_value -= multiplier;
    if(db_value < 0) db_value = 0;
    /* Auf gültigen Step runden */
    db_value = (db_value / att_step) * att_step;
    update_digit_labels();
    ws_broadcast_val();
#if RAW_UART_TEST_MODE
    send_raw_uart_test_value();
#else
    if(set_mode == 0)      apply_attenuation();        /* Set-Direct: sofort */
    else if(set_mode == 1) apply_attenuation_timed();  /* Set-Time:  verzögert */
    /* Set-Button (2): nichts senden – erst beim Set-Klick */
#endif
}

static const char * set_mode_label_str()
{
    if(set_mode == 1) return "SET-TIME";
    if(set_mode == 2) return "SET-BUTTON";
    return "SET-DIRECT";
}

static void config_create(lv_obj_t * parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* Dark tab background */
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    /* Title */
    title_label = lv_label_create(parent);
    lv_label_set_text(title_label, att_name_str.length() > 0 ? att_name_str.c_str() : "Attenuator");
    static lv_style_t style_title;
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_24);
    lv_style_set_text_color(&style_title, lv_color_hex(0x60d0ff));
    lv_obj_add_style(title_label, &style_title, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, -5);

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
    lv_label_set_text(auto_set_label, set_mode_label_str());
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
    lv_obj_add_event_cb(btn_up, btn_up_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    lv_obj_t * lbl_up = lv_label_create(btn_up);
    lv_label_set_text(lbl_up, "UP");
    lv_obj_center(lbl_up);

    lv_obj_t * btn_down = lv_btn_create(btn_cont);
    lv_obj_set_width(btn_down, 88);
    lv_obj_add_style(btn_down, &style_btn, 0);
    lv_obj_add_event_cb(btn_down, btn_down_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_down, btn_down_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    lv_obj_t * lbl_down = lv_label_create(btn_down);
    lv_label_set_text(lbl_down, "Down");
    lv_obj_center(lbl_down);

    btn_set = lv_btn_create(btn_cont);
    lv_obj_set_width(btn_set, 88);
    lv_obj_add_style(btn_set, &style_btn, 0);
    lv_obj_add_event_cb(btn_set, [](lv_event_t * e) {
        LV_UNUSED(e);
        apply_attenuation_set();
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_set = lv_label_create(btn_set);
    lv_label_set_text(lbl_set, "Set");
    lv_obj_center(lbl_set);
    if(autoset) lv_obj_add_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
}

void update_config_value(int32_t val)
{
    set_config_value(val, autoset);
    /* Do NOT call ws_broadcast_val() here – callers handle it */
}

void apply_preset_value(int32_t val)
{
    set_config_value(val, autoset);
    if(!autoset) apply_attenuation_set();
}

void apply_web_preset_value(int32_t val)
{
    set_config_value(val, false);
    apply_attenuation_set();
}

static void btn_default_cb(lv_event_t * e)
{
    if(long_press_active) {
        long_press_active = false;
        return;
    }
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    apply_preset_value(default_values[idx]);
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

/* Forward-Deklarationen für Unterseiten */
static void wlan_create(lv_obj_t * parent);
static void info_create(lv_obj_t * parent);
static void test_create(lv_obj_t * parent);

static void show_submenu_page(int idx)
{
    if(submenu_btn_cont) lv_obj_add_flag(submenu_btn_cont, LV_OBJ_FLAG_HIDDEN);
    for(int i = 0; i < 4; i++)
        if(submenu_pages[i]) lv_obj_add_flag(submenu_pages[i], LV_OBJ_FLAG_HIDDEN);
    if(idx >= 0 && idx < 4 && submenu_pages[idx])
        lv_obj_clear_flag(submenu_pages[idx], LV_OBJ_FLAG_HIDDEN);
    if(submenu_back_btn) lv_obj_clear_flag(submenu_back_btn, LV_OBJ_FLAG_HIDDEN);
}

static void show_submenu_list(void)
{
    if(submenu_btn_cont) lv_obj_clear_flag(submenu_btn_cont, LV_OBJ_FLAG_HIDDEN);
    for(int i = 0; i < 4; i++)
        if(submenu_pages[i]) lv_obj_add_flag(submenu_pages[i], LV_OBJ_FLAG_HIDDEN);
    if(submenu_back_btn) lv_obj_add_flag(submenu_back_btn, LV_OBJ_FLAG_HIDDEN);
}

static void menu_create(lv_obj_t * parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);

    /* --- Untermenü-Schaltflächen --- */
    submenu_btn_cont = lv_obj_create(parent);
    lv_obj_set_size(submenu_btn_cont, 320, 200);
    lv_obj_set_pos(submenu_btn_cont, 0, 0);
    lv_obj_set_style_bg_opa(submenu_btn_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(submenu_btn_cont, 0, 0);
    lv_obj_set_style_pad_all(submenu_btn_cont, 0, 0);
    lv_obj_clear_flag(submenu_btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    const char * item_labels[4] = { "WLAN", "Info", "Test", "Verhalten" };
    for(int i = 0; i < 4; i++) {
        lv_obj_t * btn = lv_btn_create(submenu_btn_cont);
        lv_obj_set_size(btn, 290, 38);
        lv_obj_set_pos(btn, 15, 10 + i * 46);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a5090), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_t * lbl = lv_label_create(btn);
        lv_label_set_text(lbl, item_labels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_add_event_cb(btn, [](lv_event_t * e) {
            show_submenu_page((int)(intptr_t)lv_event_get_user_data(e));
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    /* --- Unterseiten (anfangs versteckt) --- */
    for(int i = 0; i < 4; i++) {
        lv_obj_t * page = lv_obj_create(parent);
        lv_obj_set_size(page, 320, 168);
        lv_obj_set_pos(page, 0, 0);
        lv_obj_set_style_bg_color(page, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(page, 0, 0);
        lv_obj_set_style_pad_all(page, 0, 0);
        lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
        submenu_pages[i] = page;
    }

    wlan_create(submenu_pages[0]);
    info_create(submenu_pages[1]);
    test_create(submenu_pages[2]);

    /* Verhalten: Set-Modus (1 aus 3) */
    {
        static const char * btn_map[] = { "Set-Direct", "Set-Time", "Set-Button", "" };
        lv_obj_t * page = submenu_pages[3];
        lv_obj_t * label = lv_label_create(page);
        lv_label_set_text(label, "Verhalten");
        lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, 10);
        lv_obj_t * btnm = lv_btnmatrix_create(page);
        ae_btnmatrix = btnm;
        lv_btnmatrix_set_map(btnm, btn_map);
        lv_obj_set_size(btnm, 290, 56);
        lv_obj_align_to(btnm, label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
        for(int i = 0; i < 3; i++)
            lv_btnmatrix_set_btn_ctrl(btnm, i, LV_BTNMATRIX_CTRL_CHECKABLE);
        lv_btnmatrix_set_one_checked(btnm, true);
        lv_btnmatrix_set_btn_ctrl(btnm, (uint16_t)set_mode, LV_BTNMATRIX_CTRL_CHECKED);
        lv_obj_add_event_cb(btnm, [](lv_event_t * e) {
            lv_obj_t * obj = lv_event_get_target(e);
            int idx = (int)lv_btnmatrix_get_selected_btn(obj);
            set_mode = idx;
            autoset  = (set_mode != 2);  /* true für Direct+Time, false nur für Button */
            prefs.putInt("setmode", set_mode);
            if(auto_set_label)
                lv_label_set_text(auto_set_label, set_mode_label_str());
            if(btn_set) {
                if(autoset) lv_obj_add_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
                else        lv_obj_clear_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
            }
#if !RAW_UART_TEST_MODE
            Serial.print("AUTO:");
            Serial.println(autoset ? "ON" : "OFF");
#endif
            ws_broadcast_ae();
        }, LV_EVENT_VALUE_CHANGED, NULL);
    }

    /* --- Zurück-Schaltfläche --- */
    submenu_back_btn = lv_btn_create(parent);
    lv_obj_set_size(submenu_back_btn, 90, 28);
    lv_obj_set_pos(submenu_back_btn, 10, 170);
    lv_obj_set_style_bg_color(submenu_back_btn, lv_color_hex(0x444444), 0);
    lv_obj_set_style_bg_opa(submenu_back_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(submenu_back_btn, 6, 0);
    lv_obj_t * back_lbl = lv_label_create(submenu_back_btn);
    lv_label_set_text(back_lbl, "< Zuruck");
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_lbl, lv_color_white(), 0);
    lv_obj_center(back_lbl);
    lv_obj_add_flag(submenu_back_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(submenu_back_btn, [](lv_event_t * e) {
        (void)e;
        show_submenu_list();
    }, LV_EVENT_CLICKED, NULL);
}

static void wlan_create(lv_obj_t * parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    lv_obj_t * wlabel = lv_label_create(parent);
    lv_label_set_text(wlabel, "WLAN");
    lv_obj_set_style_text_font(wlabel, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(wlabel, lv_color_white(), 0);
    lv_obj_align(wlabel, LV_ALIGN_TOP_LEFT, 10, 10);

    wifi_switch = lv_switch_create(parent);
    lv_obj_align_to(wifi_switch, wlabel, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_set_style_bg_color(wifi_switch, lv_color_hex(0x1a5090), 0);
    lv_obj_set_style_bg_opa(wifi_switch, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(wifi_switch, lv_color_hex(0x008000), LV_PART_INDICATOR | LV_STATE_CHECKED);
    if(wifi_mode_setting != 0) lv_obj_add_state(wifi_switch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(wifi_switch, [](lv_event_t * e) {
        wifi_mode_setting = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED) ? 2 : 0;
        prefs.putUChar("wmode", wifi_mode_setting);
        apply_wifi_mode();
    }, LV_EVENT_VALUE_CHANGED, NULL);

    /* AP / Client Statuslabel */
    ip_label = lv_label_create(parent);
    lv_label_set_text(ip_label, "");
    lv_obj_set_style_text_font(ip_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ip_label, lv_color_hex(0x60d0ff), 0);
    lv_label_set_long_mode(ip_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(ip_label, 300);
    lv_obj_align_to(ip_label, wlabel, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 15);
}

static void info_create(lv_obj_t * parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    static lv_style_t style_key, style_val;
    static bool styles_init = false;
    if(!styles_init) {
        styles_init = true;
        lv_style_init(&style_key);
        lv_style_set_text_font(&style_key, &lv_font_montserrat_14);
        lv_style_set_text_color(&style_key, lv_color_hex(0x888888));
        lv_style_init(&style_val);
        lv_style_set_text_font(&style_val, &lv_font_montserrat_14);
        lv_style_set_text_color(&style_val, lv_color_white());
    }

    const char*   keys[5] = { "Name:", "Relais:", "Max dB:", "Schritt:", "Modus:" };
    lv_obj_t**    vals[5] = { &info_name_label, &info_relay_label, &info_max_label,
                               &info_step_label, &info_mode_label };
    int           ys[5]   = { 10, 42, 74, 106, 138 };

    for(int i = 0; i < 5; i++) {
        lv_obj_t * k = lv_label_create(parent);
        lv_label_set_text(k, keys[i]);
        lv_obj_add_style(k, &style_key, 0);
        lv_obj_set_pos(k, 10, ys[i]);
        lv_obj_t * v = lv_label_create(parent);
        lv_label_set_text(v, "-");
        lv_obj_add_style(v, &style_val, 0);
        lv_obj_set_pos(v, 120, ys[i]);
        *vals[i] = v;
    }

    /* Mit bereits bekannten Werten befüllen */
    if(att_name_str.length() > 0)
        lv_label_set_text(info_name_label, att_name_str.c_str());
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", att_relay_count);
    lv_label_set_text(info_relay_label, buf);
    snprintf(buf, sizeof(buf), "%d", (int)att_max_val);
    lv_label_set_text(info_max_label, buf);
    snprintf(buf, sizeof(buf), "%d", (int)att_step);
    lv_label_set_text(info_step_label, buf);
    lv_label_set_text(info_mode_label, att_relay_mode == 1 ? "Static" : "Bridge");
}

static const char* test_relay_name(int idx)
{
    if(att_relay_mode == 1) {
        static const char* n[] = { "10dB", "20dB", "40A", "40B" };
        if(idx >= 0 && idx < 4) return n[idx];
        return "?";
    }
    static char b[12];
    snprintf(b, sizeof(b), "Bridge %d", idx + 1);
    return b;
}

static void test_ui_update()
{
    if(test_relay_lbl) lv_label_set_text(test_relay_lbl, test_relay_name(test_sel_idx));
    if(test_state_lbl) lv_label_set_text(test_state_lbl,
        (test_sel_idx < 16 && test_states_arr[test_sel_idx]) ? "EIN" : "AUS");
}

static void test_show_controls(bool active)
{
    test_active = active;
    if(test_start_btn) {
        if(active) lv_obj_add_flag(test_start_btn, LV_OBJ_FLAG_HIDDEN);
        else       lv_obj_clear_flag(test_start_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if(test_controls) {
        if(active) lv_obj_clear_flag(test_controls, LV_OBJ_FLAG_HIDDEN);
        else       lv_obj_add_flag(test_controls, LV_OBJ_FLAG_HIDDEN);
    }
}

static void test_create(lv_obj_t * parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    /* --- Test-Typ Label --- */
    test_type_label = lv_label_create(parent);
    lv_obj_set_style_text_font(test_type_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(test_type_label, lv_color_hex(0x60d0ff), 0);
    lv_label_set_text(test_type_label, att_relay_mode == 1 ? "Static-Test" : "Bridge-Test");
    lv_obj_align(test_type_label, LV_ALIGN_TOP_MID, 0, 5);

    /* --- Start-Button --- */
    test_start_btn = lv_btn_create(parent);
    lv_obj_set_size(test_start_btn, 200, 44);
    lv_obj_align(test_start_btn, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(test_start_btn, lv_color_hex(0x1a5090), 0);
    lv_obj_set_style_bg_opa(test_start_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(test_start_btn, 8, 0);
    lv_obj_t * slbl = lv_label_create(test_start_btn);
    lv_label_set_text(slbl, "Test starten");
    lv_obj_set_style_text_font(slbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(slbl, lv_color_white(), 0);
    lv_obj_center(slbl);
    lv_obj_add_event_cb(test_start_btn, [](lv_event_t * e) {
        (void)e;
        for(int i = 0; i < 16; i++) test_states_arr[i] = 0;
        test_sel_idx = 0;
        test_count   = (att_relay_mode == 1) ? 4 : 9;
        test_ui_update();
        test_show_controls(true);
#if !RAW_UART_TEST_MODE
        Serial.println("TEST:START");
#endif
    }, LV_EVENT_CLICKED, NULL);

    /* --- Steuerungscontainer (anfangs versteckt) --- */
    test_controls = lv_obj_create(parent);
    lv_obj_set_size(test_controls, 320, 133);
    lv_obj_set_pos(test_controls, 0, 35);
    lv_obj_set_style_bg_opa(test_controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(test_controls, 0, 0);
    lv_obj_set_style_pad_all(test_controls, 0, 0);
    lv_obj_clear_flag(test_controls, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(test_controls, LV_OBJ_FLAG_HIDDEN);

    /* Navigation: < relay_name > */
    lv_obj_t * prev_btn = lv_btn_create(test_controls);
    lv_obj_set_size(prev_btn, 40, 38);
    lv_obj_set_pos(prev_btn, 5, 6);
    lv_obj_set_style_bg_color(prev_btn, lv_color_hex(0x1a5090), 0);
    lv_obj_set_style_bg_opa(prev_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(prev_btn, 6, 0);
    lv_obj_t * plbl = lv_label_create(prev_btn);
    lv_label_set_text(plbl, "<");
    lv_obj_set_style_text_font(plbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(plbl, lv_color_white(), 0);
    lv_obj_center(plbl);
    lv_obj_add_event_cb(prev_btn, [](lv_event_t * e) {
        (void)e;
        if(test_count <= 0) return;
        test_sel_idx = (test_sel_idx + test_count - 1) % test_count;
        test_ui_update();
#if !RAW_UART_TEST_MODE
        char cmd[20]; snprintf(cmd, sizeof(cmd), "TEST:SEL:%d", test_sel_idx);
        Serial.println(cmd);
#endif
    }, LV_EVENT_CLICKED, NULL);

    test_relay_lbl = lv_label_create(test_controls);
    lv_obj_set_style_text_font(test_relay_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(test_relay_lbl, lv_color_white(), 0);
    lv_label_set_long_mode(test_relay_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(test_relay_lbl, 220);
    lv_obj_set_pos(test_relay_lbl, 52, 18);
    lv_obj_set_style_text_align(test_relay_lbl, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t * next_btn = lv_btn_create(test_controls);
    lv_obj_set_size(next_btn, 40, 38);
    lv_obj_set_pos(next_btn, 275, 6);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x1a5090), 0);
    lv_obj_set_style_bg_opa(next_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(next_btn, 6, 0);
    lv_obj_t * nlbl = lv_label_create(next_btn);
    lv_label_set_text(nlbl, ">");
    lv_obj_set_style_text_font(nlbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(nlbl, lv_color_white(), 0);
    lv_obj_center(nlbl);
    lv_obj_add_event_cb(next_btn, [](lv_event_t * e) {
        (void)e;
        if(test_count <= 0) return;
        test_sel_idx = (test_sel_idx + 1) % test_count;
        test_ui_update();
#if !RAW_UART_TEST_MODE
        char cmd[20]; snprintf(cmd, sizeof(cmd), "TEST:SEL:%d", test_sel_idx);
        Serial.println(cmd);
#endif
    }, LV_EVENT_CLICKED, NULL);

    /* Status-Zeile */
    lv_obj_t * skey = lv_label_create(test_controls);
    lv_obj_set_style_text_font(skey, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(skey, lv_color_hex(0x888888), 0);
    lv_label_set_text(skey, "Status:");
    lv_obj_set_pos(skey, 5, 54);

    test_state_lbl = lv_label_create(test_controls);
    lv_obj_set_style_text_font(test_state_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(test_state_lbl, lv_color_white(), 0);
    lv_label_set_text(test_state_lbl, "AUS");
    lv_obj_set_pos(test_state_lbl, 80, 54);

    /* Aktions-Button */
    lv_obj_t * act_btn = lv_btn_create(test_controls);
    lv_obj_set_size(act_btn, 148, 36);
    lv_obj_set_pos(act_btn, 5, 86);
    lv_obj_set_style_bg_color(act_btn, lv_color_hex(0x1a7020), 0);
    lv_obj_set_style_bg_opa(act_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(act_btn, 6, 0);
    lv_obj_t * albl = lv_label_create(act_btn);
    lv_label_set_text(albl, "Puls / Toggle");
    lv_obj_set_style_text_font(albl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(albl, lv_color_white(), 0);
    lv_obj_center(albl);
    lv_obj_add_event_cb(act_btn, [](lv_event_t * e) {
        (void)e;
#if !RAW_UART_TEST_MODE
        Serial.println("TEST:ACTION");
#endif
    }, LV_EVENT_CLICKED, NULL);

    /* Ende-Button */
    lv_obj_t * end_btn = lv_btn_create(test_controls);
    lv_obj_set_size(end_btn, 155, 36);
    lv_obj_set_pos(end_btn, 160, 86);
    lv_obj_set_style_bg_color(end_btn, lv_color_hex(0x702020), 0);
    lv_obj_set_style_bg_opa(end_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(end_btn, 6, 0);
    lv_obj_t * elbl = lv_label_create(end_btn);
    lv_label_set_text(elbl, "Test beenden");
    lv_obj_set_style_text_font(elbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(elbl, lv_color_white(), 0);
    lv_obj_center(elbl);
    lv_obj_add_event_cb(end_btn, [](lv_event_t * e) {
        (void)e;
        test_show_controls(false);
#if !RAW_UART_TEST_MODE
        Serial.println("TEST:END");
#endif
    }, LV_EVENT_CLICKED, NULL);
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
    /* Web-Interface kennt nur bool → autoset=true → Set-Direct, false → Set-Button */
    set_mode = autoset ? 0 : 2;
    prefs.putInt("setmode", set_mode);
    if(ae_btnmatrix)
        lv_btnmatrix_set_btn_ctrl(ae_btnmatrix, (uint16_t)set_mode, LV_BTNMATRIX_CTRL_CHECKED);
    if(auto_set_label)
        lv_label_set_text(auto_set_label, set_mode_label_str());
    if(btn_set) {
        if(autoset) lv_obj_add_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_clear_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
    }
    /* Sende Status an Pico über Serial */
#if !RAW_UART_TEST_MODE
    Serial.print("AUTO:");
    Serial.println(autoset ? "ON" : "OFF");
#endif
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
    lv_obj_t * t3 = lv_tabview_add_tab(tabview, "Menu");

    config_create(t1);
    defaults_create(t2);
    menu_create(t3);

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
    LV_UNUSED(buf);
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

    /* Load saved default values from NVS */
    prefs.begin("att", false);
    for(int i = 0; i < DEFAULT_BUTTON_COUNT; i++) {
        char key[8];
        snprintf(key, sizeof(key), "def%d", i);
        default_values[i] = prefs.getInt(key, default_values[i]);
    }
    set_mode = prefs.getInt("setmode", 0);
    autoset  = (set_mode != 2);
    wifi_mode_setting = prefs.getUChar("wmode", 2);
    if(wifi_mode_setting != 0) {
        wifi_mode_setting = 2;
        prefs.putUChar("wmode", wifi_mode_setting);
    }

    /* Beim Boot KEIN dB-Kommando an den Pico senden:
     * Der Pico haelt den persistierten Wert und sendet ihn beim Start
     * von sich aus per UART. Wuerden wir hier "0dB" senden, wuerde der
     * gespeicherte Wert auf 0 ueberschrieben. */

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
    indev_drv.long_press_repeat_time = 150;  /* ms zwischen Repeat-Events beim Halten */
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
                    /* Veraltet – Pico sendet nun SEL direkt */
                    int md = (digit_max[2] > 0) ? 3 : (digit_max[1] > 0) ? 2 : 1;
                    selected_digit = (selected_digit >= md - 1) ? 0 : (selected_digit + 1);
                    update_cursor();
                    ws_broadcast_seldigit(selected_digit);
                }
                /* Digit-Auswahl direkt vom Pico */
                else if(input.startsWith("SEL")) {
                    int idx = input.substring(3).toInt();
                    if(idx >= 0 && idx < 3) {
                        selected_digit = idx;
                        update_cursor();
                        ws_broadcast_seldigit(selected_digit);
                    }
                }
                /* Check für AUTO-Set Status vom Pico */
                else if(input.startsWith("AUTO:")) {
                    /* Format: "AUTO:ON" oder "AUTO:OFF" */
                    autoset  = input.endsWith("ON");
                    set_mode = autoset ? 0 : 2;
                    prefs.putInt("setmode", set_mode);

                    /* Update Label im Main-Tab */
                    if(auto_set_label)
                        lv_label_set_text(auto_set_label, set_mode_label_str());

                    /* Update Btnmatrix im Config-Tab */
                    if(ae_btnmatrix)
                        lv_btnmatrix_set_btn_ctrl(ae_btnmatrix, (uint16_t)set_mode, LV_BTNMATRIX_CTRL_CHECKED);

                    /* Update SET-Button Sichtbarkeit */
                    if(btn_set) {
                        if(autoset) lv_obj_add_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
                        else lv_obj_clear_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
                    }

                    ws_broadcast_ae();
                }
                /* Attenuator-Infos vom Pico beim Programmstart */
                else if(input.startsWith("RELAYS:")) {
                    att_relay_count = input.substring(7).toInt();
                    if(info_relay_label) {
                        char buf[8]; snprintf(buf, sizeof(buf), "%d", att_relay_count);
                        lv_label_set_text(info_relay_label, buf);
                    }
                }
                else if(input.startsWith("ATTNAME:")) {
                    att_name_str = input.substring(8);
                    if(title_label)     lv_label_set_text(title_label,     att_name_str.c_str());
                    if(info_name_label) lv_label_set_text(info_name_label, att_name_str.c_str());
                }
                else if(input.startsWith("DIGITS:")) {
                    int d = input.substring(7).toInt();
                    digit_max[2] = (d >= 3) ? 9 : 0;
                    digit_max[1] = (d >= 2) ? 9 : 0;
                    /* db_value auf gültige Stellen kürzen */
                    if(digit_max[2] == 0) db_value = (db_value / 10) * 10;
                    if(digit_max[1] == 0) db_value = (db_value / 100) * 100;
                    update_digit_labels();
                    /* Cursor auf gültige Stelle setzen falls nötig */
                    if(digit_max[selected_digit] == 0) {
                        selected_digit = (digit_max[1] > 0) ? 1 : 0;
                    }
                    update_cursor();
                }
                else if(input.startsWith("STEP:")) {
                    att_step = input.substring(5).toInt();
                    if(att_step < 1) att_step = 1;
                    if(info_step_label) {
                        char buf[8]; snprintf(buf, sizeof(buf), "%d", (int)att_step);
                        lv_label_set_text(info_step_label, buf);
                    }
                }
                else if(input.startsWith("MAXDB:")) {
                    int32_t m = input.substring(6).toInt();
                    if(m > 0) {
                        att_max_val = m;
                        /* Hunderter-Stelle aktivieren falls max >= 100 */
                        digit_max[0] = (m >= 100) ? (m / 100) : 0;
                        /* db_value auf neuen Maximalwert klämmern */
                        if(db_value > att_max_val) {
                            db_value = att_max_val;
                            update_digit_labels();
                        }
                        update_cursor();
                        if(info_max_label) {
                            char buf[8]; snprintf(buf, sizeof(buf), "%d", (int)att_max_val);
                            lv_label_set_text(info_max_label, buf);
                        }
                    }
                }
                else if(input.startsWith("RELMODE:")) {
                    att_relay_mode = input.substring(8).toInt();
                    const char* ms = att_relay_mode == 1 ? "Static" : "Bridge";
                    if(info_mode_label)  lv_label_set_text(info_mode_label, ms);
                    if(test_type_label)  lv_label_set_text(test_type_label,
                        att_relay_mode == 1 ? "Static-Test" : "Bridge-Test");
                }
                else if(input.startsWith("TESTSTATE:")) {
                    String data = input.substring(10);
                    int comma = data.indexOf(',');
                    if(comma > 0) {
                        int sel  = data.substring(0, comma).toInt();
                        int stat = data.substring(comma + 1).toInt();
                        if(sel >= 0 && sel < 16) test_states_arr[sel] = stat;
                        test_sel_idx = sel;
                        test_ui_update();
                    }
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
                    if(val >= 0 && val <= att_max_val) {
                        /* Runde auf erlaubte Schritte */
                        if(digit_max[2] == 0) val = (val / 10) * 10;  /* 10 dB Schritte */
                        if(digit_max[1] == 0) val = (val / 100) * 100;  /* 100 dB Schritte */
                        
                        /* Aktualisiere nur Display, NICHT Relais (Pico hat bereits gesetzt) */
                        db_value = val;
                        update_digit_labels();
                        
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
