# Serielle Schnittstellen am Raspberry Pi Pico

## Übersicht

Der Pico-Code verwendet **zwei voneinander unabhängige serielle Schnittstellen**:

1. **Serial1** - Hardware UART für ESP32-Kommunikation
2. **Serial** - USB CDC für Debug und manuelle Steuerung

Beide Schnittstellen arbeiten **parallel** und unabhängig voneinander.

---

## Serial1 (UART0 - ESP32 Kommunikation)

### Funktion
- **Empfängt Dämpfungswerte vom ESP32**
- Hardware UART0 des RP2040
- Produktionsschnittstelle für das System

### Pin-Belegung
```
GP0 (Pin 1)  - UART0 TX (Pico→ESP32)
GP1 (Pin 2)  - UART0 RX (Pico←ESP32)
```

### Verkabelung zum ESP32
```
ESP32 P3 Connector          Raspberry Pi Pico
─────────────────────────────────────────────
GPIO21 (TX) ──────────────→ GP1 (Pin 2, RX)
GPIO22 (RX) ←────────────── GP0 (Pin 1, TX)
GND         ←──────────────→ GND
```

### Protokoll
- **Baudrate:** 115200
- **Format:** `XXdB\n` (z.B. `50dB`, `110DB`)
- **Case-insensitive:** Groß-/Kleinschreibung egal
- **Bereich:** 0-110 dB in 10 dB Schritten
- **Timeout:** 100ms

### Code-Beispiel
```cpp
void setup() {
    /* Serial1 für ESP32: GP0 (TX), GP1 (RX) = UART0 */
    Serial1.begin(115200);
    Serial1.setTimeout(100);
}

void loop() {
    /* Empfang von ESP32 */
    if(Serial1.available()) {
        String input = Serial1.readStringUntil('\n');
        input.trim();
        input.toLowerCase();
        
        int dbPos = input.indexOf("db");
        if(dbPos > 0) {
            String numStr = input.substring(0, dbPos);
            int val = numStr.toInt();
            if(val >= 0 && val <= 110) {
                apply_attenuation(val);
                
                /* LED solid für 2 Sekunden */
                led_solid_mode = true;
                led_solid_start = millis();
                digitalWrite(LED_BUILTIN, HIGH);
            }
        }
    }
}
```

### Verhalten bei Empfang
1. String wird empfangen (z.B. `"70dB\n"`)
2. Parsing und Validierung
3. `apply_attenuation(70)` wird aufgerufen
4. GPIOs werden gesetzt
5. Display wird aktualisiert
6. **LED leuchtet 2 Sekunden lang durchgehend**
7. Danach LED-Blinkmodus (300ms)

---

## Serial (USB CDC - Debug & Manuelle Steuerung)

### Funktion
- **Debug-Ausgaben** während der Entwicklung
- **Manuelle Steuerung** über PlatformIO Serial Monitor
- USB CDC (Communication Device Class)
- Unabhängig von der ESP32-Kommunikation

### Anschluss
- **USB-Kabel** zwischen Pico und PC
- Erscheint als `/dev/ttyACM0` (oder ähnlich)
- PlatformIO Monitor: `~/.platformio/penv/bin/pio device monitor`

### Kommandos
```
0-110    Setze Dämpfung (10 dB Schritte)
?        Zeige aktuellen Wert
```

### Code-Beispiel
```cpp
void setup() {
    Serial.begin(115200);
    delay(2000);  // Warte auf USB-Verbindung
    
    Serial.println("Attenuator PICO started");
    Serial.println("Commands (USB):");
    Serial.println("  0-110  : Set attenuation");
    Serial.println("  ?      : Show current value");
}

void loop() {
    /* Empfang von USB */
    if(Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        
        if(input == "?") {
            Serial.print("Current: ");
            Serial.print(current_db);
            Serial.println(" dB");
        }
        else {
            int val = input.toInt();
            if(val >= 0 && val <= 110) {
                apply_attenuation(val);
            }
            else {
                Serial.println("ERROR: Value must be 0-110");
            }
        }
    }
}
```

### Verwendung
```bash
# PlatformIO Monitor öffnen
cd /home/oe5rnl/0_dev/ESP32-ATT-C-LVGL8/pico
~/.platformio/penv/bin/pio device monitor

# Kommandos eingeben:
> 50        # Setze 50 dB
ATT: 50 dB  [10=1 20=0 40a=1 40b=0]

> ?         # Aktuellen Wert abfragen
Current: 50 dB

> 110       # Setze Maximum
ATT: 110 dB  [10=1 20=1 40a=1 40b=1]
```

---

## Vergleich Serial vs Serial1

