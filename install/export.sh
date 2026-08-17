#!/usr/bin/env bash
# Erzeugt die auslieferbaren Firmware-Dateien fuer das install/-Paket.
# Nur fuer den Entwickler gedacht (benoetigt PlatformIO). Der Endnutzer
# braucht dieses Skript NICHT.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ESP_DIR="$REPO_ROOT/esp32"
PICO_DIR="$REPO_ROOT/pico"
OUT="$SCRIPT_DIR/firmware"

# --- Werkzeuge automatisch finden ------------------------------------------
find_tool() {
  # $1 = Name; sucht in PATH und in gaengigen PlatformIO-Pfaden
  local name="$1" p
  if command -v "$name" >/dev/null 2>&1; then command -v "$name"; return 0; fi
  for p in \
    "$HOME/.platformio/penv/bin/$name" \
    "$HOME/.local/bin/$name" \
    "/usr/local/bin/$name"; do
    [ -x "$p" ] && { echo "$p"; return 0; }
  done
  return 1
}

PIO="$(find_tool pio || true)"
if [ -z "$PIO" ]; then
  echo "FEHLER: PlatformIO (pio) nicht gefunden."
  echo "Installieren oder Pfad pruefen, z. B.:"
  echo "  python3 -m pip install --user platformio"
  exit 1
fi

# esptool: bevorzugt das mitgelieferte Binary im install/esptool/-Ordner
if [ -x "$SCRIPT_DIR/esptool/esptool" ]; then
  ESPTOOL="$SCRIPT_DIR/esptool/esptool"
else
  ESPTOOL="$(find_tool esptool || true)"
fi
if [ -z "$ESPTOOL" ]; then
  echo "FEHLER: esptool nicht gefunden (weder in install/esptool/ noch im PATH)."
  exit 1
fi

# boot_app0.bin im PlatformIO-Package-Verzeichnis suchen
BOOT_APP0="$(find "$HOME/.platformio/packages" -path '*tools/partitions/boot_app0.bin' 2>/dev/null | head -n1)"
if [ -z "$BOOT_APP0" ]; then
  echo "FEHLER: boot_app0.bin nicht gefunden. Wurde das ESP32-Projekt schon einmal gebaut?"
  exit 1
fi

echo "pio:       $PIO"
echo "esptool:   $ESPTOOL"
echo "boot_app0: $BOOT_APP0"
echo

mkdir -p "$OUT"

echo "==> ESP32 bauen (env cyd)"
( cd "$ESP_DIR" && "$PIO" run -e cyd )

echo "==> ESP32 zu einer Datei zusammenfuehren (merge_bin)"
"$ESPTOOL" --chip esp32 merge_bin -o "$OUT/firmware-merged.bin" \
  --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000  "$ESP_DIR/.pio/build/cyd/bootloader.bin" \
  0x8000  "$ESP_DIR/.pio/build/cyd/partitions.bin" \
  0xe000  "$BOOT_APP0" \
  0x10000 "$ESP_DIR/.pio/build/cyd/firmware.bin"

echo "==> Pico bauen (env pico)"
( cd "$PICO_DIR" && "$PIO" run -e pico )
cp "$PICO_DIR/.pio/build/pico/firmware.uf2" "$OUT/firmware.uf2"

echo
echo "Fertig. Auslieferbare Dateien liegen in:"
echo "  $OUT/firmware-merged.bin   (ESP32)"
echo "  $OUT/firmware.uf2          (Pico)"
