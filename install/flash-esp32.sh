#!/usr/bin/env bash
# ESP32 flashen -- Linux. Kein Python/PlatformIO noetig.
# ESP32 per USB anstecken, dann dieses Skript ausfuehren.
cd "$(dirname "$0")"

chmod +x ./esptool/esptool 2>/dev/null || true

./esptool/esptool --chip esp32 --baud 921600 write_flash 0x0 firmware/firmware-merged.bin

echo
echo "Fertig. Enter zum Schliessen."
read _
