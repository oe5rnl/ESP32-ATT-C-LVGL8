#pragma once

#include <stdint.h>

#define BRIDGE 0
#define STATIC 1


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
    virtual int32_t     max_db()      const = 0;
    virtual int32_t     step_db()     const = 0;
    virtual int         relay_count() const = 0;
    virtual const char* att_name()    const = 0;

    /* Bitmask der aktiven Stellen, abgeleitet aus max_db()/step_db().
     * Bit 0 = Hunderter, Bit 1 = Zehner, Bit 2 = Einer.
     * Subklassen müssen das normalerweise NICHT überschreiben. */
    virtual uint8_t digit_mask() const {
        uint8_t m = 0;
        if(max_db()  >= 100) m |= 0x01;
        if(max_db()  >=  10) m |= 0x02;
        if(step_db() <   10) m |= 0x04;
        if(!m) m = 0x04;          /* mindestens Einer */
        return m;
    }

    /* Anzahl der aktiven Stellen (popcount(digit_mask())). */
    virtual int digit_count() const {
        uint8_t m = digit_mask();
        return (m & 1) + ((m >> 1) & 1) + ((m >> 2) & 1);
    }

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

    /* Relay driving mode: BRIDGE (H-bridge) or STATIC (direct GPIO) */
    virtual int     relay_mode()  const = 0;

    /* RF switch relay present (used to enable/disable RF path).
     * Default: false.  Override with true for types that have an RF switch. */
    virtual bool    rf_switch()   const { return false; }

    /* Current RF switch state (true = RF ON).  Only meaningful when rf_switch() == true. */
    virtual bool    get_rf()      const { return false; }

    /* Switch the RF relay.  No-op for types without an RF switch. */
    virtual void    set_rf(bool /*on*/) {}

    /* Safety watchdog – call from loop().
     * Default: no-op.  Implementations may use this to enforce
     * hardware constraints (e.g. max pulse width on relay coils). */
    virtual void    poll() {}
};

/* Factory: returns a pointer to the static instance for the given type constant
 * (see att_types.h).  Returns nullptr for unknown types.  Object is NOT heap-
 * allocated; the pointer is valid for the lifetime of the program. */
Attenuator* create_attenuator(int type);
