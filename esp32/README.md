# ESP32 - 26.5 GHz Attenuator Controller

LVGL 8 Steuerungsoberfläche für ESP32 CYD (Cheap Yellow Display) mit Touch-Display und WebGUI.

## Features

- **3-Tab-Menü** (Main, Defaults, Config) mit Tab-Leiste am unteren Rand
- **Main-Tab:** 3-stellige Ziffernanzeige (72px Custom Font), einzeln anwählbar per Touch mit Unterstrich-Cursor. UP/Down/Set-Buttons ändern die ausgewählte Stelle
- **Defaults-Tab:** 6 vordefinierte dB-Werte als Buttons (2 Reihen × 3). Kurzer Klick übernimmt den Wert, langer Klick öffnet ein Nummernfeld zum Editieren
- **Config-Tab:** Autoenter-Switch (ein/aus), WLAN-Modus (Aus / AP / Client)
- **Ziffernbegrenzung:** Maximalwert und aktivierbare Stellen über `#define` konfigurierbar (`DIGIT_MAX_VAL`, `DIGIT_MAX_0/1/2`)
- **Persistente Speicherung:** Ziffernwert, Default-Button-Werte, Autoenter und WiFi-Modus werden im ESP32 NVS gespeichert und überleben Stromausfall/Neustart
- **Webinterface:** Steuerung über Browser (WiFi AP oder Client-Modus)
- **Set-Button:** Wird nur angezeigt wenn Autoenter ausgeschaltet ist

## Hardware

- **Board:** ESP32-2432S028 (Cheap Yellow Display / CYD)
- **Display:** ILI9341, 320×240, RGB565, SPI
- **Touch:** XPT2046 auf separatem VSPI-Bus
- **Variante CYD2USB:** ST7789-Treiber (Environment `cyd2usb`)
- **Attenuator GPIOs** (active LOW):
  - GPIO 4: 10 dB Pad
  - GPIO 16: 20 dB Pad
  - GPIO 17: 40 dB Pad A
  - GPIO 35: 40 dB Pad B
- **UART für Pico** (am P3 Stecker):
  - GPIO 21: TX → Pico GP1 (Serial1 RX)
  - GPIO 22: RX ← Pico GP0 (Serial1 TX)

## Voraussetzungen

- [PlatformIO](https://platformio.org/) (CLI oder VS Code Extension)
- ESP32-2432S028 Board (CYD)

## Build & Flash

```bash
# Standard CYD (ILI9341)
~/.platformio/penv/bin/pio run -t upload

# CYD2USB Variante (ST7789)
~/.platformio/penv/bin/pio run -e cyd2usb -t upload

# Seriellen Monitor öffnen
~/.platformio/penv/bin/pio device monitor
```

> **Tipp:** Falls `pio: command not found` erscheint, verwende den vollen Pfad `~/.platformio/penv/bin/pio` oder installiere PlatformIO global.

## WiFi-Konfiguration

### WiFi-Credentials einstellen

Erstelle `src/wifi_credentials.h` mit deinen WLAN-Zugangsdaten:

```cpp
#pragma once
#define WIFI_SSID "DeinSSID"
#define WIFI_PASSWORD "DeinPasswort"
```

Diese Datei ist in `.gitignore` und wird nicht committed.

### WiFi-Modi

- **Modus 0 (Aus):** WiFi deaktiviert
- **Modus 1 (AP):** Access Point Modus
  - SSID: `ESP32-ATT`
  - Passwort: `12345678`
  - IP: `192.168.4.1`
- **Modus 2 (Client):** Verbindet sich mit deinem WLAN
  - Läuft parallel zum AP für durchgehende Erreichbarkeit
  - Verwendete Zugangsdaten: Aus Config-Tab oder `wifi_credentials.h`
  - mDNS: `esp32-att.local`

## WebGUI

Nach dem Start ist das WebInterface erreichbar unter:
- **AP-Modus:** http://192.168.4.1
- **Client-Modus:** http://esp32-att.local (oder die angezeigte IP)

Das WebGUI bietet die gleichen Funktionen wie das Touch-Display:
- Dämpfungswert einstellen (UP/DOWN oder direkte Eingabe)
- Default-Werte verwalten
- Auto-Set ein/aus
- WLAN-Netzwerke scannen und verbinden (im AP-Modus)

## Projektstruktur

```
esp32/
├── platformio.ini          # PlatformIO Konfiguration mit TFT_eSPI Build-Flags
├── lv_conf.h               # LVGL 8 Konfiguration (Montserrat 24/48 aktiviert)
├── src/
│   ├── main.cpp            # Hauptprogramm mit LVGL GUI
│   ├── webserver.h         # AsyncWebServer + WebSocket
│   ├── wifi_credentials.h  # WiFi-Zugangsdaten (git-ignored)
│   └── lv_font_digits_72.c # Custom 72px Ziffern-Font
└── README.md               # Diese Datei
```

## Anpassungen

### Maximalwerte ändern

In `src/main.cpp`:

```cpp
#define DIGIT_MAX_VAL  110   /* maximaler Gesamtwert in dB             */
#define DIGIT_MAX_0      1   /* Hunderter: 0 .. DIGIT_MAX_0           */
#define DIGIT_MAX_1      9   /* Zehner:    0 .. DIGIT_MAX_1           */
#define DIGIT_MAX_2      0   /* Einer:     0 = nicht wählbar (10er-Schritte) */
```

### Default-Werte ändern

In `src/main.cpp`:

```cpp
int32_t default_values[6] = {20, 40, 60, 10, 30, 50};
```

## Lizenz

Siehe [LICENSE](../LICENSE) im Root-Verzeichnis.

## Credits

Basiert auf dem LVGL8 Beispiel von [ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display).
