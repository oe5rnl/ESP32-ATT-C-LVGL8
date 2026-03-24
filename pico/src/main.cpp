/* Raspberry Pi Pico - Attenuator Controller
 *
 * Version 0.1 - 2026-03-21
 * 
 * Hardware: Raspberry Pi Pico
 * GPIO Mapping: Attenuator control 
 */

#include <Arduino.h>
#include <Wire.h>
#include "SSD1306.h"

/* I2C Display SSD1306 (128x64) using I2C0 on GP4 (SDA) / GP5 (SCL) */
SSD1306 display(&Wire);


#define ATTENUATOR_26_5GHz          1       
#define ATTENUATOR_GPIO_RS_70DB     2    


/* 26.5 GHz Attenuator ***********************************************************
*  Pads: 10 dB, 20 dB, 40 dB (A), 40 dB (B)  →  max 110 dB in 10 dB steps
*  Der Attenuator verfügt über eine Logik mit H Brücke
*  daher 1 Bit je Relais - high aktiv 
*/
#define ATT_GPIO_10DB    10
#define ATT_GPIO_20DB    11
#define ATT_GPIO_40DB_A  12
#define ATT_GPIO_40DB_B  13

/* Mode Select Inputs with Pull-up */
#define MODE_SELECT0     2
#define MODE_SELECT1     3

/* KY-040 Rotary Encoder */
#define ENCODER_CLK      19    // A
#define ENCODER_DT       20    // B
#define ENCODER_SW       21    // Switch/Button



/* RS 7 Relais Attenuatur   ******************************************************
* Pads:  ????
*/
// Relais 1: 10 dB ??
#define GPIO_2_7    // PIN=4  GPIO_2  Relais_1 a
#define GPIO_3_7    // PIN=5  GPIO_3  Relais_1 b

// Relais 2: 20 dB ??
#define GPIO_4_7    // PIN=6  GPIO_4  Relais_2 a
#define GPIO_5_7    // PIN=7  GPIO_5  Relais_2 b

// Relais 3: 40 dB ??
#define GPIO_6_7    // PIN=9  GPIO_6  Relais_3 a
#define GPIO_7_7    // PIN=10 GPIO_7  Relais_3 b

// Relais 4: 70 dB ??
#define GPIO_8_7    // PIN=11 GPIO_8   Relais_4 a
#define GPIO_9_7    // PIN=12 GPIO_9   Relais_4 b

// Relais  5
#define GPIO_10_7  // PIN=14 GPIO_10  Relais_5 a 
#define GPIO_11_7  // PIN=15 GPIO_11  Relais_5 b

// Relais 6
// 16 GPIO_12  Relais_6 a
#define GPIO_12_7  // PIN=16 GPIO_12  Relais_6 b
#define GPIO_13_7  // PIN=17 GPIO_13  Relais_6 b

// Relais 7
// 19 GPIO_14  Relais_7 a
#define GPIO_14_7  // PIN=19 GPIO_14  Relais_7 b
#define GPIO_15_7  // PIN=20 GPIO_15  Relais_7 a

// Relais 8
#define GPIO_16_7  // PIN=21 GPIO_16  Relais_8 a
#define GPIO_17_7  // PIN=22 GPIO_17  Relais_8 b





// Relais 


static int32_t current_db = 0;
static unsigned long last_led_toggle = 0;
static bool led_state = false;
static bool led_solid_mode = false;  /* true = 2s solid, false = blink */
static unsigned long led_solid_start = 0;
static String serial_buffer = "";  /* Buffer for USB serial input */
static int attenuator = 0;

/* Rotary Encoder State */
static int encoder_last_clk = HIGH;
static int encoder_last_sw = HIGH;
static int encoder_position = 0;
static unsigned long button_press_start = 0;  /* Zeit des Button-Drucks */
static bool button_was_pressed = false;       /* Flag für Button-Press-Erkennung */
static bool long_press_executed = false;      /* Flag: Langdruck bereits ausgeführt */

/* Display Cursor State */
static int selected_digit = 1;  /* 0 = Hunderter, 1 = Zehner (Start bei Zehner wie ESP32) */

