@echo off
REM ESP32 flashen -- Windows. Kein Python/PlatformIO noetig.
REM ESP32 per USB anstecken, dann diese Datei doppelklicken.
cd /d "%~dp0"

esptool\esptool.exe --chip esp32 --baud 921600 write_flash 0x0 firmware\firmware-merged.bin

echo.
echo Fertig.
pause
