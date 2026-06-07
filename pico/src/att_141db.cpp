#include "att_141db.h"
#include "SSD1306.h"
#include <Arduino.h>

extern SSD1306 display;

const Att141dB::RelayDef Att141dB::_relays[] = {

    {"1",   GPIO_A_1DB_ON,      GPIO_A_1DB_OFF},        // 1
    {"20A", GPIO_A_20DB_ON_A,   GPIO_A_20DB_OFF_A},     // 2
    {"2",   GPIO_A_2DB_ON,      GPIO_A_2DB_OFF},        // 3
    {"4A",  GPIO_A_4DB_ON_A,    GPIO_A_4DB_OFF_A},      // 4
    {"40A", GPIO_A_40DB_ON_A,   GPIO_A_40DB_OFF_A},     // 5
    {"20B", GPIO_A_20DB_ON_B,   GPIO_A_20B_OFF_B},      // 6
    {"10",  GPIO_A_10DB_ON,     GPIO_A_10DB_OFF},       // 7
    {"40B", GPIO_A_40DB_ON_B,   GPIO_A_40DB_OFF_B},     // 8
    {"4B",  GPIO_A_4DB_ON_B,    GPIO_A_4DB_OFF_B},      // 9

};

/* ---- private helpers ---- */

void Att141dB::pulse_pin(int pin)
{
    digitalWrite(pin, HIGH);
    delay(20);
    digitalWrite(pin, LOW);
}

void Att141dB::pulse(int idx, bool activate)
{
    pulse_pin(activate ? _relays[idx].on_pin : _relays[idx].off_pin);
}

/* ---- public interface ---- */

void Att141dB::setup()
{
    for(int i = 0; i < RELAY_COUNT; i++) {
        pinMode(_relays[i].on_pin,  OUTPUT);
        pinMode(_relays[i].off_pin, OUTPUT);
        digitalWrite(_relays[i].on_pin,  LOW);
        digitalWrite(_relays[i].off_pin, LOW);
    }
    /* Bring all bistable relays to a defined OFF state at startup */
    for(int i = 0; i < RELAY_COUNT; i++) {
        pulse(i, false);
        _states[i] = false;
    }
}

void Att141dB::apply(int32_t dv)
{
    int32_t att = (dv / step_db()) * step_db();
    if(att > max_db()) att = max_db();
    if(att < 0) att = 0;

    /* Greedy decomposition: a40A, a40B, a20A, a20B, a10, a4A, a4B, a2, a1 */
    int32_t rem = att;
    bool a40a = (rem >= 40); if(a40a) rem -= 40;
    bool a40b = (rem >= 40); if(a40b) rem -= 40;
    bool a20a = (rem >= 20); if(a20a) rem -= 20;
    bool a20b = (rem >= 20); if(a20b) rem -= 20;
    bool a10  = (rem >= 10); if(a10)  rem -= 10;
    bool a4a  = (rem >= 4);  if(a4a)  rem -= 4;
    bool a4b  = (rem >= 4);  if(a4b)  rem -= 4;
    bool a2   = (rem >= 2);  if(a2)   rem -= 2;
    bool a1   = (rem >= 1);

    /* relay order must match _relays[]: 4A,40A,10,20A,40B,4B,2,20B,1 */
    bool desired[RELAY_COUNT] = {a4a, a40a, a10, a20a, a40b, a4b, a2, a20b, a1};

    for(int i = 0; i < RELAY_COUNT; i++) {
        if(desired[i] != _states[i]) {
            pulse(i, desired[i]);
            _states[i] = desired[i];
        }
    }
}

void Att141dB::show_info()
{
    Serial.println("RS 141 dB");
    display.clear();
    display.drawString(3, 10, "RS 141 dB");
    display.drawString(3, 20, "Att: 141dB");
    display.drawString(3, 30, "Steps:  1dB");
    display.display();
}

void Att141dB::test_init()
{
    _sel = 0;
    for(int i = 0; i < RELAY_COUNT; i++) {
        _states[i] = false;
        pulse(i, false);
    }
}

void Att141dB::test_rotate(int dir)
{
    _sel = (_sel + (dir > 0 ? 1 : RELAY_COUNT - 1)) % RELAY_COUNT;
    Serial.print("Test ATT A: Relay -> ");
    Serial.println(_relays[_sel].name);
    update_test_display();
}

void Att141dB::test_toggle()
{
    _states[_sel] = !_states[_sel];
    Serial.print("Test ATT A: Toggle ");
    Serial.print(_relays[_sel].name);
    Serial.print(" -> ");
    Serial.println(_states[_sel] ? "ON" : "OFF");
    pulse(_sel, _states[_sel]);
    update_test_display();
}

void Att141dB::update_test_display()
{
    display.clear();
    display.drawString(0,  0, "TEST MODE ATT A");
    display.drawString(0, 16, "Relay:");
    display.drawString(40, 16, _relays[_sel].name);
    display.drawString(0, 32, "State:");
    display.drawString(40, 32, _states[_sel] ? "ON" : "OFF");
    display.drawString(0, 48, "Turn: Select");
    display.drawString(0, 56, "Press: Toggle");
    display.display();
}
