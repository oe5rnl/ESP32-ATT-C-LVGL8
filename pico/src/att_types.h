#pragma once

/* Attenuator Type Constants
 * Determined by voltage on ADC0 (GPIO26 / A0)
 *
 *  U_ADC            Type
 *  0.0 V .. 0.8 V   ATTENUATOR_26_5GHz
 *  0.8 V .. 1.6 V   ATTENUATOR_A
 *  1.6 V .. 2.4 V   ATTENUATOR_B
 *  2.4 V .. 3.2 V   ATTENUATOR_RS_135DB
 */

#define ADC_SELECT_PIN   A0  // GPIO26

#define ATTENUATOR_RS_135DB  0   // R&S 135 dB, bistabile Relais, 5 dB Schritte
#define ATTENUATOR_A         1   // Unbekannt / nicht implementiert
#define ATTENUATOR_B         2   // Unbekannt / nicht implementiert
#define ATTENUATOR_26_5GHz   3   // R&S 26.5 GHz, 110 dB, 10 dB Schritte

/* Returns the current attenuator type based on ADC0 voltage.
 * Declared here, defined in main.cpp. */
int getAttenuator();
