# ESP32-ATT-C-LVGL8

LVGL 8 Steuerungsoberfläche für einen 26.5 GHz Attenuator auf dem ESP32 CYD (Cheap Yellow Display).

## Features

- **3-Tab-Menü** (Main, Defaults, Config) mit Tab-Leiste am unteren Rand
- **Main-Tab:** 3-stellige Ziffernanzeige (72px Custom Font), einzeln anwählbar per Touch mit Unterstrich-Cursor. UP/Down/Set-Buttons ändern die ausgewählte Stelle
- **Defaults-Tab:** 6 vordefinierte dB-Werte als Buttons (2 Reihen × 3). Kurzer Klick übernimmt den Wert, langer Klick öffnet ein Nummernfeld zum Editieren
- **Config-Tab:** Autoenter-Switch (ein/aus)
- **Persistente Speicherung:** Ziffernwert, Default-Button-Werte und Autoenter-Einstellung werden im ESP32 NVS gespeichert und überleben Stromausfall/Neustart
- **Set-Button:** Wird nur angezeigt wenn Autoenter ausgeschaltet ist

## Hardware

- **Board:** ESP32-2432S028 (Cheap Yellow Display / CYD)
- **Display:** ILI9341, 320×240, RGB565, SPI
- **Touch:** XPT2046 auf separatem VSPI-Bus
- **Variante CYD2USB:** ST7789-Treiber (Environment `cyd2usb`)

## Voraussetzungen

