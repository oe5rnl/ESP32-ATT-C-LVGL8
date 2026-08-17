# Firmware-Installer (offline, ohne Installation)

Dieses Verzeichnis flasht die Firmware auf **ESP32** und **Raspberry Pi Pico** —
komplett offline, ohne dass der Nutzer Software installieren muss. Es wird nur
ein mitgeliefertes Programm **aufgerufen**.

```
install/
├── flash-esp32.sh      # ESP32 flashen (Linux)
├── flash-esp32.bat     # ESP32 flashen (Windows)
├── esptool/            # eigenstaendige esptool-Binaries (esptool + esptool.exe, v5.3.1)
├── firmware/           # firmware-merged.bin (ESP32) + firmware.uf2 (Pico)
└── export.sh           # nur fuer Entwickler: baut + erzeugt die Firmware-Dateien
```

---

## ESP32 flashen

1. ESP32 (CYD) per USB anstecken.
2. **Linux:** im Terminal `./flash-esp32.sh` ausführen.
   **Windows:** `flash-esp32.bat` doppelklicken.
3. Warten bis „Fertig" erscheint — die Firmware ist geflasht.

Der serielle Port wird automatisch erkannt.

### Hinweise
- **Linux-Rechte:** Falls der Zugriff auf den USB-Port scheitert, den Benutzer
  einmalig zur Gruppe `dialout` hinzufügen und neu anmelden:
  `sudo usermod -aG dialout $USER` (alternativ das Skript mit `sudo` starten).
- **Windows-Treiber:** Die CYD-Boards nutzen einen **CH340**- oder **CP2102**-
  USB-Seriell-Chip. Falls Windows das Gerät nicht erkennt, den passenden Treiber
  installieren (WCH CH340 bzw. Silicon Labs CP210x).

---

## Pico flashen (kein Programm nötig)

1. **BOOTSEL**-Taste am Pico gedrückt halten und Pico per USB anstecken.
   Es erscheint ein Laufwerk namens **`RPI-RP2`**.
2. Die Datei `firmware/firmware.uf2` per Drag & Drop auf dieses Laufwerk kopieren.
3. Der Pico startet automatisch neu und ist fertig geflasht.

---

## Für den Entwickler: Firmware-Dateien erzeugen

`export.sh` baut beide Projekte mit PlatformIO und legt die auslieferbaren
Dateien in `firmware/` ab:

```bash
cd install
./export.sh
```

Ergebnis:
- `firmware/firmware-merged.bin` — eine einzige ESP32-Datei (Bootloader +
  Partitionen + App zusammengeführt).
- `firmware/firmware.uf2` — Pico-Firmware.

Die eigenständigen **esptool**-Binaries (`esptool` für Linux, `esptool.exe` für
Windows, v5.3.1) liegen bereits in `esptool/`. Aktualisieren lassen sie sich bei
Bedarf über die [Espressif-Releases](https://github.com/espressif/esptool/releases).
