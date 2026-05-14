/* Toggle-Test: GPIO 10-13 im 2-Sekundentakt
 *
 * Jeder Pin wird einzeln 2 s HIGH, dann LOW, dann 0.5 s Pause, dann nächster.
 * Fortschritt wird auf Serial (USB) ausgegeben.
 *
 * Flashen: pio run -e toggle_test --target upload
 */
#include <Arduino.h>

static const int PINS[]  = { 10, 11, 12, 13, 15 };
static const int N_PINS  = 5;

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    for(int i = 0; i < N_PINS; i++) {
        pinMode(PINS[i], OUTPUT);
        digitalWrite(PINS[i], LOW);
    }
    Serial.println("Toggle-Test GPIO 10-13 gestartet");
}

void loop()
{
    for(int i = 0; i < N_PINS; i++) {
        Serial.print("GPIO"); Serial.print(PINS[i]); Serial.println(" -> HIGH");
        digitalWrite(PINS[i], HIGH);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(10);
        digitalWrite(PINS[i], LOW);
        digitalWrite(LED_BUILTIN, LOW);
        Serial.print("GPIO"); Serial.print(PINS[i]); Serial.println(" -> LOW");
        delay(10);
    }
}
