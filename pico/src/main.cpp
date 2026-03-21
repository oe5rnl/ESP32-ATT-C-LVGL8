/* Raspberry Pi Pico - Attenuator Controller
 *
 * Version 0.1 - 2026-03-21
 * 
 * Hardware: Raspberry Pi Pico
 * GPIO Mapping: Attenuator control 
 */

#include <Arduino.h>
#include <U8g2lib.h>

/* I2C Display SSD1306 (128x64) using Software I2C on GP19/GP20 */
U8G2_SSD1306_128X64_NONAME_F_SW_I2C display(U8G2_R0, /* clock=*/ 19, /* data=*/ 20, /* reset=*/ U8X8_PIN_NONE);


#define ATTENUATOR_26_5GHz    1       // 2,6,10,16
#define ATTENUATOR_GPIO_RS_70DB   2    


/* 26.5 GHz Attenuator ***********************************************************
*  Pads: 10 dB, 20 dB, 40 dB (A), 40 dB (B)  →  max 110 dB in 10 dB steps
*  Der Attenuator verfügt über eine Logik mit H Brücke
*  daher 1 Bit je Relais - high aktiv 
*/
#define ATT_GPIO_10DB    2
#define ATT_GPIO_20DB    6
#define ATT_GPIO_40DB_A  10
#define ATT_GPIO_40DB_B  16


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

/* Mode Select Inputs with Pull-up */
#define MODE_SELECT0     18
#define MODE_SELECT1     21

static int32_t current_db = 0;
static unsigned long last_led_toggle = 0;
static bool led_state = false;
static bool led_solid_mode = false;  /* true = 2s solid, false = blink */
static unsigned long led_solid_start = 0;

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

    digitalWrite(ATT_GPIO_10DB,   a10  ? LOW : HIGH);
    digitalWrite(ATT_GPIO_20DB,   a20  ? LOW : HIGH);
    digitalWrite(ATT_GPIO_40DB_A, a40a ? LOW : HIGH);
    digitalWrite(ATT_GPIO_40DB_B, a40b ? LOW : HIGH);

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
    
    /* Update I2C Display */
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB24_tn);  /* Large numbers */
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", (int)att);
    display.drawStr(15, 45, buf);
    display.setFont(u8g2_font_ncenB10_tr);
    display.drawStr(75, 45, "dB");
    display.sendBuffer();
}

void apply_attenuation(int32_t db_value)
{
    // wenn MODE_SELECT0 und MODE_SELECT1 high -> apply_attenuation_26
    if(digitalRead(MODE_SELECT0) == HIGH && digitalRead(MODE_SELECT1) == LOW) {
        apply_attenuation_26(db_value);
    }
    // wenn MODE_SELECT0 == LOW und MODE_SELECT1 == high -> apply_attenuation_26
    else if(digitalRead(MODE_SELECT0) == LOW && digitalRead(MODE_SELECT1) == HIGH) {
        // applay_attenuation_rs70db();  // TODO: Implementieren
    }
    else {
        Serial.println("Unsupported mode - no attenuation applied");
    }
}

void setup()
{
    Serial.begin(115200);
    delay(2000);  // Wait for USB serial
    
    Serial.println("Attenuator PICO started");
    
    /* Initialize SSD1306 Display (Software I2C on GP19/GP20) */
    display.begin();
    Serial.println("SSD1306 Display initialized");
    
    /* Startup screen */
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB10_tr);
    display.drawStr(5, 20, "26.5 GHz");
    display.drawStr(0, 45, "Attenuator");
    display.sendBuffer();
    delay(1000);
    
    /* Serial1 for ESP32 communication: GP0 (TX), GP1 (RX) = UART0 */
    Serial1.begin(115200);
    Serial1.setTimeout(100);
    
    Serial.println("\n=================================");
    Serial.println("Raspberry Pi Pico Attenuator");
    Serial.println("Version 0.1");
    Serial.println("=================================\n");
    Serial.println("Serial1 (GP0 TX / GP1 RX) ready for ESP32");

    /* Onboard LED init */
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    /* Attenuator GPIO init */
    pinMode(ATT_GPIO_10DB,   OUTPUT);
    pinMode(ATT_GPIO_20DB,   OUTPUT);
    pinMode(ATT_GPIO_40DB_A, OUTPUT);
    pinMode(ATT_GPIO_40DB_B, OUTPUT);
    
    // All OFF (HIGH = inactive)
    digitalWrite(ATT_GPIO_10DB,   HIGH);
    digitalWrite(ATT_GPIO_20DB,   HIGH);
    digitalWrite(ATT_GPIO_40DB_A, HIGH);
    digitalWrite(ATT_GPIO_40DB_B, HIGH);
    
    /* Mode Select Inputs with Pull-up */
    pinMode(MODE_SELECT0, INPUT_PULLUP);
    pinMode(MODE_SELECT1, INPUT_PULLUP);
    
    Serial.println("GPIO initialized");
    Serial.println("\nCommands (USB):");
    Serial.println("  0-110  : Set attenuation (10 dB steps)");
    Serial.println("  ?      : Show current value");
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
            Serial.println("PICO");
        }
    }

    /* Check Serial1 (from ESP32) */
    if(Serial1.available()) {
        String input = Serial1.readStringUntil('\n');
        input.trim();
        input.toLowerCase();  /* Case-insensitive */
        
        /* Parse format: "xxdb" (case-insensitive, e.g., "50dB", "110DB") */
        int dbPos = input.indexOf("db");
        if(dbPos > 0) {
            String numStr = input.substring(0, dbPos);
            int val = numStr.toInt();
            apply_attenuation(val);
            Serial.print("ESP32 -> ");
            Serial.print(val);
            Serial.println(" dB");
            
            /* LED solid for 2 seconds */
            led_solid_mode = true;
            led_solid_start = now;
            digitalWrite(LED_BUILTIN, HIGH);

        }
    }

    /* Check USB Serial (manual control) */
    if(Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        
        if(input == "?") {
            Serial.print("Current: ");
            Serial.print(current_db);
            Serial.println(" dB");
        }
        else {
            int val = input.toInt();
            apply_attenuation(val);
        }
    }
    
    delay(10);
}
