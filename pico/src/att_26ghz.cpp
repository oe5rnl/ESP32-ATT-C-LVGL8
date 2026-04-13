#include "att_26ghz.h"
#include "SSD1306.h"
#include <Arduino.h>

extern SSD1306 display;

/* Static member definitions */
const char* const Att26GHz::_names[] = {"10dB", "20dB", "40A", "40B"};
const int         Att26GHz::_gpios[] = {
    ATT_GPIO_10DB, ATT_GPIO_20DB, ATT_GPIO_40DB_A, ATT_GPIO_40DB_B
};

void Att26GHz::setup()
{
    for(int i = 0; i < RELAY_COUNT; i++) {
        pinMode(_gpios[i], OUTPUT);
        digitalWrite(_gpios[i], LOW);
    }
}

void Att26GHz::apply(int32_t dv)
{
    int32_t att = (dv / step_db()) * step_db();
    if(att > max_db()) att = max_db();
    if(att < 0) att = 0;

    int32_t rem = att;
    bool a40a = (rem >= 40); if(a40a) rem -= 40;
    bool a40b = (rem >= 40); if(a40b) rem -= 40;
    bool a20  = (rem >= 20); if(a20)  rem -= 20;
    bool a10  = (rem >= 10);

    digitalWrite(ATT_GPIO_10DB,   a10  ? HIGH : LOW);
    digitalWrite(ATT_GPIO_20DB,   a20  ? HIGH : LOW);
    digitalWrite(ATT_GPIO_40DB_A, a40a ? HIGH : LOW);
    digitalWrite(ATT_GPIO_40DB_B, a40b ? HIGH : LOW);

}

void Att26GHz::show_info()
{
    Serial.println("R&S 26_5GHz");
    display.clear();
    display.drawString(3, 10, "26.5 GHz");
    display.drawString(3, 20, "Att  : 110dB");
    display.drawString(3, 30, "Steps:  10dB");
    display.display();
}

void Att26GHz::test_init()
{
    _sel = 0;
    for(int i = 0; i < RELAY_COUNT; i++) {
        _states[i] = false;
        digitalWrite(_gpios[i], LOW);
    }
}

void Att26GHz::test_rotate(int dir)
{
    _sel = (_sel + (dir > 0 ? 1 : RELAY_COUNT - 1)) % RELAY_COUNT;
    Serial.print("Test 26GHz: Relay -> ");
    Serial.println(_names[_sel]);
    update_test_display();
}

void Att26GHz::test_toggle()
{
    _states[_sel] = !_states[_sel];
    Serial.print("Test 26GHz: Toggle ");
    Serial.print(_names[_sel]);
    Serial.print(" -> ");
    Serial.println(_states[_sel] ? "ON" : "OFF");
    digitalWrite(_gpios[_sel], _states[_sel] ? HIGH : LOW);
    update_test_display();
}

void Att26GHz::update_test_display()
{
    display.clear();
    display.drawString(0,  0, "TEST MODE 26GHz");
    display.drawString(0, 16, "Relay:");
    display.drawString(40, 16, _names[_sel]);
    display.drawString(0, 32, "State:");
    display.drawString(40, 32, _states[_sel] ? "ON" : "OFF");
    display.drawString(0, 48, "Turn: Select");
    display.drawString(0, 56, "Press: Toggle");
    display.display();
}
