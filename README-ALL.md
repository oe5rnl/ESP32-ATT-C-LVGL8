# Attenuator Controller

Das Projekt realisiert einen Controller für RF-Attenuators (HF-Dämpfungsglieder).


Es besteht aus einer Basisplatine und den optionalen Bendienelementen:  
* Ein ESP32 CYD Touch-Display  
* Ein 2.8" OLED Display  
* Einen Drehgeber mit Druckschalter  

Der Betrieb kann in folgenden Varianten erfolgen. Basisplatine und:  
* Nur ESP Touch Display
* OLED Display und Drehgeber
* ESP Touch Display und oder/und OLED sowie oder/und Drehgeber 

---

## Systemübersicht

ESP32 CYD (Touch-Display 320×240, WebGUI via WiFi, LVGL 8 UI) kommuniziert über
UART (115200 Baud) mit dem Raspberry Pi Pico (Drehgeber, SSD1306 OLED). Beide
steuern gemeinsam die Attenuator-Relais.

### ESP32 — Touch-Controller

LVGL-8-Steuerungsoberfläche auf ESP32 CYD (Cheap Yellow Display):

- 2.8" Touch-Display ILI9341 (320×240), Variante CYD2USB mit ST7789
- WLAN: AP-Modus und Client-Modus gleichzeitig
- WebGUI mit identischer Funktion wie das Touch-Display
- NVS-Persistenz (Wert, Presets, WiFi-Modus, Set-Modus)
- FreeRTOS Dual-Core: LVGL/UI auf Core 1, WebSocket/WiFi auf Core 0
- Kommunikation mit dem Pico über UART0

### Pico — Relais-/Drehgeber-Controller

Raspberry Pi Pico als Attenuator-Controller:

- Drehgeber zur Auswahl und Aktivierung von Dämpfungswerten
- Optionales SSD1306 OLED-Display (I2C)
- Automatische Attenuator-Erkennung über ADC (Spannungsteiler an GP26 / ADC0)
- Bis zu 8 bistabile Relais (H-Brücke oder Einzel-Pin)
- Serielle Steuerung über UART vom ESP32

### Verdrahtung auf der Platine

<img src="docs/verdrahtung1.svg" alt="Verdrahtung auf der Platine" width="910">

---

## Unterstützte Attenuatoren

Quelle: [pico/src/Attenuator.cpp](pico/src/Attenuator.cpp),
[pico/src/att_types.h](pico/src/att_types.h) und die jeweiligen `att_*`-Klassen.

