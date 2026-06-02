/* Raspberry Pi Pico - Attenuator Controller
 *
 * Version 0.4 - 2026-04-13
 *
 * Hardware: Raspberry Pi Pico
 *
 * serial.println  gibt über die USB zum PC (debug) ausgegeben
 * Serial1.println spricht über GPIO1 (TX) (RX) mit dem ESP32
 *  für die Ermitteltes tAttenuator, Synchronisation von db-Wert, AUTO-Set und Digit-Auswahl...
 *
 * Modular structure:
 *   Attenuator.h/cpp   – abstract base class + factory (create_attenuator)
 *   att_26ghz.*        – R&S 26.5 GHz (H-bridge, 10 dB steps)
 *   att_135db.*        – R&S 135 dB   (bistable relays, 5 dB steps)
 *   att_a.*            – Stub for unknown type A
 *   att_b.*            – Stub for unknown type B
 *   att_types.h        – type constants, ADC_SELECT_PIN, getAttenuator()
 */

#include <Arduino.h>
#include <Wire.h>
#include <cstdio>
#include <BlockDevice.h>
#include <LittleFileSystem.h>
#include "SSD1306.h"
#include "att_types.h"
#include "Attenuator.h"
#include "test.h"

#define RAW_UART_TEST_MODE 0  /* 1 = einfacher ESP32->Pico Rohdaten-Test, 0 = Normalbetrieb */

/* I2C Display SSD1306 (128x64) using I2C0 on GPIO4 PIN6 (SDA) / GPIO5 PIN7 (SCL) */
SSD1306 display(&Wire);

/* KY-040 Rotary Encoder */
#define ENCODER_SW     19   // Pin 25 Switch/Button
#define ENCODER_DT     20   // Pin 26 Data (Direction)
#define ENCODER_CLK    21   // Pin 27 Clock (Pulse)


/* -------------------------------------------------------
 * Active attenuator instance (set once in setup())
 * ------------------------------------------------------- */
static Attenuator* att = nullptr;


/* -------------------------------------------------------
 * Global State
 * ------------------------------------------------------- */

static int32_t current_db = 0;
static unsigned long last_led_toggle = 0;
static bool led_state = false;
static bool led_solid_mode = false;
static unsigned long led_solid_start = 0;
static String serial_buffer = "";

/* Rotary Encoder State */
static int encoder_last_clk = HIGH;
static int encoder_last_sw  = HIGH;
static int encoder_position = 0;
static unsigned long last_encoder_change = 0;
static bool encoder_apply_pending = false;
static unsigned long button_press_start = 0;
static bool button_was_pressed = false;
static bool long_press_executed = false;
static bool suppress_relay_update = false;
#if RAW_UART_TEST_MODE
static uint32_t raw_uart_rx_count = 0;
static int raw_uart_last_value = -1;
#endif

static const unsigned long ENCODER_SETTLE_TIME = 300;
static const unsigned long LONG_PRESS_TIME      = 800;
static const unsigned long TIMED_SETTLE_TIME    = 300;
static bool          timed_apply_pending = false;
static unsigned long timed_last_rx       = 0;

/* Doppelklick-Erkennung */
static unsigned long last_click_time = 0;
static bool waiting_for_double_click = false;
static const unsigned long DOUBLE_CLICK_TIME = 1000;

/* Display Cursor State */
static int selected_digit = 1;  /* 0 = Hunderter, 1 = Zehner */

/* AUTO-Set Mode */
static bool auto_set_mode = true;

/* Test Mode */
static bool test_mode = false;

/* -------------------------------------------------------
 * Remote-Test-Modus (gesteuert vom ESP32 per Serial1)
 * ------------------------------------------------------- */
static bool  rmt_test_active    = false;
static bool  rmt_test_is_bridge = false;
static int   rmt_test_sel       = 0;
static int   rmt_test_count     = 0;
/* Bridge-Test-Pins (identisch mit test.cpp) */
static const int RMT_HB_COUNT       = 9;
static const int rmt_hb_pin_a[9]    = {  2,  6,  8, 10, 12, 14, 16, 18, 27 };
static const int rmt_hb_pin_b[9]    = {  3,  7,  9, 11, 13, 15, 17, 22, 28 };
static int   rmt_hb_states[9]       = {};
/* Static-Test-Pins */
static const int RMT_SG_COUNT       = 4;
static const int rmt_sg_pins[4]     = { 10, 11, 12, 13 };
static bool  rmt_sg_states[4]       = {};

