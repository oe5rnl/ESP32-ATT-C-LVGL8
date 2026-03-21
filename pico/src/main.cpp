/* Raspberry Pi Pico - 26.5 GHz Attenuator Controller
 *
 * Version 0.1 - 2026-03-21
 * 
 * Hardware: Raspberry Pi Pico
 * GPIO Mapping: Attenuator control pins (active LOW)
 */

#include <Arduino.h>

/* Attenuator GPIO mapping (active LOW)
 * Pads: 10 dB, 20 dB, 40 dB (A), 40 dB (B)  →  max 110 dB in 10 dB steps
 * 
 * TODO: Pico GPIO-Pins definieren
 */
#define ATT_GPIO_10DB    2
#define ATT_GPIO_20DB    3
#define ATT_GPIO_40DB_A  4
#define ATT_GPIO_40DB_B  5

static int32_t current_db = 0;
static unsigned long last_led_toggle = 0;
static bool led_state = false;

void apply_attenuation(int32_t db_value)
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
}

void setup()
{
    Serial.begin(115200);
    delay(2000);  // Wait for USB serial
    
    Serial.println("\n=================================");
    Serial.println("Raspberry Pi Pico Attenuator");
    Serial.println("Version 0.1");
    Serial.println("=================================\n");

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
    
    Serial.println("GPIO initialized");
    Serial.println("\nCommands:");
    Serial.println("  0-110  : Set attenuation (10 dB steps)");
    Serial.println("  ?      : Show current value\n");
    
    apply_attenuation(0);
}

void loop()
{
    /* Toggle LED every 300ms */
    unsigned long now = millis();
    if(now - last_led_toggle >= 300) {
        last_led_toggle = now;
        led_state = !led_state;
        digitalWrite(LED_BUILTIN, led_state ? HIGH : LOW);
    }

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
            if(val >= 0 && val <= 110) {
                apply_attenuation(val);
            }
            else {
                Serial.println("ERROR: Value must be 0-110");
            }
        }
    }
    
    delay(10);
}
