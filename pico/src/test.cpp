#include <Arduino.h>
#include "SSD1306.h"
#include "test.h"

/* Reference to the display instance defined in main.cpp */
extern SSD1306 display;

/* Encoder pins (same as in main.cpp) */
#define ENCODER_SW  19
#define ENCODER_DT  20
#define ENCODER_CLK 21

/* -------------------------------------------------------
 * H-Bridge Startup Test Mode
 *
 * Aktivierung: Encoder-Taste beim Einschalten gedrückt halten.
 * Drehregler : H-Brücke 1–9 auswählen
 * Drücken    : Zustand wechseln  OFF → FWD → REV → OFF ...
 * Beenden    : Doppelklick (zurueck ins Menue)
 * ------------------------------------------------------- */
static const int HB_COUNT           = 9;
static const int hb_pin_a[HB_COUNT] = {  2,  6,  8, 10, 12, 14, 16, 18, 27 };
static const int hb_pin_b[HB_COUNT] = {  3,  7,  9, 11, 13, 15, 17, 22, 28 };

static void hb_draw(int sel, const int states[])
{
    static const char* const snames[] = { "Ein", "Aus" };
    char buf[24];
    display.clear();
    display.drawString(0,  0, "H-BRIDGE TEST");
    snprintf(buf, sizeof(buf), "Bridge: %d / %d", sel + 1, HB_COUNT);
    display.drawString(0, 16, buf);
    snprintf(buf, sizeof(buf), "Status  : %s", snames[states[sel]]);
    display.drawString(0, 32, buf);
    snprintf(buf, sizeof(buf), "A:GP%d  B:GP%d", hb_pin_a[sel], hb_pin_b[sel]);
    display.drawString(0, 48, buf);
    display.display();
}

void hbridge_startup_test()
{
    /* All H-bridge pins as output, LOW (safe state) */
    for(int i = 0; i < HB_COUNT; i++) {
        pinMode(hb_pin_a[i], OUTPUT); digitalWrite(hb_pin_a[i], LOW);
        pinMode(hb_pin_b[i], OUTPUT); digitalWrite(hb_pin_b[i], LOW);
    }

    int sel = 0;
    int states[HB_COUNT] = {};   /* 0=FWD, 1=REV */

    /* Taste vom Menü-Klick kann noch gedrückt sein – erst loslassen abwarten */
    while(digitalRead(ENCODER_SW) == LOW) { delay(10); }
    delay(50);

    hb_draw(sel, states);

    int last_clk = digitalRead(ENCODER_CLK);
    int last_sw  = HIGH;

    /* Doppelklick-Erkennung fuer Menue-Rueckkehr */
    const unsigned long DC_WINDOW = 400;
    bool          dc_pending     = false;
    unsigned long dc_last_release = 0;

    while(true) {
        unsigned long now = millis();

        /* Encoder rotation → select bridge */
        int clk = digitalRead(ENCODER_CLK);
        if(clk != last_clk && clk == LOW) {
            int dir = (digitalRead(ENCODER_DT) != clk) ? 1 : -1;
            sel = (sel + (dir > 0 ? 1 : HB_COUNT - 1)) % HB_COUNT;
            hb_draw(sel, states);
        }
        last_clk = clk;

        /* Tastendruck (LOW->) Doppelklick erkennen, sonst single bei Ablauf */
        int sw = digitalRead(ENCODER_SW);
        if(sw == LOW && last_sw == HIGH) {
            delay(20);  /* debounce */
            if(dc_pending && (now - dc_last_release) < DC_WINDOW) {
                /* Doppelklick → zurueck ins Menue */
                while(digitalRead(ENCODER_SW) == LOW) { delay(10); }
                delay(50);
                return;
            }
        }
        if(sw == HIGH && last_sw == LOW) {
            dc_pending      = true;
            dc_last_release = now;
        }
        if(dc_pending && (now - dc_last_release) >= DC_WINDOW) {
            /* Single-Klick → Puls in aktuelle Richtung */
            dc_pending = false;
            if(states[sel] == 0) {          /* FWD: A=HIGH, B=LOW */
                digitalWrite(hb_pin_a[sel], HIGH);
                delay(20);
                digitalWrite(hb_pin_a[sel], LOW);
            } else {                        /* REV: A=LOW, B=HIGH */
                digitalWrite(hb_pin_b[sel], HIGH);
                delay(20);
                digitalWrite(hb_pin_b[sel], LOW);
            }
            states[sel] = 1 - states[sel];
            hb_draw(sel, states);
        }
        last_sw = sw;

        delay(5);
    }
}

/* -------------------------------------------------------
 * Static GPIO Test (GPIOs 10–13)
 * ------------------------------------------------------- */
static const int SG_PINS[]          = { 10, 11, 12, 13 };
static const char* const SG_NAMES[] = { "10dB", "20dB", "40A", "40B" };
static const int SG_COUNT           = 4;

static void sg_draw(int sel, const bool states[])
{
    display.clear();
    display.drawString(0,   0, "TEST 26GHz");
    display.drawString(0,  16, "Relay: ");
    display.drawString(40, 16, SG_NAMES[sel]);
    display.drawString(0,  32, "State: ");
    display.drawString(40, 32, states[sel] ? "ON" : "OFF");
    display.drawString(0,  48, "Turn : Select");
    display.drawString(0,  56, "Press: Toggle");
    display.display();
}

void static_gpio_test()
{
    for(int i = 0; i < SG_COUNT; i++) {
        pinMode(SG_PINS[i], OUTPUT);
        digitalWrite(SG_PINS[i], LOW);
    }

    int  sel = 0;
    bool states[SG_COUNT] = {};

    /* Taste vom Menü-Klick kann noch gedrückt sein – erst loslassen abwarten */
    while(digitalRead(ENCODER_SW) == LOW) { delay(10); }
    delay(50);

    sg_draw(sel, states);

    int last_clk = digitalRead(ENCODER_CLK);
    int last_sw  = HIGH;

    /* Doppelklick-Erkennung fuer Menue-Rueckkehr */
    const unsigned long DC_WINDOW = 400;
    bool          dc_pending     = false;
    unsigned long dc_last_release = 0;

    while(true) {
        unsigned long now = millis();

        /* Drehregler → Relay wählen */
        int clk = digitalRead(ENCODER_CLK);
        if(clk != last_clk && clk == LOW) {
            int dir = (digitalRead(ENCODER_DT) != clk) ? 1 : -1;
            sel = (sel + (dir > 0 ? 1 : SG_COUNT - 1)) % SG_COUNT;
            sg_draw(sel, states);
        }
        last_clk = clk;

        /* Tastendruck → Doppelklick = Menue, Single = Toggle */
        int sw = digitalRead(ENCODER_SW);
        if(sw == LOW && last_sw == HIGH) {
            delay(20);  /* debounce */
            if(dc_pending && (now - dc_last_release) < DC_WINDOW) {
                /* Doppelklick → zurueck ins Menue */
                while(digitalRead(ENCODER_SW) == LOW) { delay(10); }
                delay(50);
                return;
            }
        }
        if(sw == HIGH && last_sw == LOW) {
            dc_pending      = true;
            dc_last_release = now;
        }
        if(dc_pending && (now - dc_last_release) >= DC_WINDOW) {
            /* Single-Klick → Zustand toggeln */
            dc_pending = false;
            states[sel] = !states[sel];
            digitalWrite(SG_PINS[sel], states[sel] ? HIGH : LOW);
            sg_draw(sel, states);
        }
        last_sw = sw;

        delay(5);
    }
}
