#!/bin/bash
# Raspberry Pi Pico Upload-Skript
# Verwendung: ./upload.sh

set -e

echo "🔨 Building firmware..."
~/.platformio/penv/bin/pio run

echo ""
echo "📋 Bereit zum Flashen!"
echo ""
echo "Schritte:"
echo "1. Halte die BOOTSEL-Taste am Pico gedrückt"
echo "2. Stecke den Pico ein (oder drücke Reset)"
echo "3. Lasse die BOOTSEL-Taste los"
echo "4. Der Pico erscheint als USB-Laufwerk 'RPI-RP2'"
echo ""
read -p "Drücke ENTER wenn der Pico als Laufwerk gemountet ist..."

# Versuche das Laufwerk zu finden
if [ -d "/media/$USER/RPI-RP2" ]; then
    echo "✅ RPI-RP2 gefunden in /media/$USER/RPI-RP2"
    echo "📤 Kopiere firmware.uf2..."
    cp .pio/build/pico/firmware.uf2 /media/$USER/RPI-RP2/
    echo "✅ Upload abgeschlossen!"
    echo "Der Pico startet automatisch neu."
else
    echo "❌ RPI-RP2 Laufwerk nicht gefunden!"
    echo "Bitte manuell kopieren:"
    echo "   cp .pio/build/pico/firmware.uf2 /media/$USER/RPI-RP2/"
    exit 1
fi
