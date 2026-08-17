# Benutzerhandbuch — Attenuator Controller

Bedienung des Dämpfungsglied-Controllers. Die Steuerung ist auf zwei Wegen
möglich, die jederzeit denselben Gerätezustand teilen (bidirektional per
WebSocket synchronisiert):

- **Touch-Display** — LVGL-Oberfläche direkt am ESP32.
- **Web-Oberfläche** — im Browser über WLAN erreichbar.

Quellen: [esp32/src/main.cpp](esp32/src/main.cpp), [esp32/src/webserver.h](esp32/src/webserver.h).

---

## A) Touch-Display

Die Oberfläche gliedert sich in **drei Tabs** (Tab-Leiste am unteren Rand):
**Main**, **Presets** und **Menu**.

### Tab „Main" — Dämpfungswert einstellen

- **3-stellige Ziffernanzeige** (Hunderter | Zehner | Einer) in großer Schrift.
- Unter der aktiven Stelle steht ein **Unterstrich-Cursor**.
- Bedienelemente:
  - **UP** — erhöht die gewählte Stelle (Hunderter × 100, Zehner × 10).
  - **DOWN** — verringert die gewählte Stelle.
  - **Set** — überträgt den Wert an den Pico (nur im Modus *Set-Button* sichtbar,
    siehe unten). Wird **rot**, sobald der angezeigte Wert vom zuletzt gesetzten
    abweicht.
  - **RF-Schalter** — schaltet den HF-Zweig **ON (grün) / OFF (rot)**. Nur
    sichtbar, wenn das erkannte Dämpfungsglied einen RF-Schalter besitzt.
  - **Status-Label** — zeigt den aktiven Set-Modus: `SET-DIRECT`, `SET-TIME`
    oder `SET-BUTTON`.

**Wert ändern:**
- **Ziffer antippen** wählt die aktive Stelle aus (Cursor springt dorthin).
- **UP/DOWN gedrückt halten** wiederholt das Zählen automatisch.
- **Ziffer lang drücken** öffnet ein Zahlenfeld zur direkten Eingabe des
  Gesamtwerts.
- Über die serielle Verbindung vom Pico empfangene Werte aktualisieren die
  Anzeige automatisch.

### Tab „Presets" — Schnellzugriffe

- **9 Buttons** in einem 3×3-Raster, jeder mit einem hinterlegten dB-Wert
  beschriftet.
- Ein Button wird **rot** hervorgehoben, wenn sein Wert dem aktuellen entspricht.
- **Kurzer Klick** — wendet den Preset-Wert sofort an.
- **Langer Klick (ca. 600 ms)** — öffnet ein Zahlenfeld zum Bearbeiten des
  Button-Werts. Der neue Wert wird dauerhaft gespeichert (NVS) und mit der
  Web-Oberfläche synchronisiert.
- Werkseinstellung der 9 Presets: **20, 40, 60, 10, 30, 50, 70, 80, 90 dB**.

### Tab „Menu" — Untermenüs

Vier Untermenüpunkte, jeweils mit **Zurück-Button**:

1. **WLAN**
   - Ein/Aus-Schalter für WLAN (0 = aus, AP + Client = ein).
   - Status-Anzeige: AP-SSID, AP-IP, Client-SSID/-IP (falls verbunden).
2. **Info** (schreibgeschützt)
   - Name des Dämpfungsglieds, Relaisanzahl, maximale Dämpfung, Schrittweite,
     Relais-Modus (Bridge/Static), ESP- und Pico-Firmware-Version.
3. **Test**
   - **Start** aktiviert den Testmodus (sendet `TEST:START` an den Pico).
   - Navigation **< Relais >** wählt das zu prüfende Relais aus
     (Bridge 1–9 bzw. Static 10/20/40A/40B).
   - Status-Anzeige **EIN/AUS**, **Puls/Toggle** (sendet `TEST:ACTION`),
     **Beenden** (sendet `TEST:END`).
4. **Verhalten** — Set-Modus auswählen (1 aus 3):
   - **Direct** — UP/DOWN setzt den Wert sofort.
   - **Time** — UP/DOWN setzt den Wert zeitgesteuert.
   - **Set-Button** — der Set-Button erscheint; die Übernahme muss manuell
     bestätigt werden.
   - Die Auswahl wird gespeichert und an den Pico übertragen.

### Wertebereich und Ziffernbegrenzung

- Das Gesamtmaximum der dB Werte ist **nicht fest**, sondern hängt vom erkannten
  Dämpfungsglied ab. Beim Start meldet der Pico Maximalwert und Schrittweite
  (`MAXDB` / `STEP`); typische Werte sind **110 dB** (26.5 GHz, 10 dB-Schritte),
  **135 dB** (RS-135, 5 dB-Schritte) und **141 dB** (RS-141).
- `DIGIT_MAX_VAL` (110) ist lediglich der Vorgabewert im ESP32-Code, bis der Pico
  den tatsächlichen Wert übermittelt; danach gilt der gemeldete Maximalwert.
- Standard-Ziffernbegrenzung vor der Erkennung: Hunderter 0–1, Zehner 0–9, Einer
  gesperrt (10er-Schritte).
- Nach der Erkennung wird die Ziffernbegrenzung aus Maximalwert und Schrittweite
  neu berechnet (die Einer-Stelle wird nur freigeschaltet, wenn die Schrittweite
  kleiner als 10 dB ist).
