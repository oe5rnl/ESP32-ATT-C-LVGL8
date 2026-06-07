#include "att_b.h"
#include "SSD1306.h"
#include <Arduino.h>

extern SSD1306 display;

/* TODO: implement all methods once ATT TYPE B hardware is defined */

void AttB::setup()               { Serial.println("ATT B: setup – NOT IMPLEMENTED"); }
void AttB::apply(int32_t dv)     { (void)dv; Serial.println("ATT B: apply – NOT IMPLEMENTED"); }

