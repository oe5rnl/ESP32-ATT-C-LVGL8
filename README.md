# 26.5 GHz Attenuator Controller

Multi-Platform Steuerung für einen 26.5 GHz Attenuator mit zwei verfügbaren Implementierungen:

## 🆕 Projekte

### [ESP32](esp32/) - Full-Featured Controller
LVGL 8 Steuerungsoberfläche für ESP32 CYD (Cheap Yellow Display) mit:
* 2.8" Touch Display (ILI9341/ST7789)
* WLAN (AP + Client-Modus)
* WebGUI mit gleicher Funktion wie am Display
* NVS Persistenz

### [Pico](pico/) - Minimalist Controller  
Einfache serielle Steuerung für Raspberry Pi Pico:
* USB-Serielle Kommunikation
* GPIO-Steuerung (kompatibel zu ESP32)
* Geplant: Display, WebServer (Pico W)

## 🚀 Quick Start

### ESP32 flashen
```bash
cd esp32
~/.platformio/penv/bin/pio run -t upload
~/.platformio/penv/bin/pio device monitor
```

### Raspberry Pi Pico flashen
```bash
cd pico
~/.platformio/penv/bin/pio run -t upload
~/.platformio/penv/bin/pio device monitor
```

> **Hinweis:** Unter Linux/Mac ist PlatformIO normalerweise unter `~/.platformio/penv/bin/pio` installiert. Falls du es global installiert hast, kannst du auch `platformio` statt des vollen Pfads verwenden.

## 📋 Hardware-Anforderungen

| Komponente | ESP32 | Pico |
|------------|-------|------|
| **Mikrocontroller** | ESP32-2432S028 (CYD) | Raspberry Pi Pico |
| **Display** | ILI9341 (320×240) Touch | Optional (geplant) |
| **WiFi** | Integriert | Pico W benötigt |
| **Attenuator GPIOs** | 4, 16, 17, 22 | 2, 3, 4, 5 |

## 📖 Detaillierte Dokumentation

Siehe jeweilige Projekt-READMEs:
- [ESP32 README](esp32/README.md) - Vollständige Feature-Liste, WebGUI, WiFi-Setup
- [Pico README](pico/README.md) - Serielle Befehle, GPIO-Konfiguration

## 🏗️ Projektstruktur

```
ESP32-ATT-C-LVGL8/
├── esp32/                  # ESP32 CYD Projekt
│   ├── platformio.ini
│   ├── lv_conf.h
│   └── src/
│       ├── main.cpp
│       ├── webserver.h
│       ├── wifi_credentials.h
│       └── lv_font_digits_72.c
├── pico/                   # Raspberry Pi Pico Projekt
│   ├── platformio.ini
│   └── src/
│       └── main.cpp
└── doku/                   # Hardware-Dokumentation
```

## 🔧 Attenuator Hardware

Beide Implementierungen steuern 4 Dämpfungsglieder (active LOW, 10 dB Schritte):
- 10 dB + 20 dB + 40 dB (A) + 40 dB (B) = max. 110 dB

## 📝 Lizenz

Siehe [LICENSE](LICENSE).

## 👥 Credits

ESP32-Version basiert auf dem LVGL8 Beispiel von [ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display).


## Implementierungsdetails
Core 0 wird automatisch von Arduino/ESP-IDF für:

* AsyncTCP / ESPAsyncWebServer verwendet — alle WebSocket-Callbacks (onWsEvent) laufen hier
* WiFi-Stack

Core 1 ist der Arduino-Hauptcore:

* setup() und loop() (laufen immer auf Core 1)
* Damit auch webserver_loop(), lv_timer_handler() und alle LVGL-Zugriffe

Die Kommunikation zwischen den Cores lauft über den FreeRTOS-Queue-Dispatcher: xQueueSend/xQueueReceive


## Basiert auf

[ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) – LVGL8 Beispiel

## Lizenz

MIT


# ESP32 flashen (z.B. /dev/ttyUSB0)
~/.platformio/penv/bin/pio run -t upload --upload-port /dev/ttyUSB0

# Pico flashen (z.B. /dev/ttyACM0)
~/.platformio/penv/bin/pio run -t upload --upload-port /dev/ttyACM0

~/.platformio/penv/bin/pio run -e cyd 2>&1 | tail -12