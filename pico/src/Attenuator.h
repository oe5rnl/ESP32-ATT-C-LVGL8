#pragma once

#include <stdint.h>

/* ============================================================
 * Abstract base class for all attenuator types.
 *
 * Each concrete type lives in its own module (att_26ghz, att_135db, etc.)
 * and implements every pure-virtual method.  main.cpp only ever touches
 * the Attenuator* pointer – no switch/case on hardware type needed there.
 * The one and only switch/case is in the factory create_attenuator().
 * ============================================================ */
class Attenuator {
public:
    virtual ~Attenuator() = default;

    /* Configure GPIO pins as outputs and bring hardware to a safe initial state */
    virtual void    setup()              = 0;

    /* Apply attenuation: clamp/round to valid steps internally */
    virtual void    apply(int32_t dv)    = 0;

    /* Attenuator characteristics */
    virtual int32_t max_db()  const      = 0;
    virtual int32_t step_db() const      = 0;

    /* Show attenuator-specific info screen on SSD1306 at startup.
     * Responsible for clear() + draw + display() */
    virtual void    show_info()          = 0;

    /* Test mode – called once when test mode is activated */
    virtual void    test_init()          = 0;

    /* Test mode – encoder rotation: dir > 0 = next relay, dir < 0 = previous */
    virtual void    test_rotate(int dir) = 0;

    /* Test mode – button press: toggle the currently selected relay */
    virtual void    test_toggle()        = 0;

    /* Test mode – redraw display with current relay name/state */
    virtual void    update_test_display() = 0;
};

/* Factory: returns a pointer to the static instance for the given type constant
 * (see att_types.h).  Returns nullptr for unknown types.  Object is NOT heap-
 * allocated; the pointer is valid for the lifetime of the program. */
Attenuator* create_attenuator(int type);