- [PlatformIO](https://platformio.org/) (CLI oder VS Code Extension)

## Build & Flash

```bash
# Standard CYD (ILI9341)
$ ~/.platformio/penv/bin/pio run -t upload

# CYD2USB Variante (ST7789)
$ ~/.platformio/penv/bin/pio run -e cyd2usb -t upload

# Seriellen Monitor öffnen
$ ~/.platformio/penv/bin/pio device monitor
```

## Projektstruktur

```
ESP32-ATT-C-LVGL8/
├── platformio.ini          # PlatformIO Konfiguration mit TFT_eSPI Build-Flags
├── lv_conf.h               # LVGL 8 Konfiguration (Montserrat 24/48 aktiviert)
├── src/
│   ├── main.cpp            # Hauptprogramm: UI, Touch, Display, NVS-Speicherung
│   └── lv_font_digits_72.c # Custom Font: Montserrat-Medium 72px (Ziffern 0-9)
├── .gitignore
└── README.md
```

## Konfiguration

### Display & Touch

Die Pin-Belegung ist in `platformio.ini` als Build-Flags definiert:

| Funktion       | Pin |
|----------------|-----|
| TFT_MISO       | 12  |
| TFT_MOSI       | 13  |
| TFT_SCLK       | 14  |
| TFT_CS         | 15  |
| TFT_DC         | 2   |
| TFT_BL         | 21  |
| XPT2046_IRQ    | 36  |
| XPT2046_MOSI   | 32  |
| XPT2046_MISO   | 39  |
| XPT2046_CLK    | 25  |
| XPT2046_CS     | 33  |

### LVGL

Die LVGL-Konfiguration befindet sich in `lv_conf.h` im Projektstammverzeichnis. Wichtige Einstellungen:

- `LV_COLOR_DEPTH 16` (RGB565)
- `LV_COLOR_16_SWAP 0`
- `LV_MEM_SIZE (48U * 1024U)`
- `LV_FONT_MONTSERRAT_24 1` (für UI-Texte, Buttons, Menü)
- `LV_FONT_MONTSERRAT_48 1`

### Custom Font

Die 72px Ziffern-Font wurde generiert mit:

```bash
npx lv_font_conv --bpp 4 --size 72 --font Montserrat-Medium.ttf \
  --range 0x20,0x30-0x39 --no-compress --format lvgl \
  -o src/lv_font_digits_72.c
```

### Persistente Speicherung (NVS)

Folgende Werte werden im ESP32 Flash (Namespace `att`) gespeichert:

| Key    | Typ   | Beschreibung           |
|--------|-------|------------------------|
| `cval` | Int   | Aktueller Ziffernwert  |
| `def0`–`def5` | Int | Default-Button-Werte |
| `ae`   | Bool  | Autoenter ein/aus      |

### Touch-Kalibrierung

Die Touch-Kalibrierungswerte in `src/main.cpp` (Zeilen 32-33) müssen eventuell an das eigene Display angepasst werden:

```cpp
uint16_t touchScreenMinimumX = 200, touchScreenMaximumX = 3700;
uint16_t touchScreenMinimumY = 240, touchScreenMaximumY = 3800;
```

## Abhängigkeiten

Werden automatisch von PlatformIO heruntergeladen (`pio run`):

- [LVGL](https://github.com/lvgl/lvgl) v8.3.x
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) v2.5.x
- [XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) v1.4

Das `.pio`-Verzeichnis (Build-Artefakte, Libraries) ist in `.gitignore` ausgeschlossen und wird bei `pio run` automatisch wiederhergestellt.

## Verwendetes Board

ESP32 Display,2.8" ESP32-Display ESP32-2432S028R Resistiver Touchscreen 240x320 TFT LCD Display Modul,ESP32 Entwicklungsplatine mit WiFi Bluetooth, für Arduino IDE(2 Stücke)

```
✅Prozessor: ESP32-D0WDQ6 Dual-Core 32-Bit LX6 CPUTaktfrequenz: einstellbar von 80 MHz bis 240 MHz
✅Speicher: 520 KB SRAM, erweiterbar mit externem Flash und TF-Karte
✅Display: 2,8"" TFT-LCD mit 240×320 Auflösung, Touch-Funktion, inklusive Stylus
✅Schnittstelle: USB Type-C für Stromversorgung & Programmierung
✅Konnektivität: WLAN 802.11 b/g/n + Bluetooth Classic + BLE
✅Audio: Unterstützung für externe Lautsprecher, integrierter Audioausgang
✅LED: RGB-Dreifarb-LED zur Statusanzeige
✅Batterie: Unterstützung externer Lithium-Batterien mit integriertem Lade-Management
✅Kamera-Kompatibilität: OV2640 / OV7670
✅Speichererweiterung: Micro-TF-Kartenslot (bis 32 GB empfohlen)
✅Betriebssystem-Unterstützung: LwIP, FreeRTOS
✅Entwicklungsumgebungen: für Arduino IDE, für MicroPython,PlatformIO, Scratch 3.0
✅Treiber-Chip: ILI9341
✅GUI-Entwicklung: Unterstützung für LVGL – Erstellung moderner grafischer Benutzeroberflächen
```
## Attenuator-Steuerung

4 Dämpfungsglieder werden über GPIOs geschaltet (active HIGH, 10 dB Schritte):

| GPIO | Dämpfungsglied |
|------|----------------|
| 4    | 10 dB          |
| 16   | 20 dB          |
| 17   | 40 dB (A)      |
| 22   | 40 dB (B)      |

Maximale Dämpfung: 110 dB (10 + 20 + 40 + 40). Die Einer-Stelle der Anzeige wird ignoriert (nur 10er-Schritte).

### Beispiele

| Anzeige | Effektiv | GPIO 4 (10) | GPIO 16 (20) | GPIO 17 (40A) | GPIO 22 (40B) |
|---------|----------|-------------|--------------|---------------|---------------|
| 053     | 50 dB    | HIGH        | LOW          | HIGH           | LOW           |
| 117     | 110 dB   | HIGH        | HIGH         | HIGH           | HIGH          |
| 029     | 20 dB    | LOW         | HIGH         | LOW            | LOW           |
| 005     | 0 dB     | LOW         | LOW          | LOW            | LOW           |

## Basiert auf

[ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) – LVGL8 Beispiel

## Lizenz

MIT


~/.platformio/penv/bin/pio run -e cyd 2>&1 | tail -12