#pragma once

#include "Attenuator.h"

/* ============================================================
 * R&S 26.5 GHz Attenuator
 * Pads: 10 dB, 20 dB, 40 dB (A), 40 dB (B)  →  max 110 dB in 10 dB steps
 * H-bridge logic: 1 bit per relay, HIGH = active
 * ============================================================ */

#define ATT_GPIO_10DB    10  // Pin 14
#define ATT_GPIO_20DB    11  // Pin 15
#define ATT_GPIO_40DB_A  12  // Pin 16
#define ATT_GPIO_40DB_B  13  // Pin 17

class Att26GHz : public Attenuator {
public:
    void    setup()               override;
    void    apply(int32_t dv)     override;
    int32_t     max_db()      const override { return 110; }
    int32_t     step_db()     const override { return  10; }
    int         relay_count() const override { return RELAY_COUNT; }
    const char* att_name()    const override { return "26.5 GHz Attenuator"; }
    int         digit_count() const override { return 2; }
    void    show_info()           override;
    void    test_init()           override;
    void    test_rotate(int dir)  override;
    void    test_toggle()         override;
    void    update_test_display() override;

private:
    static const int         RELAY_COUNT = 4;
    static const char* const _names[4];
    static const int         _gpios[4];

    int  _sel       = 0;
    bool _states[4] = {};
};
