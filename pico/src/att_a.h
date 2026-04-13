#pragma once

#include "Attenuator.h"

/* ============================================================
 * Attenuator Type A
 * Pads: 1 dB, 10 dB, 2×20 dB, 2×40 dB  →  max 131 dB in 1 dB steps
 * Bistabile Relais mit Set/Reset Logik: je 2 Pins pro Relais (ON / OFF)
 * ============================================================ */

/* --- Relais 1: 40 dB (A) --- */
#define GPIO_A_40DB_ON_A    12  // GPIO12 Pin16
#define GPIO_A_40DB_OFF_A   13  // GPIO13 Pin17

/* --- Relais 2: 20 dB (A) --- */
#define GPIO_A_20DB_ON_A    10  // GPIO10 Pin14
#define GPIO_A_20DB_OFF_A   11  // GPIO11 Pin15

/* --- Relais 3: 1 dB --- */
#define GPIO_A_1DB_ON        8  // GPIO8  Pin11
#define GPIO_A_1DB_OFF       9  // GPIO9  Pin12

/* --- Relais 4: 20 dB (B) --- */
#define GPIO_A_20DB_ON_B    14  // GPIO14 Pin19
#define GPIO_A_20DB_OFF_B   15  // GPIO15 Pin20

/* --- Relais 5: 10 dB --- */
#define GPIO_A_10DB_ON      16  // GPIO16 Pin21
#define GPIO_A_10DB_OFF     17  // GPIO17 Pin22

/* --- Relais 6: 40 dB (B) --- */
#define GPIO_A_40DB_ON_B    22  // GPIO22 Pin29
#define GPIO_A_40DB_OFF_B   26  // GPIO26 Pin31

/* --- Relais 7: RF ON/OFF --- */
#define GPIO_A_RF_ON        27  // GPIO27 Pin32
#define GPIO_A_RF_OFF       28  // GPIO28 Pin34

class AttA : public Attenuator {
public:
    void    setup()               override;
    void    apply(int32_t dv)     override;
    int32_t max_db()  const       override { return 131; }
    int32_t step_db() const       override { return   1; }
    void    show_info()           override;
    void    test_init()           override;
    void    test_rotate(int dir)  override;
    void    test_toggle()         override;
    void    update_test_display() override;

private:
    static const int RELAY_COUNT = 7;

    struct RelayDef {
        const char* name;
        int         on_pin;
        int         off_pin;
    };
    static const RelayDef _relays[7];

    int  _sel       = 0;
    bool _states[7] = {};

    static void pulse_pin(int pin);
    void        pulse(int idx, bool activate);
};