static const int32_t STARTUP_DB = 0;
static int32_t persisted_db_cache = -1000;
static mbed::BlockDevice * storage_bd = nullptr;
static mbed::LittleFileSystem storage_fs("fs");
static bool storage_ready = false;
static bool has_persisted_db = false;
static const char * DB_STORE_FILE = "/fs/att_db.txt";

#if RAW_UART_TEST_MODE
static void update_raw_uart_test_display()
{
    char line[32];
    display.clear();
    display.drawString(0, 0, "UART TEST ESP->PICO");
    snprintf(line, sizeof(line), "RX COUNT: %lu", (unsigned long)raw_uart_rx_count);
    display.drawString(0, 18, line);
    if(raw_uart_last_value >= 0) {
        snprintf(line, sizeof(line), "LAST RAW: %d", raw_uart_last_value);
        display.drawString(0, 34, line);
    } else {
        display.drawString(0, 34, "LAST RAW: ---");
    }
    display.drawString(0, 50, "UP/DOWN am ESP druecken");
    display.display();
}
#endif

/* -------------------------------------------------------
 * Persistent Storage
 * ------------------------------------------------------- */

static void init_persistent_storage()
{
    storage_bd = mbed::BlockDevice::get_default_instance();
    if(!storage_bd) return;

    int err = storage_fs.mount(storage_bd);
    if(err) {
        err = storage_fs.reformat(storage_bd);
        if(err) return;
        err = storage_fs.mount(storage_bd);
        if(err) return;
    }
    storage_ready = true;
}

static void load_persisted_db()
{
    current_db = STARTUP_DB;
    has_persisted_db = false;
    if(storage_ready) {
        FILE * f = fopen(DB_STORE_FILE, "r");
        if(f) {
            int stored = STARTUP_DB;
            if(fscanf(f, "%d", &stored) == 1 && stored >= 0 && stored <= 999) {
                current_db = stored;
                has_persisted_db = true;
            }
            fclose(f);
        }
    }
    persisted_db_cache = current_db;
}

static void save_persisted_db_if_changed(int32_t db_value)
{
    if(db_value < 0) db_value = 0;
    if(db_value == persisted_db_cache) return;
    if(!storage_ready) return;

    FILE * f = fopen(DB_STORE_FILE, "w");
    if(!f) return;
    fprintf(f, "%d\n", (int)db_value);
    fclose(f);
    has_persisted_db = true;
    persisted_db_cache = db_value;
}

static void sync_state_to_esp32()
{
    if(has_persisted_db) {
        Serial1.print(current_db);
        Serial1.println("dB");
    }
    Serial1.print("AUTO:");
    Serial1.println(auto_set_mode ? "ON" : "OFF");
    Serial1.print("SEL");
    Serial1.println(selected_digit);
}

static void send_info_to_esp32()
{
    Serial1.print("RELAYS:");
    Serial1.println(att ? att->relay_count() : 0);
    Serial1.print("ATTNAME:");
    Serial1.println(att ? att->att_name() : "UNKNOWN");
    Serial1.print("DIGITS:");
    Serial1.println(att ? att->digit_count() : 2);
    Serial1.print("STEP:");
    Serial1.println(att ? (int)att->step_db() : 10);
    Serial1.print("MAXDB:");
    Serial1.println(att ? (int)att->max_db() : 110);
    Serial1.print("RELMODE:");
    Serial1.println(att ? att->relay_mode() : BRIDGE);
}

/* -------------------------------------------------------
 * Remote-Test-Funktionen (ESP32-gesteuert, non-blocking)
 * ------------------------------------------------------- */
static void rmt_test_send_state()
{
    int s = rmt_test_is_bridge ? rmt_hb_states[rmt_test_sel]
                               : (int)rmt_sg_states[rmt_test_sel];
    Serial1.print("TESTSTATE:");
    Serial1.print(rmt_test_sel);
    Serial1.print(",");
    Serial1.println(s);
}