- Bei direkter Eingabe wird der Wert auf das Maximum begrenzt und auf gesperrte
  Stellen abgerundet (z. B. 33 → 30, wenn die Einer-Stelle gesperrt ist).

---

## B) Web-Oberfläche

Erreichbar im Browser, sobald WLAN aktiv ist:

- Über den Access-Point **`ESP32-ATT`** (Passwort `12345678`), Standard-IP
  `192.168.4.1`.
- Oder nach WLAN-Anbindung im Heimnetz unter **`esp32-att.local`** (mDNS).

Die Seite spiegelt die Display-Funktionen und ist in Echtzeit per WebSocket
synchronisiert (Heartbeat im Sekundentakt hält die Verbindung).

### Hauptbereich — Dämpfungswert

- **3 Ziffern** (anklickbar zur Auswahl der aktiven Stelle).
- **DOWN / UP** — Wert um einen Schritt verringern/erhöhen.
- **Set** — überträgt den Wert (nur im Modus *Set-Button* sichtbar).
- **RF** — HF-Schalter umschalten (nur sichtbar, wenn vorhanden).

### Verhalten — Set-Modus

Drei Schaltflächen wählen den Set-Modus: **Set-Direct**, **Set-Time**,
**Set-Button** (Wirkung identisch zum Display).

### Defaults — Presets

- **3×3-Raster** mit 9 Buttons (Wert + „dB").
- **Kurzer Klick** — Preset anwenden.
- **Langer Klick (ca. 600 ms)** — Zahleneingabe-Dialog zum Bearbeiten; der Wert
  wird dauerhaft gespeichert.

### WLAN-Möglichkeiten

Das Gerät kennt zwei WLAN-Betriebsarten (umschaltbar am Display unter
**Menu → WLAN**, im NVS gespeichert):

- **Aus** — WLAN vollständig deaktiviert; die Web-Oberfläche ist dann nicht
  erreichbar.
- **AP + Client** — der Access Point ist **immer** aktiv, zusätzlich verbindet
  sich das Gerät als Client mit einem vorhandenen WLAN, sobald Zugangsdaten
  hinterlegt sind.

Daraus ergeben sich für den Zugriff über den Browser zwei Wege:

- **Access-Point (immer verfügbar):** direkt mit dem WLAN **`ESP32-ATT`**
  (Passwort `12345678`) verbinden und `http://192.168.4.1` öffnen. Dieser Weg
  funktioniert auch ohne jede Heimnetz-Anbindung.
- **Heimnetz (Client):** nach erfolgreicher Anbindung ist das Gerät zusätzlich
  im normalen WLAN erreichbar — über `http://esp32-att.local` (mDNS) oder die
  angezeigte Client-IP.

#### WLAN einrichten (Bereich „WLAN-Einrichtung", aufklappbar)

- **Netzwerke suchen** — startet einen WLAN-Scan (`/api/scan`), die Ergebnisse
  werden nachgeladen (`/api/scanresult`).
- **Netzwerk-Auswahl** — Dropdown mit den gefundenen Netzwerken.
- **SSID** und **Passwort** — manuelle Eingabe möglich, Passwort mit
  Sichtbarkeits-Umschalter.
- **Verbinden** — speichert die Zugangsdaten dauerhaft (NVS) und verbindet nicht
  blockierend (`/api/connect`). Bei Timeout (ca. 20 s) oder Fehler bleibt der
  Access Point aktiv (Fallback auf AP-only), sodass die Oberfläche erreichbar
  bleibt.
- **Status-Anzeige** — grün bei erfolgreicher Verbindung, rot bei Fehler.

> **Hinweis:** Das komplette Ausschalten des WLANs ist nur am Display möglich,
> nicht über die Web-Oberfläche (das würde die eigene Verbindung trennen).

### Statusbereich

- **LED-Indikator** — grün = verbunden, rot = getrennt (Reconnect läuft).
- **WLAN-Info** — „aus" / „AP + Client" / „AP + Client verbunden" inkl.
  IP-Adressen und Signalstärke.

### HTTP-Endpunkte (technisch)

| Endpunkt | Methode | Funktion |
|----------|---------|----------|
| `/` | GET | Web-Seite |
| `/ws` | WebSocket | Echtzeit-Synchronisation |
| `/api/mode` | GET | aktueller WLAN-Modus |
| `/api/scan` | GET | WLAN-Scan starten |
| `/api/scanresult` | GET | Scan-Ergebnisse abrufen |
| `/api/wifistatus` | GET | Verbindungsstatus (SSID, IP, RSSI, AP-IP) |
| `/api/connect` | POST | WLAN-Zugangsdaten setzen und verbinden |

> **Hinweis:** Ein OTA-Firmware-Update über die Web-Oberfläche ist derzeit
> **nicht** implementiert.

---

## Gemeinsamkeiten

- Beide Oberflächen teilen denselben Zustand (Dämpfungswert, Presets, Set-Modus,
  RF-Schalter, WLAN-Einstellungen) und sind per WebSocket synchronisiert.
- Preset-Werte, Set-Modus, WLAN-Modus und Zugangsdaten werden im NVS des ESP32
  gespeichert und überstehen einen Neustart.
