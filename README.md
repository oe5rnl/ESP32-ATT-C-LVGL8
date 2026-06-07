# Attenuator Controller — ESP32 + Pico

Steuerungssystem für HF-Attenuatoren.  

Es besteht  
  * aus einem HW-Treiber der die Attenautoren ansteuert und einem  
    optionalen 0.96" Display  sowie einem Rotary Encode zur Dateneingabe.
  * Einem 


 mit bistabilen Relais.
---

## Systemübersicht

ESP32 CYD (Touch Display 320x240, WebGUI via WiFi, LVGL 8 UI)
kommuniziert via UART 115200 Baud (TX GPIO1 / RX GPIO3) mit dem
Raspberry Pi Pico (Rotary Encoder, SSD1306 OLED).
Beide steuern gemeinsam die Attenuator-Relais.

### ESP32 — Touch Controller

LVGL 8 Steuerungsoberfläche auf ESP32 CYD (Cheap Yellow Display):

- 2.8" Touch-Display ILI9341 (320x240)
- WLAN: AP-Modus und Client-Modus gleichzeitig
- WebGUI mit identischer Funktion wie das Touch-Display
- NVS-Persistenz (Wert, Presets, WiFi-Modus, Set-Modus)
- FreeRTOS Dual-Core: LVGL/UI auf Core 1, WebSocket/WiFi auf Core 0
- Kommunikation mit dem Pico über UART

### Pico — Portextender / Drehgeber Controller

Raspberry Pi Pico als Attenuator-Controller:

- Drehgeber zur Auswahl und Aktivierung von Dämpfungswerten
- Optionales SSD1306 OLED-Display (I2C)
- Automatische Attenuator-Erkennung über ADC (Spannungsteiler an GP26)
- Bis zu 8 bistabile Relais (H-Brücke oder Einzel-Pin)
- Serielle Steuerung über UART vom ESP32

---

## Unterstützte Attenuatoren

| Typ | Max. dB | Schritte | Relay-Modus |
|-----|--------:|--------:|-------------|
| R&S 26.5 GHz | 110 dB | 10 dB | Static (Einzelpin) |
| R&S RS-135 dB | 135 dB | 5 dB | Bridge (H-Brücke) |
| RS-141 dB | — | — | (nicht implementiert) |
| Typ B | — | — | (nicht implementiert) |

Automatische Erkennung über Spannungsteiler an GP26 (ADC0):

| Spannung | Attenuator |
|----------|-----------|
| 0.0 – 0.8 V | 26.5 GHz |
| 0.8 – 1.6 V | RS-141 dB |
| 1.6 – 2.4 V | Typ B |
| >= 2.4 V | RS-135 dB |

Widerstände für den Spannungsteiler (R10 nach 3,3 V, R11 nach GND):

| Attenuator | R10 | R11 |
|-----------|-----|-----|
| 26.5 GHz | offen | offen |
| RS-141 dB | 3k3 | 4k7 |
| Typ B | 4k7 | 3k3 |
| RS-135 dB | 0 Ohm | offen |

> **Achtung:** Niemals zwei 0-Ohm-Widerstände einbauen — das kurzschließt den 3,3-V-Ausgang des Pico.

---

## Hardware

### Verbindung ESP32 <-> Pico (3,3 V UART)

| Signal | ESP32 (P3) | Pico |
|--------|-----------|------|
| GND | Schwarz | GND Pin 3 |
| TX ESP32 | Gelb | RX GP1 Pin 2 |
| RX ESP32 | Blau | TX GP0 Pin 1 |

### ESP32 — Wichtige GPIOs

| Funktion | GPIO |
|----------|------|
| UART TX -> Pico | GPIO 1 |
| UART RX <- Pico | GPIO 3 |
| Display SPI | TFT_eSPI konfiguriert |
| Touch VSPI | XPT2046 |

### Pico — GPIO-Belegung

| Pin | GPIO | Funktion |
|-----|------|----------|
| 1 | GP0 | TX -> ESP32 (Serial1) |
| 2 | GP1 | RX <- ESP32 (Serial1) |
| 4 | GP2 | Mode-Select 0 |
| 5 | GP3 | Mode-Select 1 |
| 6 | GP4 | I2C SDA (Display) |
| 7 | GP5 | I2C SCL (Display) |
| 9 | GP6 | Relais 1a |
| 10 | GP7 | Relais 1b |
| 11 | GP8 | Relais 2a |
| 12 | GP9 | Relais 2b |
| 14 | GP10 | Relais 3a |
| 15 | GP11 | Relais 3b |
| 16 | GP12 | Relais 4a |
| 17 | GP13 | Relais 4b |
| 19 | GP14 | Relais 5a |
| 20 | GP15 | Relais 5b |
| 21 | GP16 | Relais 6a |
| 22 | GP17 | Relais 7a |
| 24 | GP18 | Relais 7b |
| 25 | GP19 | Encoder CLK (A) |
| 26 | GP20 | Encoder DT (B) |
| 27 | GP21 | Encoder SW |
| 31 | GP26 | ADC0 — Attenuator-Erkennung |

