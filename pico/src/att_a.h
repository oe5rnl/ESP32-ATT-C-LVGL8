#pragma once

#include "Attenuator.h"

/* ============================================================
 * Attenuator Type A – Stub  (not yet implemented)
 * TODO: implement once hardware is known
 * ============================================================ */
class AttA : public Attenuator {
public:
    void    setup()               override;
    void    apply(int32_t dv)     override;
    int32_t max_db()  const       override { return 0; }
    int32_t step_db() const       override { return 1; }
    void    show_info()           override;
    void    test_init()           override;
    void    test_rotate(int dir)  override;
    void    test_toggle()         override;
    void    update_test_display() override;
};
