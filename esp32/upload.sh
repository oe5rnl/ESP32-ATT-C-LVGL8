#!/bin/bash
# Raspberry Pi Pico Upload-Skript
# Verwendung: ./upload.sh

set -e

echo "🔨 Building firmware..."
~/.platformio/penv/bin/pio run

echo "🔨 Upload firmware..."
/home/$USER/.platformio/penv/bin/pio run -t upload --upload-port /dev/ttyUSB0