---

## Quick Start

### ESP32 flashen
```bash
cd esp32
./upload.sh
# oder manuell:
~/.platformio/penv/bin/pio run -t upload
~/.platformio/penv/bin/pio device monitor
```

### Pico flashen
```bash
cd pico
./upload_main.sh
# oder manuell:
~/.platformio/penv/bin/pio run -t upload
~/.platformio/penv/bin/pio device monitor
```

---

## Serielles Kommunikationsprotokoll (UART, 115200 Baud)

Alle Nachrichten sind ASCII-Zeilen, abgeschlossen mit `\n`.

### Pico -> ESP32

**Startup-Info** (einmalig nach Pico-Reset):

| Befehl | Beschreibung |
|--------|--------------|
| `RELAYS:<n>` | Anzahl der Relais |
| `ATTNAME:<name>` | Attenuator-Name |
| `STEP:<n>` | Schrittweite in dB |
| `MAXDB:<n>` | Maximalwert in dB |
| `RELMODE:<n>` | Relay-Modus: `0`=Bridge, `1`=Static |
| `RFSWITCH:<n>` | RF-Schalter vorhanden: `0`/`1` |
| `PICOVER:<ver>` | Pico-Firmware-Version |

**Laufzeit:**

| Befehl | Beschreibung |
|--------|--------------|
| `<n>dB` | Aktueller Dämpfungswert (Display-Sync) |
| `SEL<n>` | Ausgewählte Stelle: `0`=Hunderter, `1`=Zehner, `2`=Einer |
| `SETMODE:<n>` | Set-Modus: `0`=Direct, `1`=Time, `2`=Button |
| `RF:<n>` | RF-Schalter-Zustand: `0`/`1` |
| `TESTSTATE:<i>,<s>` | Test: Relais-Index `i`, Zustand `s` (`0`/`1`) |
| `SETOK` | Pico hat im Set-Button-Modus angewendet |

### ESP32 -> Pico

| Befehl | Beschreibung |
|--------|--------------|
| `<n>dB` | Dämpfung Display-Sync (kein Schalten) |
| `SET:<n>dB` | Dämpfung sofort schalten (Relais) |
| `TIMED:<n>dB` | Zeitgesteuert schalten |
| `SEL<n>` | Aktive Stelle ändern |
| `SETMODE:<n>` | Set-Modus umschalten |
| `RF:<n>` | RF-Schalter setzen: `0`/`1` |
| `TEST:START` | Relay-Test starten |
| `TEST:SEL:<i>` | Test-Relais `i` auswählen |
| `TEST:ACTION` | Test: Puls/Toggle am gewählten Relais |
| `TEST:END` | Test beenden |

---

## Projektstruktur

```
ESP32-ATT-C-LVGL8/
├── shared/
│   └── version.h               # Versionsnummern ESP32 + Pico
├── esp32/
│   ├── platformio.ini
│   ├── lv_conf.h               # LVGL-Konfiguration
│   └── src/
│       ├── main.cpp            # LVGL UI, UART-Kommunikation
│       ├── webserver.h         # AsyncWebServer, WebSocket, HTML
│       ├── wifi_credentials.h  # WLAN-Zugangsdaten (nicht im Repo)
│       └── lv_font_digits_72.c # Custom 72px Ziffern-Font
├── pico/
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp            # Hauptprogramm, UART, Encoder
│       ├── Attenuator.h/.cpp   # Abstrakte Attenuator-Basisklasse
│       ├── att_26ghz.h/.cpp    # R&S 26.5 GHz Implementierung
│       ├── att_135db.h/.cpp    # R&S 135 dB Implementierung
│       ├── att_types.h         # Typ-Konstanten + ADC-Erkennung
│       ├── SSD1306.h/.cpp      # OLED-Display-Treiber
│       └── big_digits.h        # Grosse Ziffern fuer OLED
├── Schaltung/                  # KiCad Schaltplan + PCB
└── docs/                       # Zusaetzliche Dokumentation
```

---

## Implementierungsdetails ESP32

| Core | Aufgaben |
|------|---------|
| **Core 0** | WiFi-Stack, AsyncTCP, ESPAsyncWebServer, WebSocket-Callbacks |
| **Core 1** | `setup()` / `loop()`, LVGL `lv_timer_handler()`, UART-Empfang |

Kommunikation zwischen den Cores über FreeRTOS-Queue (`xQueueSend` / `xQueueReceive`).

---

## Lizenz

MIT — siehe [LICENSE](LICENSE).

## Basiert auf

[ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) — LVGL8 Beispiel
