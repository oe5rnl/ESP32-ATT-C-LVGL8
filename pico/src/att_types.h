#pragma once

/* Attenuator Type Constants
 * Determined by hardware select pins MODE_SELECT0 (GPIO6) / MODE_SELECT1 (GPIO7)
 *
 *  SELECT0  SELECT1  Type
 *   LOW      LOW     ATTENUATOR_RS_135DB
 *   LOW      HIGH    ATTENUATOR_A  (not implemented)
 *   HIGH     LOW     ATTENUATOR_B  (not implemented)
 *   HIGH     HIGH    ATTENUATOR_26_5GHz
 */

#define MODE_SELECT0     6   // GPIO6 Pin9
#define MODE_SELECT1     7   // GPIO7 Pin10

#define ATTENUATOR_RS_135DB  0   // R&S 135 dB, bistabile Relais, 5 dB Schritte
#define ATTENUATOR_A         1   // Unbekannt / nicht implementiert
#define ATTENUATOR_B         2   // Unbekannt / nicht implementiert
#define ATTENUATOR_26_5GHz   3   // R&S 26.5 GHz, 110 dB, 10 dB Schritte

/* Returns the current attenuator type based on MODE_SELECT pin states.
 * Declared here, defined in main.cpp. */
int getAttenuator();
