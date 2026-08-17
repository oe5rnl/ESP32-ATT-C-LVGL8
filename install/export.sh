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

PIO="$HOME/.platformio/penv/bin/pio"
ESPTOOL="$HOME/.platformio/penv/bin/esptool"
BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

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
