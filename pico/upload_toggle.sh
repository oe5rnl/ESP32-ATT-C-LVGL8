#!/bin/bash
# Build & Upload: toggle_gpio10_13.cpp (Environment: toggle_gpio10_13)
# Verwendung: ./upload_toggle.sh
#
# Voraussetzung: Pico im BOOTSEL-Modus eingesteckt
# (BOOTSEL-Taste halten, einstecken, loslassen -> erscheint als RPI-RP2)

set -e

ENV=toggle_gpio10_13
MOUNT="/media/$USER/RPI-RP2"
UF2=".pio/build/${ENV}/firmware.uf2"
PIO="$HOME/.platformio/penv/bin/pio"

cd "$(dirname "$0")"

echo "🔨 Building '${ENV}' (toggle_gpio10_13.cpp)..."
"$PIO" run -e "$ENV"

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
