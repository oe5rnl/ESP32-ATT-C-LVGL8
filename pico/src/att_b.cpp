#include "att_b.h"
#include "SSD1306.h"
#include <Arduino.h>

extern SSD1306 display;

/* TODO: implement all methods once ATT TYPE B hardware is defined */

void AttB::setup()               { Serial.println("ATT B: setup – NOT IMPLEMENTED"); }
void AttB::apply(int32_t dv)     { (void)dv; Serial.println("ATT B: apply – NOT IMPLEMENTED"); }

void AttB::show_info()
{
    Serial.println("ATT B (not impl.)");
    display.clear();
    display.drawString(3, 10, "ATT TYPE B");
    display.drawString(3, 20, "NOT IMPLEMENTED");
    display.display();
}

void AttB::test_init()             { Serial.println("ATT B: test_init – NOT IMPLEMENTED"); }
void AttB::test_rotate(int dir)    { (void)dir; Serial.println("ATT B: test_rotate – NOT IMPLEMENTED"); }
void AttB::test_toggle()           { Serial.println("ATT B: test_toggle – NOT IMPLEMENTED"); }

void AttB::update_test_display()
{
    display.clear();
    display.drawString(0,  0, "ATT TYPE B");
    display.drawString(0, 16, "NOT IMPLEMENTED");
    display.display();
}