/* AUTO-Set Mode */
static bool auto_set_mode = false;  /* AUTO-Set on/off */


int getAttenuator()
{
    if(digitalRead(MODE_SELECT0) == HIGH && digitalRead(MODE_SELECT1) == HIGH) {
        return ATTENUATOR_26_5GHz;
    }
    else if(digitalRead(MODE_SELECT0) == LOW && digitalRead(MODE_SELECT1) == HIGH) {
        return ATTENUATOR_GPIO_RS_70DB;
    }
    else {
        return 0;  // Unknown
    }    
}

void setup_attenuation_26()
{
    /* Attenuator GPIO init */
    pinMode(ATT_GPIO_10DB,   OUTPUT);
    pinMode(ATT_GPIO_20DB,   OUTPUT);
    pinMode(ATT_GPIO_40DB_A, OUTPUT);
    pinMode(ATT_GPIO_40DB_B, OUTPUT);
    
    // All OFF (LOW = inactive, HIGH = active)
    digitalWrite(ATT_GPIO_10DB,   LOW);
    digitalWrite(ATT_GPIO_20DB,   LOW);
    digitalWrite(ATT_GPIO_40DB_A, LOW);
    digitalWrite(ATT_GPIO_40DB_B, LOW);
        
}

void apply_attenuation_26(int32_t db_value)
{
    // Round to 10 dB steps
    int32_t att = (db_value / 10) * 10;
    if(att > 110) att = 110;
    if(att < 0) att = 0;
    
    current_db = att;

    int32_t rem = att;
    bool a40a = (rem >= 40); if(a40a) rem -= 40;
    bool a40b = (rem >= 40); if(a40b) rem -= 40;
    bool a20  = (rem >= 20); if(a20)  rem -= 20;
    bool a10  = (rem >= 10);

    digitalWrite(ATT_GPIO_10DB,   a10  ? HIGH : LOW);
    digitalWrite(ATT_GPIO_20DB,   a20  ? HIGH : LOW);
    digitalWrite(ATT_GPIO_40DB_A, a40a ? HIGH : LOW);
    digitalWrite(ATT_GPIO_40DB_B, a40b ? HIGH : LOW);

    Serial.print("ATT: ");
    Serial.print(att);
    Serial.print(" dB  [10=");
    Serial.print(a10);
    Serial.print(" 20=");
    Serial.print(a20);
    Serial.print(" 40a=");
    Serial.print(a40a);
    Serial.print(" 40b=");
    Serial.print(a40b);
    Serial.println("]");
}


void setup_attenuation_70db()
{
}

void apply_attenuation_70db(int32_t db_value)
{
}

/* Zeichne Cursor (Unterstrich) unter der ausgewählten Ziffer */
void drawCursor()
{
    /* Digit-Positionen: Breite=24px, Abstand=2px, Start bei x=10, y=16 */
    /* Unterstrich bei y = 16 + 32 + 1 = 49 */
    uint8_t cursor_y = 49;
    uint8_t cursor_x = 10;
    
    if(selected_digit == 1) {
        cursor_x = 10 + 24 + 2;  /* Zehnerstelle */
    }
    /* Digit 2 (Einerstelle) wird nicht verwendet für 10dB-Schritte */
    
    /* Zeichne Unterstrich (24 Pixel breit) */
    for(uint8_t i = 0; i < 24; i++) {
        display.setPixel(cursor_x + i, cursor_y, true);
    }
}