| Typ | Max. dB | Schritte | Relay-Modus | Status im Code |
|-----|--------:|---------:|-------------|----------------|
| R&S 26.5 GHz | 110 dB | 10 dB | Static (Einzelpin) | implementiert (`Att26GHz`) |
| R&S RS-135 dB | 135 dB | 5 dB | Bridge (H-Brücke) | implementiert (`Att135dB`) |
| RS-141 dB | (variable Pads) | — | Bridge (H-Brücke) | implementiert (`Att141dB`) |
| Typ B | — | — | — | **nicht** implementiert (`AttB` → „NOT IMPLEMENTED") |

### Automatische Erkennung über ADC (GP26 / ADC0)

**Maßgeblich ist der Code** in [pico/src/main.cpp](pico/src/main.cpp) (`getAttenuator()`):

| Spannung (v) | Erkannter Attenuator |
|--------------|----------------------|
| 0.0 V ≤ v < 0.7 V | RS-141 dB |
| 0.9 V ≤ v < 1.5 V | 26.5 GHz |
| 1.7 V ≤ v < 2.3 V | Typ B |
| sonst | RS-135 dB |

> **Hinweis:** Die alten READMEs und der Kommentar in
> [pico/src/att_types.h](pico/src/att_types.h) nennen eine andere Zuordnung
> (0.0–0.8 V → 26.5 GHz usw.). Diese stimmt **nicht** mit dem aktuellen Code
> überein. Zusätzlich enthält `getAttenuator()` eine fehlerhaft aussehende Zeile
> (`  ++       return ATTENUATOR_RS_135DB;`) — das sollte bei Gelegenheit im Code
> geprüft/korrigiert werden.

### Widerstände für den Spannungsteiler

R10 nach 3,3 V, R11 nach GND (an Pico ADC):

```
 3,3V
  |
 R10
  |---- Pico ADC (GP26)
  |
 R11
  |
 GND
```

| Attenuator | R10 | R11 |
|------------|-----|-----|
| 26.5 GHz | offen | offen |
| RS-141 dB | 3k3 | 4k7 |
| Typ B | 4k7 | 3k3 |
| RS-135 dB | 0 Ohm | offen |

> **ACHTUNG:** Niemals zwei 0-Ohm-Widerstände einbauen — das kurzschließt den
> 3,3-V-Ausgang des Pico.
>
> **ACHTUNG:** Vor dem Verbinden des Attenuators mit der Steuerplatine am Display
> prüfen, ob der gewünschte Attenuator angezeigt wird. Wird `ATTENUATOR_26_5GHz`
> eingestellt, aber ein Attenuator mit H-Brücken-Ansteuerung verwendet, kann es
> zur Zerstörung der Relais kommen.

---

## Hardware

### Verbindung ESP32 ↔ Pico (3,3 V UART)

| Signal | ESP32 (P3) | Pico |
|--------|-----------|------|
| GND | Schwarz | GND Pin 3 |
| TX ESP32 → | Gelb | RX GP1 Pin 2 |
| RX ESP32 ← | Blau | TX GP0 Pin 1 |

### ESP32 — wichtige GPIOs

Quelle: [esp32/src/main.cpp](esp32/src/main.cpp) (`Serial.begin(115200)` = UART0).

| Funktion | GPIO |
|----------|------|
| UART TX → Pico | GPIO 1 (UART0 TX) |
| UART RX ← Pico | GPIO 3 (UART0 RX) |
| Display SPI | über TFT_eSPI konfiguriert |
| Touch (XPT2046) | separater VSPI-Bus |

- **Board:** ESP32-2432S028 (Cheap Yellow Display / CYD)
- **Display:** ILI9341, 320×240, RGB565, SPI (Variante CYD2USB: ST7789, Environment `cyd2usb`)
- **Touch:** XPT2046 auf separatem VSPI-Bus

> **Korrektur:** Die alte [esp32/README.md](esp32/README.md) nennt UART-Pins
> GPIO21/22 und dedizierte ESP32-Attenuator-GPIOs (4/16/17/35, active LOW). Beides
> ist im aktuellen Code **nicht** vorhanden — der ESP32 nutzt UART0 (GPIO1/3) und
> die Relais werden vom Pico geschaltet.

### Pico — GPIO-Belegung

Quelle: [pico/README.md](pico/README.md) sowie die `att_*.h`-Header.

| Pin | GPIO | Funktion |
|-----|------|----------|
| 1 | GP0 | TX → ESP32 (Serial1) |
| 2 | GP1 | RX ← ESP32 (Serial1) |
| 4 | GP2 | Mode-Select 0 / Relais |
| 5 | GP3 | Mode-Select 1 / Relais |
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

#### Relais-Mapping RS-135 dB (bistabil, ON/OFF-Pin-Paare)

Quelle: [pico/src/att_135db.h](pico/src/att_135db.h) / [.cpp](pico/src/att_135db.cpp).
Header-Beschriftungen sind teils irreführend; die Pad-Werte wurden per Messung ermittelt.

| Relais | ON-Pin | OFF-Pin | tatsächlicher Pad-Wert |
|--------|--------|---------|------------------------|
| U1 (40A) | GP3 | GP2 | 40 dB |
| U2 (20A) | GP7 | GP6 | 20 dB |
| U3 (5 dB) | GP9 | GP8 | 5 dB |
| U4 (20B) | GP11 | GP10 | 20 dB |
| U5 (10 dB) | GP13 | GP12 | 10 dB |
| U6 (40B) | GP15 | GP14 | 40 dB |
| U7 (RF) | GP17 | GP16 | RF ON/OFF |

#### Relais-Mapping 26.5 GHz (Static, HIGH = aktiv)

Quelle: [pico/src/att_26ghz.h](pico/src/att_26ghz.h).

| Pad | GPIO | Pin |
|-----|------|-----|
| 10 dB | GP10 | 14 |
| 20 dB | GP11 | 15 |
| 40 dB (A) | GP12 | 16 |
| 40 dB (B) | GP13 | 17 |

### Display am Pico (I2C, SSD1306)

| Signal | Pico |
|--------|------|
| GND | GND Pin 18 |
| 3V3 | 3V3 Pin 36 |
| SDA | GP4 Pin 6 |
| SCL | GP5 Pin 7 |

---

## ESP32 GUI-Funktionen

Quelle: [esp32/src/main.cpp](esp32/src/main.cpp), [esp32/src/webserver.h](esp32/src/webserver.h).

- **3-Tab-Menü** (Main, Defaults, Config) mit Tab-Leiste am unteren Rand.
- **Main-Tab:** 3-stellige Ziffernanzeige (72 px Custom Font), einzeln per Touch
  anwählbar mit Unterstrich-Cursor. UP/DOWN/Set ändern die ausgewählte Stelle.
- **Defaults-Tab:** vordefinierte dB-Werte als Buttons. Kurzer Klick übernimmt den
  Wert, langer Klick öffnet ein Nummernfeld zum Editieren.
- **Config-Tab:** Autoenter-Switch (ein/aus), WLAN-Modus.
- **Ziffernbegrenzung** über `#define` konfigurierbar:
  ```cpp
  #define DIGIT_MAX_VAL  110   /* maximaler Gesamtwert in dB */
  #define DIGIT_MAX_0      1   /* Hunderter: 0 .. DIGIT_MAX_0 */
  #define DIGIT_MAX_1      9   /* Zehner:    0 .. DIGIT_MAX_1 */
  #define DIGIT_MAX_2      0   /* Einer:     0 = nicht wählbar (10er-Schritte) */
  ```
- **Default-Werte** (9 Buttons, `DEFAULT_BUTTON_COUNT = 9`):
  ```cpp
  int32_t default_values[DEFAULT_BUTTON_COUNT] = {20, 40, 60, 10, 30, 50, 70, 80, 90};
  ```
- **Persistenz:** Ziffernwert, Default-Button-Werte, Autoenter und WiFi-Modus
  werden im ESP32-NVS gespeichert und überleben Stromausfall/Neustart.
- **Set-Button:** wird nur angezeigt, wenn Autoenter ausgeschaltet ist.

> **Korrektur:** Die alte [esp32/README.md](esp32/README.md) nennt „6 vordefinierte
> dB-Werte" mit `{20, 40, 60, 10, 30, 50}`. Der Code verwendet inzwischen 9 Werte.

---

## WiFi-Konfiguration

### WiFi-Credentials einstellen

Erstelle `esp32/src/wifi_credentials.h` mit deinen WLAN-Zugangsdaten (Vorlage:
[esp32/src/wifi_credentials.h.example](esp32/src/wifi_credentials.h.example)):

```cpp
#pragma once
#define WIFI_SSID     "DeinSSID"
#define WIFI_PASSWORD "DeinPasswort"
```

Diese Datei ist in [.gitignore](.gitignore) und wird nicht committet.

### WiFi-Modi

Quelle: [esp32/src/webserver.h](esp32/src/webserver.h).

- **Modus 0 (Aus):** WiFi deaktiviert.
- **Modus 2 (AP + Client):** Access Point und Client gleichzeitig.
  - AP-SSID: `ESP32-ATT`
  - AP-Passwort: `12345678` (Standardwert — bitte ändern)
  - AP-IP: `192.168.4.1`
  - Client verbindet sich mit deinem WLAN; Zugangsdaten aus Config-Tab oder
    `wifi_credentials.h`
  - mDNS: `esp32-att.local`

### WebGUI

Nach dem Start erreichbar unter:
- **AP-Modus:** `http://192.168.4.1`
- **Client-Modus:** `http://esp32-att.local` (oder die angezeigte IP)

Funktionen: Dämpfungswert einstellen (UP/DOWN oder direkte Eingabe),
Default-Werte verwalten, Auto-Set ein/aus, WLAN-Netzwerke scannen und verbinden.

---

## Quick Start

### ESP32 flashen
```bash
cd esp32
./upload.sh
# oder manuell:
~/.platformio/penv/bin/pio run -t upload
~/.platformio/penv/bin/pio device monitor
# CYD2USB-Variante (ST7789):
~/.platformio/penv/bin/pio run -e cyd2usb -t upload
```

### Pico flashen
```bash
cd pico
./upload_main.sh
# oder manuell:
~/.platformio/penv/bin/pio run -t upload
~/.platformio/penv/bin/pio device monitor
```

> **Tipp:** Falls `pio: command not found` erscheint, den vollen Pfad
> `~/.platformio/penv/bin/pio` verwenden oder PlatformIO global installieren.

**Voraussetzungen:** [PlatformIO](https://platformio.org/) (CLI oder VS-Code-Extension).

---

## Serielles Kommunikationsprotokoll (UART, 115200 Baud)

Alle Nachrichten sind ASCII-Zeilen, abgeschlossen mit `\n`.

### Pico → ESP32

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

### ESP32 → Pico

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

## Implementierungsdetails ESP32

| Core | Aufgaben |
|------|----------|
| **Core 0** | WiFi-Stack, AsyncTCP, ESPAsyncWebServer, WebSocket-Callbacks |
| **Core 1** | `setup()` / `loop()`, LVGL `lv_timer_handler()`, UART-Empfang |

Kommunikation zwischen den Cores über FreeRTOS-Queue (`xQueueSend` / `xQueueReceive`).

---

## Projektstruktur

```
ESP32-ATT-C-LVGL8/
├── shared/
│   └── version.h               # Versionsnummern ESP32 + Pico (0.51)
├── esp32/
│   ├── platformio.ini          # PlatformIO + TFT_eSPI Build-Flags
│   ├── lv_conf.h               # LVGL-8-Konfiguration
│   └── src/
│       ├── main.cpp            # LVGL UI, UART-Kommunikation
│       ├── webserver.h         # AsyncWebServer, WebSocket, HTML
│       ├── wifi_credentials.h  # WLAN-Zugangsdaten (nicht im Repo)
│       └── lv_font_digits_72.c # Custom 72 px Ziffern-Font
├── pico/
│   └── src/
│       ├── main.cpp            # Hauptprogramm, UART, Encoder, ADC-Erkennung
│       ├── Attenuator.h/.cpp   # Abstrakte Basisklasse + Factory
│       ├── att_26ghz.h/.cpp    # R&S 26.5 GHz (Static, 110 dB / 10 dB)
│       ├── att_135db.h/.cpp    # R&S 135 dB (Bridge, 135 dB / 5 dB)
│       ├── att_141db.h/.cpp    # RS-141 dB (Bridge)
│       ├── att_b.h/.cpp        # Typ B (nicht implementiert)
│       ├── att_types.h         # Typ-Konstanten + ADC-Grenzwerte (Kommentar veraltet)
│       ├── SSD1306.h/.cpp      # OLED-Display-Treiber
│       └── big_digits.h        # Große Ziffern für OLED
├── Schaltung/                  # KiCad Schaltplan + PCB
└── docs/                       # Zusätzliche Dokumentation
```

---

## Korrekturen gegenüber den alten READMEs

Beim Zusammenführen wurden folgende Abweichungen zwischen README-Text und
Quellcode festgestellt und in dieser konsolidierten Fassung korrigiert:

| Thema | Alter README-Text | Code-Realität |
|-------|-------------------|---------------|
| ESP32-UART-Pins | GPIO21 TX / GPIO22 RX ([esp32/README.md](esp32/README.md)) | `Serial` = UART0, GPIO1 TX / GPIO3 RX |
| ESP32-Attenuator-GPIOs | GPIO 4/16/17/35, active LOW | im aktuellen Code nicht vorhanden (Relais steuert der Pico) |
| ADC-Erkennung | 0.0–0.8→26.5G, 0.8–1.6→141, 1.6–2.4→B, ≥2.4→135 | 0.0–0.7→141, 0.9–1.5→26.5G, 1.7–2.3→B, sonst→135 |
| RS-141 dB | „nicht implementiert" | `Att141dB` ist implementiert |
| Typ B | „nicht implementiert" | korrekt: `AttB` nicht implementiert |
| Default-Buttons | 6 Werte `{20,40,60,10,30,50}` | 9 Werte `{20,40,60,10,30,50,70,80,90}`, `DEFAULT_BUTTON_COUNT=9` |
| Version | 0.5 | 0.51 (siehe [shared/version.h](shared/version.h)) |

Offene Punkte im Code (nicht in dieser README behoben):
- `getAttenuator()` in [pico/src/main.cpp](pico/src/main.cpp) enthält eine
  fehlerhaft aussehende Zeile `++ return ATTENUATOR_RS_135DB;`.
- Der ADC-Grenzwert-Kommentar in [pico/src/att_types.h](pico/src/att_types.h)
  weicht vom tatsächlichen Erkennungscode ab.

---

## Lizenz

MIT — siehe [LICENSE](LICENSE).

Ausnahme: Der Ordner [esp32/docu/esp32-display/](esp32/docu/esp32-display/)
enthält Hersteller-Dokumentation (Sunton) und ist **nicht** von der MIT-Lizenz
abgedeckt.

## Basiert auf

[ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)
— LVGL8-Beispiel.