static void rmt_test_start()
{
    rmt_test_is_bridge = !att || (att->relay_mode() == BRIDGE);
    rmt_test_sel       = 0;
    if(rmt_test_is_bridge) {
        rmt_test_count = RMT_HB_COUNT;
        for(int i = 0; i < RMT_HB_COUNT; i++) rmt_hb_states[i] = 0;
        for(int i = 0; i < RMT_HB_COUNT; i++) {
            pinMode(rmt_hb_pin_a[i], OUTPUT); digitalWrite(rmt_hb_pin_a[i], LOW);
            pinMode(rmt_hb_pin_b[i], OUTPUT); digitalWrite(rmt_hb_pin_b[i], LOW);
        }
    } else {
        rmt_test_count = RMT_SG_COUNT;
        for(int i = 0; i < RMT_SG_COUNT; i++) rmt_sg_states[i] = false;
        for(int i = 0; i < RMT_SG_COUNT; i++) {
            pinMode(rmt_sg_pins[i], OUTPUT);
            digitalWrite(rmt_sg_pins[i], LOW);
        }
    }
    rmt_test_active = true;
    rmt_test_send_state();
}

static void rmt_test_select(int idx)
{
    if(idx < 0 || idx >= rmt_test_count) return;
    rmt_test_sel = idx;
    rmt_test_send_state();
}

static void rmt_test_action()
{
    int i = rmt_test_sel;
    if(rmt_test_is_bridge) {
        if(rmt_hb_states[i] == 0) {
            digitalWrite(rmt_hb_pin_a[i], HIGH); delay(20); digitalWrite(rmt_hb_pin_a[i], LOW);
        } else {
            digitalWrite(rmt_hb_pin_b[i], HIGH); delay(20); digitalWrite(rmt_hb_pin_b[i], LOW);
        }
        rmt_hb_states[i] = 1 - rmt_hb_states[i];
    } else {
        rmt_sg_states[i] = !rmt_sg_states[i];
        digitalWrite(rmt_sg_pins[i], rmt_sg_states[i] ? HIGH : LOW);
    }
    rmt_test_send_state();
}

static void rmt_test_end()
{
    if(rmt_test_is_bridge) {
        for(int i = 0; i < RMT_HB_COUNT; i++) {
            digitalWrite(rmt_hb_pin_a[i], LOW);
            digitalWrite(rmt_hb_pin_b[i], LOW);
        }
    } else {
        for(int i = 0; i < RMT_SG_COUNT; i++) {
            digitalWrite(rmt_sg_pins[i], LOW);
        }
    }
    rmt_test_active = false;
}

/* -------------------------------------------------------
 * Attenuator Type Detection
 * ------------------------------------------------------- */

int getAttenuator()
{
    // analogReadResolution(12);
    int raw = analogRead(ADC_SELECT_PIN);
    float v = (float)raw / 4095.0f * 3.3f;

    Serial.print("Analog: raw=");
    Serial.print(raw);
    Serial.print(" float=");
    Serial.println(v, 3);

    if(v >= 0.0f && v < 0.8f) return ATTENUATOR_26_5GHz;
    if(v >= 0.8f && v < 1.6f) return ATTENUATOR_RS_141DB;
    if(v >= 1.6f && v < 2.4f) return ATTENUATOR_B;
    if(v >= 2.4f)             return ATTENUATOR_RS_135DB;
    return -1;
}

/* -------------------------------------------------------
 * Display helpers
 * ------------------------------------------------------- */

static void drawCursor()
{
    uint8_t cursor_y = 49;
    uint8_t cursor_x = 10 + (uint8_t)selected_digit * 26;
    for(uint8_t i = 0; i < 24; i++) {
        display.setPixel(cursor_x + i, cursor_y, true);
    }
}

static void update_attenuation_display_state(int32_t db_value)
{
    int32_t dv = db_value;
    if(dv < 0) dv = 0;

    int32_t prev_db = current_db;
    current_db = dv;
    if(current_db != prev_db) {
        save_persisted_db_if_changed(current_db);
    }

    if(test_mode) {
        if(att) att->update_test_display();
        return;
    }

    display.clear();
    display.drawBigNumber(10, 16, (uint16_t)dv);
    display.drawString(90, 28, "dB");
    drawCursor();
    display.drawString(0, 56, auto_set_mode ? "AUTO-SET: ON" : "AUTO-SET: OFF");
    display.display();
}

static void refresh_attenuation_display()
{
    update_attenuation_display_state(current_db);
}

/* -------------------------------------------------------
 * Attenuation application
 * ------------------------------------------------------- */

static void apply_relays(int32_t db_value)
{
    if(att) att->apply(db_value);
}

