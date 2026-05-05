#include "att_135db.h"
#include "SSD1306.h"
#include <Arduino.h>

extern SSD1306 display;

/* Mapping pad value → physical relay slot, ermittelt durch Messung:
 *   U1 (GPIO2/3)   = 5 dB
 *   U2 (GPIO6/7)   = 20 dB   (Header-Name irrefuehrend "10dB")
 *   U3 (GPIO8/9)   = 40 dB   (Header-Name irrefuehrend "20B")
 *   U4 (GPIO10/11) = 20 dB
 *   U5 (GPIO12/13) = 10 dB   (Header-Name irrefuehrend "40A")
 *   U6 (GPIO14/15) = 40 dB
 */
const Att135dB::RelayDef Att135dB::_relays[] = {
    {"40A",  GPIO_135DB_20DB_ON_B,  GPIO_135DB_20DB_OFF_B }, /* U3 = 40 dB */
    {"20A",  GPIO_135DB_20DB_ON_A,  GPIO_135DB_20DB_OFF_A }, /* U4 = 20 dB */
    {"5dB",  GPIO_135DB_5DB_ON,     GPIO_135DB_5DB_OFF    }, /* U1 = 5 dB  */
    {"20B",  GPIO_135DB_10DB_ON,    GPIO_135DB_10DB_OFF   }, /* U2 = 20 dB */
    {"10dB", GPIO_135DB_40DB_ON_A,  GPIO_135DB_40DB_OFF_A }, /* U5 = 10 dB */
    {"40B",  GPIO_135DB_40DB_ON_B,  GPIO_135DB_40DB_OFF_B }, /* U6 = 40 dB */
    {"RF",   GPIO_135DB_RF_ON,      GPIO_135DB_RF_OFF     }, /* U7 = RF    */
};

/* ---- private helpers ---- */

void Att135dB::pulse_pin(int pin)
{
    digitalWrite(pin, HIGH);
    delay(20);
    digitalWrite(pin, LOW);
}

void Att135dB::pulse(int idx, bool activate)
{
    pulse_pin(activate ? _relays[idx].on_pin : _relays[idx].off_pin);
}

/* ---- public interface ---- */

void Att135dB::setup()
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
    /* Relay 7 (RF) always ON – pulse ON pin */
    digitalWrite(_relays[6].on_pin, HIGH);
    delay(20);
    digitalWrite(_relays[6].on_pin, LOW);
    _states[6] = true;

}

void Att135dB::apply(int32_t dv)
{
    int32_t att = (dv / step_db()) * step_db();
    if(att > max_db()) att = max_db();
    if(att < 0) att = 0;

    /* Greedy decomposition: 40A → 40B → 20A → 20B → 10 → 5 */
    int32_t rem = att;
    bool a40a = (rem >= 40); if(a40a) rem -= 40;
    bool a40b = (rem >= 40); if(a40b) rem -= 40;
    bool a20a = (rem >= 20); if(a20a) rem -= 20;
    bool a20b = (rem >= 20); if(a20b) rem -= 20;
    bool a10  = (rem >= 10); if(a10)  rem -= 10;
    bool a5   = (rem >= 5);
    bool rf   = true;  /* Relay 7 (RF) always ON */

    /* relay order must match _relays[]: 40A,20A,5dB,20B,10dB,40B,RF */
    bool desired[RELAY_COUNT] = {a40a, a20a, a5, a20b, a10, a40b, rf};

    /* Set all needed pins HIGH in parallel */
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

    // Serial.print("ATT 135dB: ");
    // Serial.print(att);
    // Serial.print(" dB  [40A="); Serial.print(a40a);
    // Serial.print(" 20A=");      Serial.print(a20a);
    // Serial.print(" 5=");        Serial.print(a5);
    // Serial.print(" 20B=");      Serial.print(a20b);
    // Serial.print(" 10=");       Serial.print(a10);
    // Serial.print(" 40B=");      Serial.print(a40b);
    // Serial.print(" RF=");       Serial.print(rf);
    // Serial.println("]");
}

void Att135dB::show_info()
{
    Serial.println("R&S 135 dB");
    display.clear();
    display.drawString(3, 10, "RS-135 dB");
    display.drawString(3, 20, "Att: 135dB");
    display.drawString(3, 30, "Steps: 5dB");
    display.display();
}

void Att135dB::test_init()
{
    _sel = 0;
    for(int i = 0; i < RELAY_COUNT; i++) {
        _states[i] = false;
        pulse(i, false);
    }
}

void Att135dB::test_rotate(int dir)
{
    _sel = (_sel + (dir > 0 ? 1 : RELAY_COUNT - 1)) % RELAY_COUNT;
    Serial.print("Test 135dB: Relay -> ");
    Serial.println(_relays[_sel].name);
    update_test_display();
}

void Att135dB::test_toggle()
{
    _states[_sel] = !_states[_sel];
    Serial.print("Test 135dB: Toggle ");
    Serial.print(_relays[_sel].name);
    Serial.print(" -> ");
    Serial.println(_states[_sel] ? "ON" : "OFF");
    pulse(_sel, _states[_sel]);
    update_test_display();
}

void Att135dB::update_test_display()
{
    display.clear();
    display.drawString(0,  0, "TEST MODE 135dB");
    display.drawString(0, 16, "Relay:");
    display.drawString(40, 16, _relays[_sel].name);
    display.drawString(0, 32, "State:");
    display.drawString(40, 32, _states[_sel] ? "ON" : "OFF");
    display.drawString(0, 48, "Turn: Select");
    display.drawString(0, 56, "Press: Toggle");
    display.display();
}
