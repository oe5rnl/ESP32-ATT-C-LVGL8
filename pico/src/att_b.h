#pragma once

#include "Attenuator.h"

/* ============================================================
 * Attenuator Type B – Stub  (not yet implemented)
 * TODO: implement once hardware is known
 * ============================================================ */
class AttB : public Attenuator {
public:
    void    setup()               override;
    void    apply(int32_t dv)     override;
    int32_t     max_db()      const override { return   0; }
    int32_t     step_db()     const override { return   1; }
    int         relay_count() const override { return 0; }
    const char* att_name()    const override { return "ATT-B"; }
    int         digit_count() const override { return 2; }
    void    show_info()           override;
    void    test_init()           override;
    void    test_rotate(int dir)  override;
    void    test_toggle()         override;
    void    update_test_display() override;
    int     relay_mode()  const override { return BRIDGE; }
};