static void apply_attenuation(int32_t db_value)
{
    update_attenuation_display_state(db_value);
    if(!suppress_relay_update) apply_relays(db_value);
}

static void apply_attenuation_from_esp_db(int32_t db_value)
{
    update_attenuation_display_state(db_value);
    if(!auto_set_mode) {
        Serial.print("ESP32 dB command -> display only: ");
        Serial.print(current_db);
        Serial.println(" dB");
        return;
    }
    /* Coalesce schnelle Repeats: Relais erst schalten wenn keine weiteren
     * Frames im UART-RX-Puffer warten (spart delay(20) je Bit-Wechsel). */
    if(Serial1.available()) return;
    if(!suppress_relay_update) apply_relays(current_db);
}

static void apply_attenuation_from_esp_set(int32_t db_value)
{
    update_attenuation_display_state(db_value);
    if(!suppress_relay_update) apply_relays(current_db);
}

/* -------------------------------------------------------
 * Encoder helpers (depend on active attenuator)
 * ------------------------------------------------------- */

static int encoder_step()
{
    if(selected_digit == 0) return 100;
    if(selected_digit == 1) return 10;
    return att ? att->step_db() : 1;
}

static int32_t att_max_db()
{
    return att ? att->max_db() : 110;
}

/* -------------------------------------------------------
 * Info screen: zeigt erkannten Attenuator + ADC-Spannung,
 * wartet auf Tastendruck, dann zurueck ins Menue.
 * ------------------------------------------------------- */
static void info_screen()
{
    /* Taste vom Menü-Klick kann noch gedrückt sein – erst loslassen abwarten */
    while(digitalRead(ENCODER_SW) == LOW) { delay(10); }
    delay(50);

    analogReadResolution(12);
    int adc_raw = analogRead(ADC_SELECT_PIN);
    float adc_v = (float)adc_raw / 4095.0f * 3.3f;
    int att_code = getAttenuator();
    const char* att_label = att ? att->att_name() : "UNKNOWN";
    char buf[24];
    display.clear();
    display.drawString(15,  0, "Att detected");
    //snprintf(buf, sizeof(buf), "ADC:%d.%02dV %d",(int)adc_v, (int)(adc_v * 100) % 100, adc_raw);
    snprintf(buf, sizeof(buf), "ADC: %d.%02dV",(int)adc_v, (int)(adc_v * 100) % 100);
    display.drawString(0, 16, buf);
    snprintf(buf, sizeof(buf), "Code:%d %s", att_code, att_label);
    display.drawString(0, 32, buf);
    display.drawString(0, 56, "Taste: zurueck");
    display.display();

    /* Auf naechsten Tastendruck warten, dann Loslassen abwarten */
    while(digitalRead(ENCODER_SW) == HIGH) { delay(10); }
    while(digitalRead(ENCODER_SW) == LOW)  { delay(10); }
    delay(50);
}

/* -------------------------------------------------------
 * Menu (LongPress im Normalbetrieb / Taste beim Start)
 * Auswahl per Drehregler, Bestätigung per Taste.
 * Menüpunkte: "Zurueck"  → kehrt in den Normalbetrieb zurück
 *             "Info"     → info_screen()
 *             "Bridge"   → hbridge_startup_test()
 *             "statisch" → static_gpio_test()
 *             "Reset"    → NVIC_SystemReset()
 * ------------------------------------------------------- */
