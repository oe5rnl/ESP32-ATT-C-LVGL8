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
 *   att_types.h        – type constants, MODE_SELECT pins, getAttenuator()
 */

#include <Arduino.h>
#include <Wire.h>
#include <cstdio>
#include <BlockDevice.h>
#include <LittleFileSystem.h>
#include "SSD1306.h"
#include "att_types.h"
#include "Attenuator.h"

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

/* -------------------------------------------------------
 * Attenuator Type Detection
 * ------------------------------------------------------- */

int getAttenuator()
{
    if(digitalRead(MODE_SELECT0) == LOW  && digitalRead(MODE_SELECT1) == LOW)  return ATTENUATOR_RS_135DB;
    if(digitalRead(MODE_SELECT0) == LOW  && digitalRead(MODE_SELECT1) == HIGH) return ATTENUATOR_A;
    if(digitalRead(MODE_SELECT0) == HIGH && digitalRead(MODE_SELECT1) == LOW)  return ATTENUATOR_B;
    if(digitalRead(MODE_SELECT0) == HIGH && digitalRead(MODE_SELECT1) == HIGH) return ATTENUATOR_26_5GHz;
    return -1;
}

/* -------------------------------------------------------
 * Display helpers
 * ------------------------------------------------------- */

static void drawCursor()
{
    uint8_t cursor_y = 49;
    uint8_t cursor_x = (selected_digit == 1) ? (10 + 24 + 2) : 10;
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
    return att ? att->step_db() : 10;
}

static int32_t att_max_db()
{
    return att ? att->max_db() : 110;
}

/* -------------------------------------------------------
 * setup()
 * ------------------------------------------------------- */
void setup()
{
    Serial.begin(115200);
    delay(100);

    Serial.println("\n\n=================================");
    Serial.println("Raspberry Pi Pico Attenuator");
    Serial.println("Version 0.4");
    Serial.println("=================================\n");

    /* Mode Select Inputs FIRST – needed by getAttenuator() / create_attenuator() */
    pinMode(MODE_SELECT0, INPUT_PULLUP);
    pinMode(MODE_SELECT1, INPUT_PULLUP);
    delay(10);

    /* Create attenuator instance for the detected hardware type */
    att = create_attenuator(getAttenuator());

    init_persistent_storage();
    load_persisted_db();

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    Wire.begin();
    display.init();

    /* Show attenuator info screen */
    if(att) {
        att->show_info();
    } else {
        char buf[24];
        display.clear();
        snprintf(buf, sizeof(buf), "Att-Code: %d", getAttenuator());
        display.drawString(3, 10, "Error");
        display.drawString(3, 20, "Attenuator");
        display.drawString(3, 30, buf);
        display.display();
    }
    delay(2000);

    /* Setup attenuator GPIOs */
    if(att) att->setup();

    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT,  INPUT_PULLUP);
    pinMode(ENCODER_SW,  INPUT_PULLUP);
    encoder_last_clk = digitalRead(ENCODER_CLK);
    encoder_last_sw  = digitalRead(ENCODER_SW);
    Serial.println("KY-040 Rotary Encoder initialized (GP19/20/21)");

    Serial1.begin(115200);
    Serial1.setTimeout(100);
    Serial.println("Serial1 (GP0 TX / GP1 RX) ready for ESP32-Controller communication");

    apply_attenuation(current_db);

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

                Serial1.print(new_db);
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
    }

    /* Read Encoder Button */
    int sw_state = digitalRead(ENCODER_SW);

    if(sw_state == LOW && encoder_last_sw == HIGH) {
        button_press_start   = now;
        button_was_pressed   = true;
        long_press_executed  = false;
        waiting_for_double_click = false;
    }

    if(sw_state == LOW && button_was_pressed && !long_press_executed) {
        if((now - button_press_start) >= 5000) {
            test_mode = true;
            if(att) att->test_init();

            Serial.println("\n*** TEST MODE ACTIVATED ***");
            Serial.println("Turn: Select relay");
            Serial.println("Press: Toggle selected relay");
            Serial.println("Exit: Power cycle\n");

            if(att) att->update_test_display();

            digitalWrite(LED_BUILTIN, HIGH);
            led_solid_mode  = true;
            led_solid_start = now;
            long_press_executed = true;
        }
    }

    if(sw_state == HIGH && encoder_last_sw == LOW && button_was_pressed) {
        button_was_pressed = false;

        if(test_mode) {
            if(att) att->test_toggle();
        } else if(!long_press_executed) {
            if(waiting_for_double_click && (now - last_click_time) < DOUBLE_CLICK_TIME) {
                Serial.println("DOUBLE CLICK -> Toggle Digit Selection");
                waiting_for_double_click = false;
                selected_digit = (selected_digit == 0) ? 1 : 0;
                refresh_attenuation_display();
                Serial1.println("DIGIT");
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

        if(input == "?") {
            Serial1.print("Current: "); Serial1.print(current_db); Serial1.println(" dB");
        } else if(input.startsWith("SEL")) {
            int digit = input.substring(3).toInt();
            if(digit == 0 || digit == 1) {
                selected_digit = digit;
                Serial.print("ESP32 selected digit: "); Serial.println(digit);
                refresh_attenuation_display();
            }
        } else if(input.startsWith("AUTO:")) {
            auto_set_mode = input.endsWith("ON");
            Serial.print("ESP32 AUTO-Set: "); Serial.println(auto_set_mode ? "ON" : "OFF");
            refresh_attenuation_display();
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

    delay(10);
}