| Eigenschaft | Serial1 | Serial |
|------------|---------|--------|
| **Typ** | Hardware UART0 | USB CDC |
| **Pins** | GP0 (TX), GP1 (RX) | USB-Kabel |
| **Verbindung** | ESP32 über P3 | PC über USB |
| **Zweck** | Produktions-Interface | Debug & Test |
| **Protokoll** | `XXdB\n` (Case-insensitive) | Zahlen oder `?` |
| **Baudrate** | 115200 | 115200 |
| **Initialisierung** | `Serial1.begin(115200)` | `Serial.begin(115200)` |
| **Empfang** | `Serial1.available()` | `Serial.available()` |
| **Ausgabe** | `Serial1.print()` | `Serial.print()` |
| **LED-Trigger** | Ja (2s solid) | Nein |
| **Immer aktiv** | Ja (nach Boot) | Nur wenn USB verbunden |

---

## Parallelbetrieb

Beide Schnittstellen funktionieren **gleichzeitig**:

```
┌─────────────────────┐
│   ESP32 (GPIO21)    │
│        TX           │
└──────────┬──────────┘
           │
           ▼
    ┌──────────────┐         USB
    │ Pico GP1 RX  │◄───────────────┐
    │              │                 │
    │   Serial1 ───┼──► apply_att()  │
    │   Serial  ───┼──► apply_att()  │
    │              │                 │
    │   Outputs:   │                 │
    │   - GPIOs    │                 ▼
    │   - Display  │           ┌──────────┐
    │   - LED      │           │    PC    │
    └──────────────┘           └──────────┘
```

### Beispiel-Szenario
1. **ESP32 sendet:** `"70dB\n"` über Serial1
   - Pico empfängt auf GP1
   - Setzt Dämpfung auf 70 dB
   - LED leuchtet 2s
   - Display zeigt "70 dB"

2. **Gleichzeitig Monitor verbunden:**
   - Serial zeigt: `"ATT: 70 dB [10=1 20=0 40a=1 40b=1]"`
   - Keine Beeinflussung der Serial1-Kommunikation

3. **Manuelle Eingabe über USB:** `50`
   - Setzt Dämpfung auf 50 dB
   - ESP32 weiß nichts davon (keine Rückmeldung)
   - Display und GPIOs werden aktualisiert

---

## Hardware-Details

### UART0 (Serial1) am RP2040
```
Pico Pin 1  - GP0  - UART0_TX
Pico Pin 2  - GP1  - UART0_RX
Pico Pin 3  - GND
```

### USB CDC (Serial)
- RP2040 hat **eingebauten USB-Controller**
- Kein zusätzlicher Chip nötig (wie bei Arduino Uno)
- Automatische Enumeration beim Anstecken
- Erscheint als ACM-Device unter Linux

---

## Troubleshooting

### Serial1 funktioniert nicht
```
✓ Verkabelung prüfen:
  - ESP32 GPIO21 → Pico GP1
  - ESP32 GPIO22 → Pico GP0
  - GND → GND
  
✓ ESP32 sendet?
  - Serial Monitor am ESP32 öffnen
  - Bei "Apply" sollte "Serial2 → XXdB" erscheinen
  
✓ Baudrate korrekt?
  - ESP32: Serial2.begin(115200)
  - Pico: Serial1.begin(115200)
```

### Serial (USB) funktioniert nicht
```
✓ USB-Kabel eingesteckt?
✓ PlatformIO Monitor läuft?
  cd pico && ~/.platformio/penv/bin/pio device monitor
  
✓ Richtiges Device?
  ls -l /dev/ttyACM*
  
✓ Berechtigung?
  sudo usermod -a -G dialout $USER
  (Neuanmeldung erforderlich)
```

### LED blinkt nicht nach ESP32-Empfang
```
✓ String-Format korrekt?
  - Muss "db" oder "DB" enthalten
  - Beispiel: "50dB\n"
  
✓ led_solid_mode wird gesetzt?
  - Debug: Serial.println in Serial1-Handler einfügen
  
✓ 2 Sekunden abwarten
  - LED wechselt nach 2s von solid auf blink
```

### Display zeigt nichts
```
✓ I2C-Verkabelung (Software I2C):
  - GP19 (Pin 25) → SCL
  - GP20 (Pin 26) → SDA
  - 3.3V → VCC
  - GND → GND
  
✓ U8g2 Library installiert?
  platformio.ini: olikraus/U8g2@^2.35.9
  
✓ Display-Adresse korrekt?
  - Standard: 0x3C
  - Bei Problemen I2C-Scanner verwenden
```

---

## Zusammenfassung

| **Serial1** | **Serial** |
|-------------|-----------|
| UART Hardware | USB Software |
| ESP32 → Pico | PC ↔ Pico |
| Automatisch | Manuell |
| Produktiv | Debug |
| `"XXdB\n"` | Zahlen/`?` |
| LED 2s solid | LED unverändert |

**Beide Schnittstellen arbeiten unabhängig und parallel.**