static void runtime_menu()
{
    /* Test-Eintrag je nach Relais-Modus des aktiven Attenuators */
    bool use_bridge = !att || (att->relay_mode() == BRIDGE);
    const char* test_label = use_bridge ? "Bridge" : "statisch";

    const char* menu_items[4] = { "Zurueck", "Info", test_label, "Reset" };
    const int MENU_COUNT = 4;
    int sel = 0;

    auto draw_menu = [&]() {
        display.clear();
        display.drawString(0, 0, "MENU");
        for(int i = 0; i < MENU_COUNT; i++) {
            if(i == sel) display.drawString(0, 16 + i * 12, ">");
            display.drawString(10, 16 + i * 12, menu_items[i]);
        }
        display.display();
    };

    /* Taste kann noch gedrückt sein – erst loslassen abwarten */
    while(digitalRead(ENCODER_SW) == LOW) { delay(10); }
    delay(50);

    draw_menu();

    int last_clk = digitalRead(ENCODER_CLK);
    int last_sw  = HIGH;

    while(true) {
        int clk = digitalRead(ENCODER_CLK);
        if(clk != last_clk && clk == LOW) {
            int dir = (digitalRead(ENCODER_DT) != clk) ? 1 : -1;
            sel = (sel + (dir > 0 ? 1 : MENU_COUNT - 1)) % MENU_COUNT;
            draw_menu();
        }
        last_clk = clk;

        int sw = digitalRead(ENCODER_SW);
        if(sw == LOW && last_sw == HIGH) {
            delay(20); /* debounce */
            while(digitalRead(ENCODER_SW) == LOW) { delay(10); }
            delay(50);
            if(sel == 0) {
                return; /* Zurueck -> Normalbetrieb */
            } else if(sel == 1) {
                info_screen();
                draw_menu();
            } else if(sel == 2) {
                if(use_bridge) hbridge_startup_test();
                else           static_gpio_test();
                draw_menu();
            } else { /* Reset */
                display.clear();
                display.drawString(0, 24, "RESET...");
                display.display();
                delay(200);
                NVIC_SystemReset();
            }
        }
        last_sw = sw;
        delay(5);
    }
}

/* -------------------------------------------------------
 * setup()
 * ------------------------------------------------------- */
void setup()
{
    delay(1000);
    
    /* Init PICO Led */
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    /* Encoder pins early – needed for startup test check */
    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT,  INPUT_PULLUP);
    pinMode(ENCODER_SW,  INPUT_PULLUP);
    delay(10);

    /* Init ESP32 serial */
    Serial1.begin(115200);
    Serial1.setTimeout(20);
    delay(10);

    /* Init debug serial */
    Serial.begin(115200);
    delay(500);

    Serial.println("\n\n=================================");
    Serial.println("Raspberry Pi Pico Attenuator");
    Serial.println("Version 0.4");
    Serial.println("=================================\n");
    Serial.println("Serial1 (GP0 TX / GP1 RX) ready for ESP32-Controller communication");

    /*  Init I2C */
    Wire.begin();

    /* Init Pico Display */
    if(display.init()) {
        Serial.print("SSD1306 ready on I2C 0x");
        Serial.println(display.address(), HEX);
    } else {
        Serial.println("SSD1306 not found on I2C 0x3C/0x3D");
    }

    /* detect Attenuator type by ADC Voltage */
    /* ADC_SELECT_PIN (A0/GPIO26) – read once to determine attenuator type */
    pinMode(ADC_SELECT_PIN, INPUT);
    analogReadResolution(12);
    delay(100);
 
    /* Create attenuator instance for the detected hardware type */
    att = create_attenuator(getAttenuator());

    /* Setup attenuator GPIOs */
    if(att) att->setup();

    /* Show attenuator info screen */
    if(!att) {
        char buf[24];
        display.clear();
        snprintf(buf, sizeof(buf), "Att-Code: %d", getAttenuator());
        display.drawString(3, 10, "Error");
        display.drawString(3, 20, "Attenuator");
        display.drawString(3, 30, buf);
        display.display();
        while (1) {};
    }

    init_persistent_storage();
    load_persisted_db();
    /* Gespeicherten Wert auf gültigen Step des aktiven Attenuators runden */
    if(att) {
        int32_t s = att->step_db();
        if(s > 1) current_db = (current_db / s) * s;
        if(current_db > att->max_db()) current_db = att->max_db();
        persisted_db_cache = current_db;
    }


    send_info_to_esp32();

    /* Menu: Encoder-Taste beim Einschalten gedrückt halten */
    if(digitalRead(ENCODER_SW) == LOW) {
        runtime_menu();
    }

    encoder_last_clk = digitalRead(ENCODER_CLK);
    encoder_last_sw  = digitalRead(ENCODER_SW);
    Serial.println("KY-040 Rotary Encoder initialized (GP19/20/21)");

    apply_attenuation(current_db);

    // send_info_to_esp32();
    //delay(2000);
    //delay(100);
    for(int i = 0; i < 3; i++) {
        sync_state_to_esp32();
        delay(80);
    }

#if RAW_UART_TEST_MODE
    update_raw_uart_test_display();
#endif
}

/* -------------------------------------------------------
 * loop()
 * ------------------------------------------------------- */