void apply_attenuation(int32_t db_value)
{
    // Round to 10 dB steps
    int32_t att = (db_value / 10) * 10;
    if(att > 110) att = 110;
    if(att < 0) att = 0;
    
    current_db = att;
    
    /* Update I2C Display ALWAYS with large digits */
    display.clear();
    display.drawBigNumber(10, 16, (uint16_t)att);  /* 3-digit number at (10, 16) */
    display.drawString(90, 28, "dB");              /* "dB" label */
    drawCursor();  /* Unterstrich unter ausgewählter Stelle */
    
    /* AUTO-Set Status anzeigen (kleine Schrift unten) */
    if(auto_set_mode) {
        display.drawString(0, 56, "AUTO-SET: ON");
    } else {
        display.drawString(0, 56, "AUTO-SET: OFF");
    }
    
    display.display();
    
    /* Relais nur schalten wenn AUTO-Set aktiviert ist */
    if(!auto_set_mode) {
        Serial.print("Display only (AUTO-Set OFF): ");
        Serial.print(att);
        Serial.println(" dB");
        return;  /* Keine Relais-Schaltung */
    }
    
    // wenn MODE_SELECT0 und MODE_SELECT1 high -> apply_attenuation_26
    if(digitalRead(MODE_SELECT0) == HIGH && digitalRead(MODE_SELECT1) == HIGH) {
        apply_attenuation_26(db_value);
    }
    // wenn MODE_SELECT0 == LOW und MODE_SELECT1 == high -> apply_attenuation_26
    else if(digitalRead(MODE_SELECT0) == LOW && digitalRead(MODE_SELECT1) == HIGH) {
        // applay_attenuation_rs70db();  // TODO: Implementieren
        Serial.print("ATT RS-7: ");
        Serial.print(att);
        Serial.println(" dB (TODO)");
    }
    else {
        Serial.print("Display: ");
        Serial.print(att);
        Serial.println(" dB (no mode)");
    }
}

void setup()
{
    Serial.begin(115200);
    delay(2000);  // Wait for USB serial
    
    Serial.println("\n\n=================================");
    Serial.println("Raspberry Pi Pico Attenuator");
    Serial.println("Version 0.2");
    Serial.println("=================================\n");
    Serial.println("Serial1 (GP0 TX / GP1 RX) ready for ESP32-Controller communication");

    /* Onboard LED init */
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);


    /* Initialize I2C on standard pins: GP4 (SDA), GP5 (SCL) for display output */
    Wire.begin();
    Serial.println("I2C initialized on GP4 (SDA) and GP5 (SCL)");
    
    /* Initialize SSD1306 Display */
    display.init();
    Serial.println("SSD1306 Display initialized");
    
    /* Mode Select Inputs with Pull-up - MUSS VOR dem Lesen konfiguriert werden! */
    pinMode(MODE_SELECT0, INPUT_PULLUP);
    pinMode(MODE_SELECT1, INPUT_PULLUP);   
    delay(10);  // Kurze Verzögerung, damit sich die Pullups stabilisieren
    
    Serial.println("Reading Mode Select pins...");
    Serial.println("  MODE_SELECT0 (GP18): " + String(digitalRead(MODE_SELECT0) == HIGH ? "HIGH" : "LOW"));
    Serial.println("  MODE_SELECT1 (GP21): " + String(digitalRead(MODE_SELECT1) == HIGH ? "HIGH" : "LOW"));
    Serial.print("Attenuator: ");

    display.clear();    
    
    // R&S 26 Ghz Attenuator
    if(getAttenuator() == ATTENUATOR_26_5GHz) {
        attenuator = ATTENUATOR_26_5GHz;
        Serial.println("R&S 26_5GHz");
        display.drawString(10, 10, "26.5 GHz");
        setup_attenuation_26();  // Initialisierung mit 0 dB
    }
    // R&S 70 dB Attenuator
    else if(getAttenuator() == ATTENUATOR_GPIO_RS_70DB) {
        attenuator = ATTENUATOR_GPIO_RS_70DB;
        Serial.println("R&S 70 dB");
        display.drawString(10, 10, "RS-70 dB");
        setup_attenuation_70db();  // TODO: Implementieren
    }
    else {
        display.drawString(10, 10, "Unknown");
        Serial.println("Unknown ");
    }

    display.display();
    delay(1000);
    
    /* KY-040 Rotary Encoder init */
    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT, INPUT_PULLUP);
    pinMode(ENCODER_SW, INPUT_PULLUP);
    encoder_last_clk = digitalRead(ENCODER_CLK);
    encoder_last_sw = digitalRead(ENCODER_SW);
    Serial.println("KY-040 Rotary Encoder initialized (GP19/20/21)");
    
    /* Serial1 for ESP32 communication: GP0 (TX), GP1 (RX) = UART0 */
    Serial1.begin(115200);
    Serial1.setTimeout(100);

    Serial.println("GPIO initialized");
    Serial.println("\nCommands (USB + ENTER):");
    Serial.println("  0-110 + ENTER : Set attenuation (10 dB steps)");
    Serial.println("  ? + ENTER     : Show current value");
    Serial.println("\nWaiting for ESP32 on Serial1 (GP0/GP1)...\n");
    
    apply_attenuation(0);
}

