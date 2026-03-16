/* ESP32-ATT-C-LVGL8
 *
 * LVGL8 Widget Demo for ESP32 Cheap Yellow Display (CYD)
 * Based on the LVGL8 example from ESP32-Cheap-Yellow-Display
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
#include "webserver.h"

Preferences prefs;

// ----------------------------
// UI: Tab Menu
// ----------------------------
LV_FONT_DECLARE(lv_font_digits_72);

int32_t config_value = 0;
static lv_obj_t * digit_labels[3];  /* 0=hundreds, 1=tens, 2=ones */
static lv_obj_t * digit_cursor;     /* underline indicator */
static int selected_digit = 2;      /* default: ones */
static lv_obj_t * tabview;

/* Default values for the 6 buttons */
int32_t default_values[6] = {20, 40, 60, 10, 30, 50};
static lv_obj_t * default_labels[6];

/* Keyboard edit state */
static lv_obj_t * kb = NULL;
static lv_obj_t * ta = NULL;
static int editing_index = -1;
static bool long_press_active = false;
bool autoenter = false;
static lv_obj_t * btn_set = NULL;
static lv_obj_t * ae_switch = NULL;

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
        if(val > 999) val = 999;
        if(editing_index >= 0 && editing_index < 6) {
            default_values[editing_index] = val;
            lv_label_set_text_fmt(default_labels[editing_index], "%d dB", val);
            char key[8];
            snprintf(key, sizeof(key), "def%d", editing_index);
            prefs.putInt(key, val);
            ws_broadcast_def(editing_index);
        }
        kb_close();
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
    lv_obj_t * parent = lv_obj_get_parent(btn);

    ta = lv_textarea_create(lv_scr_act());
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 3);
    lv_obj_set_width(ta, lv_obj_get_width(btn));
    lv_obj_set_height(ta, lv_obj_get_height(btn));

    /* Get absolute screen coordinates of the pressed button */
    lv_area_t btn_area;
    lv_obj_get_coords(btn, &btn_area);
    lv_obj_set_pos(ta, btn_area.x1, btn_area.y1);
    lv_textarea_set_text(ta, "");
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_CANCEL, NULL);

    kb = lv_keyboard_create(lv_scr_act());
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(kb, ta);
}

/* Update the 3 digit labels from config_value */
static void update_digit_labels(void)
{
    int v = config_value;
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
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    selected_digit = idx;
    update_cursor();
}

static void btn_up_cb(lv_event_t * e)
{
    int multiplier = (selected_digit == 0) ? 100 : (selected_digit == 1) ? 10 : 1;
    config_value += multiplier;
    if(config_value > 999) config_value -= 1000;
    update_digit_labels();
    prefs.putInt("cval", config_value);
    ws_broadcast_val();
}

static void btn_down_cb(lv_event_t * e)
{
    int multiplier = (selected_digit == 0) ? 100 : (selected_digit == 1) ? 10 : 1;
    config_value -= multiplier;
    if(config_value < 0) config_value = 0;
    update_digit_labels();
    prefs.putInt("cval", config_value);
    ws_broadcast_val();
}

