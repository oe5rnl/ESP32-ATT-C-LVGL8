#pragma once

/* H-Bridge Startup Test
 * Aktivierung: Encoder-Taste beim Einschalten gedrückt halten.
 * Drehregler : H-Brücke 1–9 auswählen
 * Drücken    : Zustand wechseln  OFF → FWD → REV → OFF ...
 * Beenden    : Doppelklick (zurueck ins Menue)
 */
void hbridge_startup_test();
void static_gpio_test();