void loop()
{
    unsigned long now = millis();

    /* Hardware safety watchdog – must run first, unconditionally */
    if(att) att->poll();

    /* LED control */
    if(led_solid_mode) {
        if(now - led_solid_start >= 2000) {
            led_solid_mode = false;
            last_led_toggle = now;
        }
    } else {
        if(now - last_led_toggle >= 300) {
            last_led_toggle = now;
            led_state = !led_state;
            digitalWrite(LED_BUILTIN, led_state ? HIGH : LOW);
        }
    }

    /* Read KY-040 Rotary Encoder */
    int clk_state = digitalRead(ENCODER_CLK);
    if(clk_state != encoder_last_clk && clk_state == LOW) {
        if(test_mode) {
            int dir = (digitalRead(ENCODER_DT) != clk_state) ? 1 : -1;
            if(att) att->test_rotate(dir);
        } else {
            Serial.println("*\n");
            int step      = encoder_step();
            int32_t max_v = att_max_db();
            int new_db    = current_db;

            if(digitalRead(ENCODER_DT) != clk_state) {
                new_db += step;
                if(new_db > max_v) new_db = max_v;
                encoder_position++;
            } else {
                new_db -= step;
                if(new_db < 0) new_db = 0;
                encoder_position--;
            }

            if(new_db != current_db) {
                suppress_relay_update = true;
                apply_attenuation(new_db);
                suppress_relay_update = false;
                encoder_apply_pending = true;
                last_encoder_change   = now;

                Serial1.print(current_db);
                Serial1.println("dB");

                led_solid_mode  = true;
                led_solid_start = now;
                digitalWrite(LED_BUILTIN, HIGH);
            }
        }
    }
    encoder_last_clk = clk_state;

    if(encoder_apply_pending && !test_mode && (now - last_encoder_change) >= ENCODER_SETTLE_TIME) {
        encoder_apply_pending = false;
        Serial.print("Encoder settled -> apply ");
        Serial.print(current_db);
        Serial.println(" dB");
        apply_relays(current_db);
        Serial1.print(current_db);
        Serial1.println("dB");
    }

    /* Set-Time: nach Ruhe der TIMED-Frames Relais schalten */
    if(timed_apply_pending && !test_mode && (now - timed_last_rx) >= TIMED_SETTLE_TIME) {
        timed_apply_pending = false;
        Serial.print("TIMED settled -> apply ");
        Serial.print(current_db);
        Serial.println(" dB");
        apply_relays(current_db);
        Serial1.print(current_db);
        Serial1.println("dB");
        led_solid_mode  = true;
        led_solid_start = now;
        digitalWrite(LED_BUILTIN, HIGH);
    }

    /* Read Encoder Button */
    int sw_state = digitalRead(ENCODER_SW);

    if(sw_state == LOW && encoder_last_sw == HIGH) {
        button_press_start   = now;
        button_was_pressed   = true;
        long_press_executed  = false;
    }

    /* LongPress -> Runtime-Menue anzeigen */
    if(button_was_pressed && !long_press_executed && sw_state == LOW
       && (now - button_press_start) >= LONG_PRESS_TIME) {
        long_press_executed = true;
        runtime_menu();
        refresh_attenuation_display();
    }

    if(sw_state == HIGH && encoder_last_sw == LOW && button_was_pressed) {
        button_was_pressed = false;

        if(test_mode) {
            if(att) att->test_toggle();
        } else if(!long_press_executed) {
            if(waiting_for_double_click && (now - last_click_time) < DOUBLE_CLICK_TIME) {
                Serial.println("DOUBLE CLICK -> Toggle Digit Selection");
                waiting_for_double_click = false;
                { int md = att ? att->digit_count() : 2;
                  selected_digit = (selected_digit >= md - 1) ? 0 : (selected_digit + 1); }
                refresh_attenuation_display();
                Serial1.print("SEL");
                Serial1.println(selected_digit);
                led_solid_mode  = true;
                led_solid_start = now;
                digitalWrite(LED_BUILTIN, HIGH);
            } else {
                waiting_for_double_click = true;
                last_click_time = now;
            }
        }
    }
    encoder_last_sw = sw_state;

    if(waiting_for_double_click && (now - last_click_time) >= DOUBLE_CLICK_TIME) {
        waiting_for_double_click = false;
        if(!test_mode && !auto_set_mode) {
            Serial.print("SINGLE CLICK -> Apply value: ");
            Serial.print(current_db);
            Serial.println(" dB (AUTO-Set was OFF)");
            apply_relays(current_db);
            Serial1.print(current_db);
            Serial1.println("dB");
            led_solid_mode  = true;
            led_solid_start = now;
            digitalWrite(LED_BUILTIN, HIGH);
        }
    }

    /* Check Serial1 (from ESP32) */
#if RAW_UART_TEST_MODE
    while(Serial1.available()) {
        raw_uart_last_value = Serial1.read();
        raw_uart_rx_count++;
        Serial.print("RAW RX #"); Serial.print(raw_uart_rx_count);
        Serial.print(": "); Serial.println(raw_uart_last_value);
        update_raw_uart_test_display();
        led_solid_mode  = true;
        led_solid_start = now;
        digitalWrite(LED_BUILTIN, HIGH);
    }
#else
    if(Serial1.available()) {
        String input = Serial1.readStringUntil('\n');
        input.trim();
        Serial.print("RX from ESP: \""); Serial.print(input); Serial.println("\"");

        if(input == "?") {
            Serial1.print("Current: "); Serial1.print(current_db); Serial1.println(" dB");
        } else if(input.startsWith("SEL")) {
            int digit = input.substring(3).toInt();
            int max_digits = att ? att->digit_count() : 2;
            if(digit >= 0 && digit < max_digits) {
                selected_digit = digit;
                Serial.print("ESP32 selected digit: "); Serial.println(digit);
                refresh_attenuation_display();
            }
        } else if(input.startsWith("AUTO:")) {
            auto_set_mode = input.endsWith("ON");
            Serial.print("ESP32 AUTO-Set: "); Serial.println(auto_set_mode ? "ON" : "OFF");
            refresh_attenuation_display();
        } else if(input == "TEST:START") {
            rmt_test_start();
        } else if(input.startsWith("TEST:SEL:")) {
            rmt_test_select(input.substring(9).toInt());
        } else if(input == "TEST:ACTION") {
            if(rmt_test_active) rmt_test_action();
        } else if(input == "TEST:END") {
            rmt_test_end();
        } else if(input.startsWith("TIMED:")) {
            /* Set-Time: Anzeige sofort, Relais nach TIMED_SETTLE_TIME Ruhe */
            String sub = input.substring(6);
            sub.toLowerCase();
            int dbPos = sub.indexOf("db");
            if(dbPos >= 0) sub = sub.substring(0, dbPos);
            sub.trim();
            int val = sub.toInt();
            if(val > 0 || sub == "0") {
                Serial.print("TIMED RX: "); Serial.println(val);
                update_attenuation_display_state(val);
                timed_apply_pending = true;
                timed_last_rx       = now;
                led_solid_mode  = true;
                led_solid_start = now;
                digitalWrite(LED_BUILTIN, HIGH);
            }
        } else {
            bool force_apply = false;
            if(input.startsWith("SET:")) {
                force_apply = true;
                input = input.substring(4);
                input.trim();
            }
            input.toLowerCase();
            int dbPos = input.indexOf("db");
            if(dbPos > 0) input = input.substring(0, dbPos);
            int val = input.toInt();
            if(val > 0 || input == "0") {
                if(force_apply) {
                    Serial.print("ESP32 SET command: "); Serial.print(val); Serial.println(" dB");
                    apply_attenuation_from_esp_set(val);
                } else {
                    apply_attenuation_from_esp_db(val);
                }
                led_solid_mode  = true;
                led_solid_start = now;
                digitalWrite(LED_BUILTIN, HIGH);
            }
        }
    }
#endif

    /* Check USB Serial */
    while(Serial.available()) {
        char c = Serial.read();
        if(c == '\n' || c == '\r') {
            if(serial_buffer.length() > 0) {
                String input = serial_buffer;
                serial_buffer = "";
                input.trim();
                if(input == "?") {
                    Serial.print("Current: "); Serial.print(current_db); Serial.println(" dB");
                } else {
                    input.toLowerCase();
                    int dbPos = input.indexOf("db");
                    if(dbPos > 0) input = input.substring(0, dbPos);
                    int val = input.toInt();
                    if(val > 0 || input == "0") {
                        apply_attenuation(val);
                        Serial1.print(val); Serial1.println("dB");
                        led_solid_mode  = true;
                        led_solid_start = now;
                        digitalWrite(LED_BUILTIN, HIGH);
                    }
                }
            }
        } else if(c >= 32 && c < 127) {
            serial_buffer += c;
        }
    }

    delay(2);
}
