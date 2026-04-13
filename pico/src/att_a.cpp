#include "att_a.h"
#include "SSD1306.h"
#include <Arduino.h>

extern SSD1306 display;

/* TODO: implement all methods once ATT TYPE A hardware is defined */

void AttA::setup()               { Serial.println("ATT A: setup – NOT IMPLEMENTED"); }
void AttA::apply(int32_t dv)     { (void)dv; Serial.println("ATT A: apply – NOT IMPLEMENTED"); }

void AttA::show_info()
{
    Serial.println("ATT A (not impl.)");
    display.clear();
    display.drawString(3, 10, "ATT TYPE A");
    display.drawString(3, 20, "NOT IMPLEMENTED");
    display.display();
}

void AttA::test_init()             { Serial.println("ATT A: test_init – NOT IMPLEMENTED"); }
void AttA::test_rotate(int dir)    { (void)dir; Serial.println("ATT A: test_rotate – NOT IMPLEMENTED"); }
void AttA::test_toggle()           { Serial.println("ATT A: test_toggle – NOT IMPLEMENTED"); }

void AttA::update_test_display()
{
    display.clear();
    display.drawString(0,  0, "ATT TYPE A");
    display.drawString(0, 16, "NOT IMPLEMENTED");
    display.display();
}
