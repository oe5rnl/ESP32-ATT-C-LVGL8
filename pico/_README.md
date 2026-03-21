# Raspberry Pi Pico - Attenuator Controller

Einfache Implementierung des 26.5 GHz Attenuator Controllers für den Raspberry Pi Pico.

## Hardware

- **Board**: Raspberry Pi Pico
- **GPIO-Pins** (anpassbar in `src/main.cpp`):
  - GP2: 10 dB Pad (active LOW)
  - GP3: 20 dB Pad (active LOW)
  - GP4: 40 dB Pad A (active LOW)
  - GP5: 40 dB Pad B (active LOW)
- **UART1 für ESP32-Kommunikation**:
  - GP0: TX → ESP32 GPIO22 (Serial1 = UART0)
  - GP1: RX ← ESP32 GPIO21

## Funktionen

Aktuell: 
- Serielle Steuerung über USB (CDC)
- Serial1-Kommunikation mit ESP32 auf GP0/GP1 (empfängt "xxdB" Format)

- Sende `0-110` für Dämpfung in 10 dB Schritten
- Sende `?` für aktuellen Wert

## Build & Upload

```bash
cd pico
~/.platformio/penv/bin/pio run -t upload
~/.platformio/penv/bin/pio device monitor
```

> **Linux/Mac:** PlatformIO ist normalerweise unter `~/.platformio/penv/bin/pio` installiert.

## Geplante Features

- [ ] USB HID/CDC Interface
- [ ] I2C/SPI Slave für Steuerung vom ESP32
- [ ] Display Interface (SPI)
- [ ] Webserver (mit Pico W)
- [ ] EEPROM für Default-Werte

## Unterschiede zu ESP32-Version

| Feature | ESP32 | Pico |
|---------|-------|------|
| Display | LVGL8 Touch GUI | (geplant) |
| WiFi | AP + Client | Pico W benötigt |
| Webserver | Vollständig | (geplant) |
| Storage | NVS Preferences | LittleFS/Flash |
| Steuerung | Touch + Web | Seriell |

## Entwicklung

Das Pico-Projekt ist komplett eigenständig und unabhängig vom ESP32-Projekt im Hauptordner.
