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

    digitalWrite(ATT_GPIO_20DB,   a20  ? HIGH : LOW);
    digitalWrite(ATT_GPIO_40DB_A, a40a ? HIGH : LOW);
    digitalWrite(ATT_GPIO_40DB_B, a40b ? HIGH : LOW);

}

