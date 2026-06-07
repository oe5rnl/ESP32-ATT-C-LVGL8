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
    int     relay_mode()  const override { return BRIDGE; }
};
