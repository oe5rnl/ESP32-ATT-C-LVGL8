# ESP32-ATT-C-LVGL8

~/.platformio/penv/bin/pio run 2>&1 | tail -30


LVGL 8 Widget-Demo für das ESP32 Cheap Yellow Display (CYD).

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
pio run -t upload

# CYD2USB Variante (ST7789)
pio run -e cyd2usb -t upload

# Seriellen Monitor öffnen
pio device monitor
```

## Projektstruktur

```
ESP32-ATT-C-LVGL8/
├── platformio.ini      # PlatformIO Konfiguration mit TFT_eSPI Build-Flags
├── lv_conf.h           # LVGL 8 Konfiguration
├── src/
│   └── main.cpp        # Hauptprogramm: Display, Touch, LVGL Demo
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
- `LV_USE_DEMO_WIDGETS 1`

### Touch-Kalibrierung

Die Touch-Kalibrierungswerte in `src/main.cpp` (Zeilen 32-33) müssen eventuell an das eigene Display angepasst werden:

```cpp
uint16_t touchScreenMinimumX = 200, touchScreenMaximumX = 3700;
uint16_t touchScreenMinimumY = 240, touchScreenMaximumY = 3800;
```

## Abhängigkeiten

Werden automatisch von PlatformIO heruntergeladen:

- [LVGL](https://github.com/lvgl/lvgl) v8.3.x
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) v2.5.x
- [XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) v1.4

## Basiert auf

[ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) – LVGL8 Beispiel

## Lizenz

MIT
