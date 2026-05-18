# Auto-Set Funktion

## Grundidee

Auto-Set steuert, ob eine Änderung des dB-Werts *sofort* an die Relais
weitergegeben wird oder erst nach einem expliziten Bestätigungsschritt.

---

## ESP32

**Zustand:** `bool autoset`  
Standard: `true`, persistent in NVS via `prefs.putBool("ae", autoset)`

### Auto-Set ON
- Jeder Druck auf UP/DOWN (`btn_up_cb` / `btn_down_cb`) ruft sofort
  `apply_attenuation()` auf → Relais schalten unmittelbar
- Der „SET"-Button ist **versteckt** (`LV_OBJ_FLAG_HIDDEN`)
- Digit-Klick im Haupttab löst ebenfalls sofort `apply_attenuation()` aus

### Auto-Set OFF
- UP/DOWN ändert nur den angezeigten Wert, die Relais schalten **nicht**
- Der „SET"-Button wird **sichtbar** – erst sein Druck ruft
  `apply_attenuation_set()` auf
- Der Wert kann unverbindlich eingestellt und dann per Tastendruck übernommen werden

### Quellen für Zustandsänderungen

| Auslöser                          | Reaktion                                                              |
|-----------------------------------|-----------------------------------------------------------------------|
| Toggle-Switch im Config-Tab (LVGL)| Speichert in NVS, sendet `AUTO:ON/OFF` per Serial an Pico, WebSocket-Broadcast |
| Web-Interface (Browser)           | `web_update_ae()` synchronisiert LVGL-Switch + Label + SET-Button, sendet `AUTO:ON/OFF` an Pico |
| Serial-Eingang vom Pico (`AUTO:ON/OFF`) | Aktualisiert `autoset`, NVS, LVGL-Switch, Label, SET-Button    |

---

## Pico

**Zustand:** `bool auto_set_mode`  
Standard: `true`

### Auto-Set ON
- Wenn der ESP32 einen dB-Wert schickt (`"XXXdB"`), ruft
  `apply_attenuation_from_esp_db()` auch `apply_relays()` auf → Relais
  schalten sofort mit
- OLED zeigt unten: `AUTO-SET: ON`

### Auto-Set OFF
- `apply_attenuation_from_esp_db()` aktualisiert nur Display und internen
  Zustand, ruft **kein** `apply_relays()` auf
- OLED zeigt unten: `AUTO-SET: OFF`
- Ein **Encoder-Einzelklick** übernimmt den angezeigten Wert manuell auf die
  Relais und sendet ihn zurück an den ESP32

### Quellen für Zustandsänderungen

| Auslöser                          | Reaktion                                        |
|-----------------------------------|-------------------------------------------------|
| Serial vom ESP32 (`AUTO:ON/OFF`)  | Setzt `auto_set_mode`, aktualisiert OLED        |
| Beim Start                        | Sendet aktuellen Zustand per `sync_state_to_esp32()` → ESP32 übernimmt ihn |

---

## Synchronisation zwischen ESP32 und Pico

```
ESP32  ──── AUTO:ON / AUTO:OFF ────►  Pico   (Serial1, 115200 Baud)
Pico   ──── AUTO:ON / AUTO:OFF ────►  ESP32  (beim Booten via sync_state_to_esp32)
```

Der Zustand wird in **beide Richtungen** synchronisiert.  
Der ESP32 ist Master für Änderungen aus UI und Web-Interface.  
Der Pico schickt seinen persistierten Zustand beim Start.  
**Letzter Schreiber gewinnt.**
