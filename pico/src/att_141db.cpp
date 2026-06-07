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

/* ---- public interface ---- */

void Att141dB::setup()
{
    for(int i = 0; i < RELAY_COUNT; i++) {
        pinMode(_relays[i].on_pin,  OUTPUT);
        pinMode(_relays[i].off_pin, OUTPUT);
        digitalWrite(_relays[i].on_pin,  LOW);
        digitalWrite(_relays[i].off_pin, LOW);
    }
    /* Bring all bistable relays to OFF state in parallel */
    for(int i = 0; i < RELAY_COUNT; i++) {
        digitalWrite(_relays[i].off_pin, HIGH);
        _states[i] = false;
    }
    delay(20);
    for(int i = 0; i < RELAY_COUNT; i++) {
        digitalWrite(_relays[i].off_pin, LOW);
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

    /* relay order must match _relays[]: 1, 20A, 2, 4A, 40A, 20B, 10, 40B, 4B */
    bool desired[RELAY_COUNT] = {a1, a20a, a2, a4a, a40a, a20b, a10, a40b, a4b};

    /* Set all needed pins HIGH in parallel, then pulse LOW together */
    bool any_change = false;
    for(int i = 0; i < RELAY_COUNT; i++) {
        if(desired[i] != _states[i]) {
            int pin = desired[i] ? _relays[i].on_pin : _relays[i].off_pin;
            digitalWrite(pin, HIGH);
            any_change = true;
        }
    }
    if(any_change) {
        delay(20);
        for(int i = 0; i < RELAY_COUNT; i++) {
            if(desired[i] != _states[i]) {
                int pin = desired[i] ? _relays[i].on_pin : _relays[i].off_pin;
                digitalWrite(pin, LOW);
                _states[i] = desired[i];
            }
        }
    }
}