static void config_create(lv_obj_t * parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t * title = lv_label_create(parent);
    lv_label_set_text(title, "26.5 GHz Attenuator");
    static lv_style_t style_title;
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_24);
    lv_obj_add_style(title, &style_title, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, -5);

    /* Large number display – 3 individual clickable digit labels */
    static lv_style_t style_big;
    lv_style_init(&style_big);
    lv_style_set_text_font(&style_big, &lv_font_digits_72);
    lv_style_set_text_color(&style_big, lv_color_black());

    /* Container for the 3 digits (no background, no border) */
    lv_obj_t * digit_cont = lv_obj_create(parent);
    lv_obj_set_size(digit_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(digit_cont, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_flex_flow(digit_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(digit_cont, 2, 0);
    lv_obj_set_style_pad_all(digit_cont, 0, 0);
    lv_obj_set_style_bg_opa(digit_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(digit_cont, LV_OPA_TRANSP, 0);

    int digits[3] = { (config_value / 100) % 10, (config_value / 10) % 10, config_value % 10 };
    for(int i = 0; i < 3; i++) {
        digit_labels[i] = lv_label_create(digit_cont);
        lv_obj_add_style(digit_labels[i], &style_big, 0);
        lv_obj_set_width(digit_labels[i], 42);
        lv_obj_set_style_text_align(digit_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        char d[2] = { (char)('0' + digits[i]), 0 };
        lv_label_set_text(digit_labels[i], d);
        lv_obj_add_flag(digit_labels[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(digit_labels[i], digit_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    /* Cursor line under selected digit */
    digit_cursor = lv_obj_create(parent);
    lv_obj_set_size(digit_cursor, 38, 3);
    lv_obj_set_style_bg_color(digit_cursor, lv_color_black(), 0);
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
    lv_obj_add_style(unit_label, &style_unit, 0);
    lv_obj_align_to(unit_label, digit_cont, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, 8);

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

    lv_obj_t * btn_up = lv_btn_create(btn_cont);
    lv_obj_set_width(btn_up, 88);
    lv_obj_add_event_cb(btn_up, btn_up_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_up = lv_label_create(btn_up);
    lv_label_set_text(lbl_up, "UP");
    lv_obj_center(lbl_up);

    lv_obj_t * btn_down = lv_btn_create(btn_cont);
    lv_obj_set_width(btn_down, 88);
    lv_obj_add_event_cb(btn_down, btn_down_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_down = lv_label_create(btn_down);
    lv_label_set_text(lbl_down, "Down");
    lv_obj_center(lbl_down);

    btn_set = lv_btn_create(btn_cont);
    lv_obj_set_width(btn_set, 88);
    lv_obj_add_event_cb(btn_set, [](lv_event_t * e) {
        /* TODO: send config_value to attenuator */
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_set = lv_label_create(btn_set);
    lv_label_set_text(lbl_set, "Set");
    lv_obj_center(lbl_set);
    if(autoenter) lv_obj_add_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
}

void update_config_value(int32_t val)
{
    config_value = val;
    update_digit_labels();
    prefs.putInt("cval", config_value);
    lv_tabview_set_act(tabview, 0, LV_ANIM_OFF);
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
}

static void defaults_create(lv_obj_t * parent)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(parent, 10, 0);
    lv_obj_set_style_pad_row(parent, 10, 0);
    lv_obj_set_style_pad_top(parent, 20, 0);
    lv_obj_set_style_pad_bottom(parent, 0, 0);
    lv_obj_set_style_pad_left(parent, 10, 0);
    lv_obj_set_style_pad_right(parent, 10, 0);
    lv_obj_set_style_text_font(parent, &lv_font_montserrat_24, 0);

    for(int i = 0; i < 6; i++) {
        lv_obj_t * btn = lv_btn_create(parent);
        lv_obj_set_width(btn, 85);
        lv_obj_add_event_cb(btn, btn_default_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(btn, btn_long_press_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
        default_labels[i] = lv_label_create(btn);
        lv_label_set_text_fmt(default_labels[i], "%d dB", default_values[i]);
        lv_obj_center(default_labels[i]);
    }
}

static void help_create(lv_obj_t * parent)
{
    lv_obj_t * label = lv_label_create(parent);
    lv_label_set_text(label, "Set Autovalue");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, 10);

    lv_obj_t * sw = lv_switch_create(parent);
    ae_switch = sw;
    lv_obj_align_to(sw, label, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    lv_obj_set_style_bg_color(sw, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    if(autoenter) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, [](lv_event_t * e) {
        autoenter = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        prefs.putBool("ae", autoenter);
        if(btn_set) {
            if(autoenter) lv_obj_add_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_clear_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
        }
        ws_broadcast_ae();
    }, LV_EVENT_VALUE_CHANGED, NULL);
}

/* Called from webserver.h when web client changes default_values */
void web_update_defaults(void)
{
    for(int i = 0; i < 6; i++) {
        if(default_labels[i]) {
            lv_label_set_text_fmt(default_labels[i], "%d dB", default_values[i]);
        }
    }
}

/* Called from webserver.h when web client changes autoenter */
void web_update_ae(void)
{
    if(ae_switch) {
        if(autoenter) lv_obj_add_state(ae_switch, LV_STATE_CHECKED);
        else          lv_obj_clear_state(ae_switch, LV_STATE_CHECKED);
    }
    if(btn_set) {
        if(autoenter) lv_obj_add_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_clear_flag(btn_set, LV_OBJ_FLAG_HIDDEN);
    }
}

static void create_ui(void)
{
    tabview = lv_tabview_create(lv_scr_act(), LV_DIR_BOTTOM, 40);

    /* Style tab buttons: selected = white bg, unselected = blue bg */
    lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_24, 0);
    lv_obj_set_style_bg_color(tab_btns, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(tab_btns, lv_color_white(), 0);
    lv_obj_set_style_bg_color(tab_btns, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(tab_btns, lv_color_black(), LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_t * t1 = lv_tabview_add_tab(tabview, "Main");
    lv_obj_t * t2 = lv_tabview_add_tab(tabview, "Default");
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
    Serial.begin(115200);

    /* Load saved default values from NVS */
    prefs.begin("att", false);
    for(int i = 0; i < 6; i++) {
        char key[8];
        snprintf(key, sizeof(key), "def%d", i);
        default_values[i] = prefs.getInt(key, default_values[i]);
    }
    config_value = prefs.getInt("cval", config_value);
    autoenter = prefs.getBool("ae", false);

    String info = "LVGL version ";
    info += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
    Serial.println(info);

    lv_init();

#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print);
#endif

    // Start second SPI bus for touchscreen
    mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(mySpi);
    ts.setRotation(1); // Landscape orientation

    tft.begin();
    tft.setRotation(1); // Landscape orientation

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

    // Start Webserver
    webserver_setup();

    Serial.println("Setup done");
}

void loop()
{
    lv_timer_handler();
    webserver_loop();
    delay(5);
}
