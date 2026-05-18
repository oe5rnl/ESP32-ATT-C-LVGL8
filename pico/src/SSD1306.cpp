#include "SSD1306.h"
#include "font.h"
#include "big_digits.h"

// SSD1306 Kommandos
#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR 0x22
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_CHARGEPUMP 0x8D

SSD1306::SSD1306(TwoWire* wire, uint8_t addr) 
    : _wire(wire), _addr(addr) {
    memset(_buffer, 0, sizeof(_buffer));
}

uint8_t SSD1306::address() const {
    return _addr;
}

bool SSD1306::probe(uint8_t addr) {
    _wire->beginTransmission(addr);
    return _wire->endTransmission() == 0;
}

bool SSD1306::sendCommand(uint8_t cmd) {
    _wire->beginTransmission(_addr);
    _wire->write(0x00); // Command mode
    _wire->write(cmd);
    return _wire->endTransmission() == 0;
}

bool SSD1306::sendData(const uint8_t* data, size_t len) {
    _wire->beginTransmission(_addr);
    _wire->write(0x40); // Data mode
    for (size_t i = 0; i < len; i++) {
        _wire->write(data[i]);
    }
    return _wire->endTransmission() == 0;
}

bool SSD1306::init() {
    const uint8_t addresses[] = { _addr, static_cast<uint8_t>(_addr == 0x3C ? 0x3D : 0x3C) };
    bool found = false;

    for (uint8_t attempt = 0; attempt < 3 && !found; attempt++) {
        delay(20);
        for (uint8_t addr : addresses) {
            if (probe(addr)) {
                _addr = addr;
                found = true;
                break;
            }
        }
    }
    if (!found) return false;

    // Initialisierungssequenz für 128x64 Display
    if (!sendCommand(SSD1306_DISPLAYOFF)) return false;
    if (!sendCommand(SSD1306_SETDISPLAYCLOCKDIV)) return false;
    if (!sendCommand(0x80)) return false;
    if (!sendCommand(SSD1306_SETMULTIPLEX)) return false;
    if (!sendCommand(0x3F)) return false; // 64 Zeilen
    if (!sendCommand(SSD1306_SETDISPLAYOFFSET)) return false;
    if (!sendCommand(0x00)) return false;
    if (!sendCommand(SSD1306_SETSTARTLINE | 0x00)) return false;
    if (!sendCommand(SSD1306_CHARGEPUMP)) return false;
    if (!sendCommand(0x14)) return false; // Charge pump aktivieren
    if (!sendCommand(SSD1306_MEMORYMODE)) return false;
    if (!sendCommand(0x00)) return false; // Horizontal addressing mode
    if (!sendCommand(SSD1306_SEGREMAP | 0x01)) return false;
    if (!sendCommand(SSD1306_COMSCANDEC)) return false;
    if (!sendCommand(SSD1306_SETCOMPINS)) return false;
    if (!sendCommand(0x12)) return false;
    if (!sendCommand(SSD1306_SETCONTRAST)) return false;
    if (!sendCommand(0xCF)) return false;
    if (!sendCommand(SSD1306_SETPRECHARGE)) return false;
    if (!sendCommand(0xF1)) return false;
    if (!sendCommand(SSD1306_SETVCOMDETECT)) return false;
    if (!sendCommand(0x40)) return false;
    if (!sendCommand(SSD1306_DISPLAYALLON_RESUME)) return false;
    if (!sendCommand(SSD1306_NORMALDISPLAY)) return false;
    if (!sendCommand(SSD1306_DISPLAYON)) return false;

    clear();
    display();
    return true;
}

void SSD1306::clear() {
    memset(_buffer, 0, sizeof(_buffer));
}

void SSD1306::display() {
    if (!sendCommand(SSD1306_COLUMNADDR)) return;
    if (!sendCommand(0)) return;     // Column start
    if (!sendCommand(WIDTH - 1)) return; // Column end
    if (!sendCommand(SSD1306_PAGEADDR)) return;
    if (!sendCommand(0)) return;     // Page start
    if (!sendCommand(PAGES - 1)) return; // Page end
    
    // Daten in kleineren Chunks senden (I2C-Buffer-Limitierung)
    const size_t chunk_size = 16;
    for (size_t i = 0; i < sizeof(_buffer); i += chunk_size) {
        size_t remaining = sizeof(_buffer) - i;
        size_t to_send = remaining < chunk_size ? remaining : chunk_size;
        if (!sendData(_buffer + i, to_send)) return;
    }
}

void SSD1306::setPixel(uint8_t x, uint8_t y, bool on) {
    if (x >= WIDTH || y >= HEIGHT) return;
    
    uint8_t page = y / 8;
    uint8_t bit = y % 8;
    uint16_t idx = x + page * WIDTH;
    
    if (on) {
        _buffer[idx] |= (1 << bit);
    } else {
        _buffer[idx] &= ~(1 << bit);
    }
}

void SSD1306::drawChar(uint8_t x, uint8_t y, char c) {
    if (c < 32 || c > 127) c = '?';
    
    const uint8_t* glyph = font8x8_basic[(uint8_t)c];
    
    // Font ist zeilenweise gespeichert (8 Zeilen x 8 Spalten)
    // Jedes Byte ist eine horizontale Zeile von 8 Pixeln
    for (uint8_t row = 0; row < 8; row++) {
        uint8_t line = glyph[row];
        for (uint8_t col = 0; col < 8; col++) {
            if (line & (1 << col)) {
                setPixel(x + col, y + row, true);
            }
        }
    }
}

void SSD1306::drawString(uint8_t x, uint8_t y, const char* str) {
    uint8_t posX = x;
    while (*str) {
        if (posX + 8 > WIDTH) break;
        drawChar(posX, y, *str);
        posX += 8;
        str++;
    }
}

void SSD1306::invertDisplay(bool invert) {
    sendCommand(invert ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
}

void SSD1306::drawBigDigit(uint8_t x, uint8_t y, uint8_t digit) {
    if (digit > 9) return;
    
    // Zeichne die große Ziffer (24x32 Pixel)
    for (uint8_t row = 0; row < big_digit_height; row++) {
        for (uint8_t byteIdx = 0; byteIdx < 3; byteIdx++) {
            uint8_t data = big_digits[digit][row][byteIdx];
            for (uint8_t bit = 0; bit < 8; bit++) {
                if (data & (0x80 >> bit)) {
                    uint8_t pixelX = x + byteIdx * 8 + bit;
                    uint8_t pixelY = y + row;
                    if (pixelX < WIDTH && pixelY < HEIGHT) {
                        setPixel(pixelX, pixelY, true);
                    }
                }
            }
        }
    }
}

void SSD1306::drawBigNumber(uint8_t x, uint8_t y, uint16_t number) {
    // Immer 3 Ziffern anzeigen (mit führenden Nullen)
    // Hunderterstelle
    uint8_t hundreds = (number / 100) % 10;
    drawBigDigit(x, y, hundreds);
    
    // Zehnerstelle
    uint8_t tens = (number / 10) % 10;
    drawBigDigit(x + big_digit_width + 2, y, tens);
    
    // Einerstelle
    uint8_t ones = number % 10;
    drawBigDigit(x + (big_digit_width + 2) * 2, y, ones);
}