void loop()
{
    unsigned long now = millis();
    
    /* LED control: solid for 2s after receiving dB string, then blink */
    if(led_solid_mode) {
        if(now - led_solid_start >= 2000) {
            /* 2 seconds elapsed, switch back to blink mode */
            led_solid_mode = false;
            last_led_toggle = now;
        }
    }
    else {
        /* Blink mode: toggle LED every 300ms */
        if(now - last_led_toggle >= 300) {
            last_led_toggle = now;
            led_state = !led_state;
            digitalWrite(LED_BUILTIN, led_state ? HIGH : LOW);
            // Serial.println("PICO");
        }
    }

    /* Read KY-040 Rotary Encoder - steuert ausgewählte Stelle */
    int clk_state = digitalRead(ENCODER_CLK);
    if(clk_state != encoder_last_clk && clk_state == LOW) {
        /* CLK hat eine fallende Flanke */
        int step = (selected_digit == 0) ? 100 : 10;  /* 100dB für Hunderter, 10dB für Zehner */
        int new_db = current_db;
        if(digitalRead(ENCODER_DT) != clk_state) {
            /* DT ist anders als CLK -> Drehung im Uhrzeigersinn (CW) = Erhöhen */
            new_db += step;
            if(new_db > 110) new_db = 110;
            encoder_position++;
            Serial.print("Encoder CW  -> ");
        }
        else {
            /* DT ist gleich wie CLK -> Drehung gegen Uhrzeigersinn (CCW) = Verringern */
            new_db -= step;
            if(new_db < 0) new_db = 0;
            encoder_position--;
            Serial.print("Encoder CCW -> ");
        }
        
        /* Anwendung der neuen Dämpfung */
        if(new_db != current_db) {
            apply_attenuation(new_db);
            Serial.print(new_db);
            Serial.println(" dB");
            
            /* Sende neuen Wert an ESP32 über Serial1 */
            Serial1.print(new_db);
            Serial1.println("dB");
            
            /* LED solid für 2 Sekunden */
            led_solid_mode = true;
            led_solid_start = now;
            digitalWrite(LED_BUILTIN, HIGH);
        }
    }
    encoder_last_clk = clk_state;
    
    /* Read Encoder Button/Switch mit Langdruck-Erkennung */
    int sw_state = digitalRead(ENCODER_SW);
    
    /* Fallende Flanke: Button wurde gedrückt */
    if(sw_state == LOW && encoder_last_sw == HIGH) {
        button_press_start = now;
        button_was_pressed = true;
        long_press_executed = false;
    }
    
    /* Button ist gedrückt: Prüfe auf Langdruck (1 Sekunde) */
    if(sw_state == LOW && button_was_pressed && !long_press_executed) {
        unsigned long press_duration = now - button_press_start;
        if(press_duration >= 1000) {
            /* Langer Druck (>= 1s): Toggle AUTO-Set Modus SOFORT */
            auto_set_mode = !auto_set_mode;
            Serial.print("Encoder Button LONG PRESS -> AUTO-Set: ");
            Serial.println(auto_set_mode ? "ON" : "OFF");
            
            /* Sende AUTO-Set Status an ESP32 über Serial1 */
            Serial1.print("AUTO:");
            Serial1.println(auto_set_mode ? "ON" : "OFF");
            
            /* Update Display */
            apply_attenuation(current_db);
            
            /* LED solid für 2 Sekunden */
            led_solid_mode = true;
            led_solid_start = now;
            digitalWrite(LED_BUILTIN, HIGH);
            
            long_press_executed = true;  /* Markiere als ausgeführt */
        }
    }
    
    /* Steigende Flanke: Button wurde losgelassen */
    if(sw_state == HIGH && encoder_last_sw == LOW && button_was_pressed) {
        button_was_pressed = false;
        
        /* Nur Digit Toggle wenn KEIN Langdruck ausgeführt wurde */
        if(!long_press_executed) {
            /* Kurzer Druck (< 1s): Toggle Digit Selection */
            Serial.println("Encoder Button SHORT PRESS -> Toggle Digit Selection");
            
            /* Toggle lokale selected_digit Variable (0=Hunderter, 1=Zehner) */
            selected_digit = (selected_digit == 0) ? 1 : 0;
            
            /* Update Display mit neuem Cursor */
            apply_attenuation(current_db);
            
            /* Sende DIGIT-Befehl an ESP32 über Serial1 */
            Serial1.println("DIGIT");
            
            /* LED solid für 2 Sekunden */
            led_solid_mode = true;
            led_solid_start = now;
            digitalWrite(LED_BUILTIN, HIGH);
        }
    }
    encoder_last_sw = sw_state;

    /* Check Serial1 (from ESP32) */
    if(Serial1.available()) {
        String input = Serial1.readStringUntil('\n');
        input.trim();
        
        if(input == "?") {
            Serial1.print("Current: ");
            Serial1.print(current_db);
            Serial1.println(" dB");
        }
        else if(input.startsWith("SEL")) {
            /* Digit-Auswahl vom ESP32: SEL0, SEL1 */
            int digit = input.substring(3).toInt();
            if(digit == 0 || digit == 1) {
                selected_digit = digit;
                Serial.print("ESP32 selected digit: ");
                Serial.println(digit);
                /* Update Display mit neuem Cursor */
                apply_attenuation(current_db);
            }
        }
        else if(input.startsWith("AUTO:")) {
            /* AUTO-Set Status vom ESP32: AUTO:ON oder AUTO:OFF */
            auto_set_mode = input.endsWith("ON");
            Serial.print("ESP32 AUTO-Set: ");
            Serial.println(auto_set_mode ? "ON" : "OFF");
            /* Update Display */
            apply_attenuation(current_db);
        }
        else {
            /* Parse number: accept plain number or "xxdB" format */
            input.toLowerCase();
            int dbPos = input.indexOf("db");
            if(dbPos > 0) {
                input = input.substring(0, dbPos);  /* Remove "db" suffix */
            }
            int val = input.toInt();
            if(val > 0 || input == "0") {  /* Valid number */
                apply_attenuation(val);
                /* LED solid for 2 seconds */
                led_solid_mode = true;
                led_solid_start = now;
                digitalWrite(LED_BUILTIN, HIGH);
            }
        }
    }

    /* Check USB Serial (manual control) - character-by-character collection */
    while(Serial.available()) {
        char c = Serial.read();
        if(c == '\n' || c == '\r') {
            if(serial_buffer.length() > 0) {
                String input = serial_buffer;
                serial_buffer = "";  /* Clear buffer */
                input.trim();
                
                if(input == "?") {
                    Serial.print("Current: ");
                    Serial.print(current_db);
                    Serial.println(" dB");
                }
                else {
                    /* Parse number: accept plain number or "xxdB" format */
                    input.toLowerCase();
                    int dbPos = input.indexOf("db");
                    if(dbPos > 0) {
                        input = input.substring(0, dbPos);  /* Remove "db" suffix */
                    }
                    int val = input.toInt();
                    if(val > 0 || input == "0") {  /* Valid number */
                        apply_attenuation(val);
                        
                        /* Sende Wert auch an ESP32 über Serial1 */
                        Serial1.print(val);
                        Serial1.println("dB");
                        
                        /* LED solid for 2 seconds */
                        led_solid_mode = true;
                        led_solid_start = now;
                        digitalWrite(LED_BUILTIN, HIGH);
                    }
                }
            }
        }
        else if(c >= 32 && c < 127) {  /* Printable ASCII */
            serial_buffer += c;
        }
    }
    
    delay(10);
}
