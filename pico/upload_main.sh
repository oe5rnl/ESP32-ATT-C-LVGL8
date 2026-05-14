#!/bin/bash
# Build & Upload: main.cpp (Environment: pico)
# Verwendung: ./upload_main.sh
#
# Voraussetzung: Pico im BOOTSEL-Modus eingesteckt
# (BOOTSEL-Taste halten, einstecken, loslassen -> erscheint als RPI-RP2)

set -e

ENV=pico
MOUNT="/media/$USER/RPI-RP2"
UF2=".pio/build/${ENV}/firmware.uf2"
PIO="$HOME/.platformio/penv/bin/pio"

cd "$(dirname "$0")"

echo "🔨 Building '${ENV}' (main.cpp)..."
"$PIO" run -e "$ENV"

echo ""
echo "📋 Bereit zum Flashen!"
echo "1. Halte die BOOTSEL-Taste am Pico gedrückt"
echo "2. Stecke den Pico ein (oder drücke Reset)"
echo "3. Lasse die BOOTSEL-Taste los"
echo "4. Der Pico erscheint als USB-Laufwerk 'RPI-RP2'"
echo ""
read -p "Drücke ENTER wenn der Pico als Laufwerk gemountet ist..."

if [ ! -d "$MOUNT" ]; then
    echo ""
    echo "❌ '$MOUNT' nicht gefunden."
    echo "   Pico im BOOTSEL-Modus einstecken und Skript erneut starten."
    exit 1
fi

echo "📤 Kopiere $UF2 -> $MOUNT/"
cp "$UF2" "$MOUNT/"
sync
echo "✅ Upload abgeschlossen. Pico startet neu."
