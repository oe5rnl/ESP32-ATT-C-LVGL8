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

    Serial.printf("ATT: %d dB  [10=%d 20=%d 40a=%d 40b=%d]\n",
                  (int)att, a10, a20, a40a, a40b);
}

void setup()
{
    Serial.begin(115200);
    delay(2000);  // Wait for USB serial
    
    Serial.println("\n=================================");
    Serial.println("Raspberry Pi Pico Attenuator");
    Serial.println("Version 0.1");
    Serial.println("=================================\n");

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
    if(Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        
        if(input == "?") {
            Serial.printf("Current: %d dB\n", (int)current_db);
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
