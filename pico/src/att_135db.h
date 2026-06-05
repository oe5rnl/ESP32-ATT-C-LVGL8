#pragma once

#include "Attenuator.h"

/* ============================================================
 * R&S 135 dB Attenuator
 * Pads: 5 dB, 10 dB, 2×20 dB, 2×40 dB  →  max 135 dB in 5 dB steps
 * Bistabile Relais mit Set/Reset Logik: je 2 Pins pro Relais (ON / OFF)
 * ============================================================ */


/* --- U1 Relais 1: 40 dB (A) --- */
#define GPIO_135DB_40DB_ON_A     3      // GPIO3 Pin5
#define GPIO_135DB_40DB_OFF_A    2      // GPIO2 Pin4

/* --- U2 Relais 2: 20 dB (A) --- */
#define GPIO_135DB_20DB_ON_A     7      // GPIO7 Pin10
#define GPIO_135DB_20DB_OFF_A    6      // GPIO6 Pin9

/* --- U3 Relais 3:  5 dB --- */
#define GPIO_135DB_5DB_ON        9      // GPIO9 Pin12
#define GPIO_135DB_5DB_OFF       8      // GPIO8 Pin11

/* --- U4 Relais 4: 20 dB (dB) --- */
#define GPIO_135DB_20DB_ON_B    11     // GPIO11 Pin15
#define GPIO_135DB_20DB_OFF_B   10     // GPIO10 Pin14

/* --- U5 Relais 5:10 dB --- */
#define GPIO_135DB_10DB_ON      13      // GPIO13 Pin17
#define GPIO_135DB_10DB_OFF     12      // GPIO12 Pin16

/* --- U6 Relais 6:  40 dB (B) --- */
#define GPIO_135DB_40DB_ON_B    15      // GPIO15 Pin20
#define GPIO_135DB_40DB_OFF_B   14      // GPIO14 Pin19 

/* --- U7 Relais 7: RF ON/OFF --- */
#define GPIO_135DB_RF_ON        17      // GPIO17 Pin22
#define GPIO_135DB_RF_OFF       16      // GPIO16 Pin21

// /* --- U8 Relais 8: NC --- */
// #define GPIO_18                 18   // GPIO18 Pin24
// #define GPIO_22                 22   // GPIO19 Pin29

// /* --- UC Relais 9: NC --- */
// #define GPIO_27                 27   // GPIO27 Pin32
// #define GPIO_28                 28   // GPIO28 Pin34




class Att135dB : public Attenuator {
public:
    void    setup()               override;
    void    apply(int32_t dv)     override;
    int32_t     max_db()      const override { return 135; }
    int32_t     step_db()     const override { return   5; }
    int         relay_count() const override { return RELAY_COUNT; }
    const char* att_name()    const override { return "RS-135dB"; }
    int         digit_count() const override { return 3; }
    void    show_info()           override;
    void    test_init()           override;
    void    test_rotate(int dir)  override;
    void    test_toggle()         override;
    void    update_test_display() override;
    int     relay_mode()  const override { return BRIDGE; }
    bool    rf_switch()   const override { return true; }

    /* Safety watchdog: forces any relay pin LOW if it has been HIGH
     * for more than PIN_MAX_HIGH_MS milliseconds. Call from loop(). */
    void    poll()                override;

private:
    static const int              RELAY_COUNT      = 7;
    static const unsigned long    PIN_MAX_HIGH_MS  = 100;

    struct RelayDef {
        const char* name;
        int         on_pin;
        int         off_pin;
    };
    static const RelayDef _relays[7];

    int  _sel       = 0;
    bool _states[7] = {};

    /* Watchdog: timestamp (millis) when each pin was first seen HIGH.
     * Indexed [relay][0=off_pin, 1=on_pin]. 0 means "not currently HIGH". */
    unsigned long _pin_armed_ms[7][2] = {};

    static void pulse_pin(int pin);
    void        pulse(int idx, bool activate);
};

