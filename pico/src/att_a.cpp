#include "att_a.h"
#include "SSD1306.h"
#include <Arduino.h>

extern SSD1306 display;

const AttA::RelayDef AttA::_relays[] = {
    {"40A",  GPIO_A_40DB_ON_A,  GPIO_A_40DB_OFF_A },
    {"20A",  GPIO_A_20DB_ON_A,  GPIO_A_20DB_OFF_A },
    {"1dB",  GPIO_A_1DB_ON,     GPIO_A_1DB_OFF    },
    {"20B",  GPIO_A_20DB_ON_B,  GPIO_A_20DB_OFF_B },
    {"10dB", GPIO_A_10DB_ON,    GPIO_A_10DB_OFF   },
    {"40B",  GPIO_A_40DB_ON_B,  GPIO_A_40DB_OFF_B },
    {"RF",   GPIO_A_RF_ON,      GPIO_A_RF_OFF     },
};

/* ---- private helpers ---- */

void AttA::pulse_pin(int pin)
{
    digitalWrite(pin, HIGH);
    delay(20);
    digitalWrite(pin, LOW);
}

void AttA::pulse(int idx, bool activate)
{
    pulse_pin(activate ? _relays[idx].on_pin : _relays[idx].off_pin);
}

/* ---- public interface ---- */

void AttA::setup()
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

void AttA::apply(int32_t dv)
{
    int32_t att = (dv / step_db()) * step_db();
    if(att > max_db()) att = max_db();
    if(att < 0) att = 0;

    /* Greedy decomposition: 40A → 40B → 20A → 20B → 10 → 1 */
    int32_t rem = att;
    bool a40a = (rem >= 40); if(a40a) rem -= 40;
    bool a40b = (rem >= 40); if(a40b) rem -= 40;
    bool a20a = (rem >= 20); if(a20a) rem -= 20;
    bool a20b = (rem >= 20); if(a20b) rem -= 20;
    bool a10  = (rem >= 10); if(a10)  rem -= 10;
    bool a1   = (rem >= 1);
    bool rf   = (att > 0);

    /* relay order must match _relays[]: 40A,20A,1dB,20B,10dB,40B,RF */
    bool desired[RELAY_COUNT] = {a40a, a20a, a1, a20b, a10, a40b, rf};

    for(int i = 0; i < RELAY_COUNT; i++) {
        if(desired[i] != _states[i]) {
            pulse(i, desired[i]);
            _states[i] = desired[i];
        }
    }
}

void AttA::show_info()
{
    Serial.println("ATT TYPE A");
    display.clear();
    display.drawString(3, 10, "ATT TYPE A");
    display.drawString(3, 20, "Att: 131dB");
    display.drawString(3, 30, "Steps:  1dB");
    display.display();
}

void AttA::test_init()
{
    _sel = 0;
    for(int i = 0; i < RELAY_COUNT; i++) {
        _states[i] = false;
        pulse(i, false);
    }
}

void AttA::test_rotate(int dir)
{
    _sel = (_sel + (dir > 0 ? 1 : RELAY_COUNT - 1)) % RELAY_COUNT;
    Serial.print("Test ATT A: Relay -> ");
    Serial.println(_relays[_sel].name);
    update_test_display();
}

void AttA::test_toggle()
{
    _states[_sel] = !_states[_sel];
    Serial.print("Test ATT A: Toggle ");
    Serial.print(_relays[_sel].name);
    Serial.print(" -> ");
    Serial.println(_states[_sel] ? "ON" : "OFF");
    pulse(_sel, _states[_sel]);
    update_test_display();
}

void AttA::update_test_display()
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
