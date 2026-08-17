#pragma once

#include "Attenuator.h"

/* ============================================================
 *
 * Attenuator Type 141 dB 
 *
 * Pads: 1 dB, 20 dB, 2 dB, 4 dB, 40 dB, 20 dB, 10 dB, 40 db, 4 dB  →  max 141 dB in 1 dB steps
 *
 * * Bistabile Relais mit Set/Reset Logik: je 2 Pins pro Relais (ON / OFF)
 *
 *  v0.56 oe5rnl 20260607
 * 
 *  ============================================================ */

/* --- Relais 1: 1 dB --- */
#define GPIO_A_1DB_ON       2  // GPIO2 Pin4
#define GPIO_A_1DB_OFF      3  // GPIO3 Pin5

/* --- Relais 2: 20 dB (A)--- */
#define GPIO_A_20DB_ON_A    6  // GPIO16 Pin9    
#define GPIO_A_20DB_OFF_A   7  // GPIO7 Pin10    

/* --- Relais 3: 2 dB  --- */
#define GPIO_A_2DB_ON       8  // GPIO8  Pin11                                            
#define GPIO_A_2DB_OFF      9  // GPIO9  Pin12                             

/* --- Relais 4: 4 dB (A) --- */
#define GPIO_A_4DB_ON_A    10  // GPIO10 Pin14
#define GPIO_A_4DB_OFF_A   11  // GPIO11 Pin15                        

/* --- Relais 5: 40 dB (A) --- */
#define GPIO_A_40DB_ON_A   12  // GPIO12 Pin16                           
#define GPIO_A_40DB_OFF_A  13  // GPIO13 Pin17

/* --- Relais 6: 20 dB (B) --- */
#define GPIO_A_20DB_ON_B   14  // GPIO14 Pin19
#define GPIO_A_20B_OFF_B   15  // GPIO15 Pin20

/* --- Relais 7:--- 10 dB */
#define GPIO_A_10DB_ON     16  // GPIO16 Pin21
#define GPIO_A_10DB_OFF    17  // GPIO17 Pin22

/* --- Relais 8: 40 dB (B) --- */
#define GPIO_A_40DB_ON_B   18  // GPIO18 Pin24
#define GPIO_A_40DB_OFF_B  22  // GPIO22 Pin29

/* --- Relais 9:4  dB (B)--- */
#define GPIO_A_4DB_ON_B    27  // GPIO27 Pin32
#define  GPIO_A_4DB_OFF_B  28  // GPIO28 Pin34




class Att141dB : public Attenuator {
public:
    void        setup()                 override;
    void        apply(int32_t dv)       override;
    int32_t     max_db()                const override { return 141; }
    int32_t     step_db()               const override { return   1; }
    int         relay_count()           const override { return RELAY_COUNT; }
    const char* att_name()              const override { return "RS-141dB"; }
    int         relay_mode()  const override { return BRIDGE; }

private:
    static const int RELAY_COUNT = 9;

    struct RelayDef {
        const char* name;
        int         on_pin;
        int         off_pin;
    };
    static const RelayDef _relays[RELAY_COUNT];

    bool _states[RELAY_COUNT] = {};
};
