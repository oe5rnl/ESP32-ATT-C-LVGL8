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

void SSD1306::sendCommand(uint8_t cmd) {
    _wire->beginTransmission(_addr);
    _wire->write(0x00); // Command mode
    _wire->write(cmd);
    _wire->endTransmission();
}

void SSD1306::sendData(const uint8_t* data, size_t len) {
    _wire->beginTransmission(_addr);
    _wire->write(0x40); // Data mode
    for (size_t i = 0; i < len; i++) {
        _wire->write(data[i]);
    }
    _wire->endTransmission();
}

void SSD1306::init() {
    // Initialisierungssequenz für 128x64 Display
    sendCommand(SSD1306_DISPLAYOFF);
    sendCommand(SSD1306_SETDISPLAYCLOCKDIV);
    sendCommand(0x80);
    sendCommand(SSD1306_SETMULTIPLEX);
    sendCommand(0x3F); // 64 Zeilen
    sendCommand(SSD1306_SETDISPLAYOFFSET);
    sendCommand(0x00);
    sendCommand(SSD1306_SETSTARTLINE | 0x00);
    sendCommand(SSD1306_CHARGEPUMP);
    sendCommand(0x14); // Charge pump aktivieren
    sendCommand(SSD1306_MEMORYMODE);
    sendCommand(0x00); // Horizontal addressing mode
    sendCommand(SSD1306_SEGREMAP | 0x01);
    sendCommand(SSD1306_COMSCANDEC);
    sendCommand(SSD1306_SETCOMPINS);
    sendCommand(0x12);
    sendCommand(SSD1306_SETCONTRAST);
    sendCommand(0xCF);
    sendCommand(SSD1306_SETPRECHARGE);
    sendCommand(0xF1);
    sendCommand(SSD1306_SETVCOMDETECT);
    sendCommand(0x40);
    sendCommand(SSD1306_DISPLAYALLON_RESUME);
    sendCommand(SSD1306_NORMALDISPLAY);
    sendCommand(SSD1306_DISPLAYON);
}

void SSD1306::clear() {
    memset(_buffer, 0, sizeof(_buffer));
}

void SSD1306::display() {
    sendCommand(SSD1306_COLUMNADDR);
    sendCommand(0);     // Column start
    sendCommand(WIDTH - 1); // Column end
    sendCommand(SSD1306_PAGEADDR);
    sendCommand(0);     // Page start
    sendCommand(PAGES - 1); // Page end
    
    // Daten in kleineren Chunks senden (I2C-Buffer-Limitierung)
    const size_t chunk_size = 16;
    for (size_t i = 0; i < sizeof(_buffer); i += chunk_size) {
        size_t remaining = sizeof(_buffer) - i;
        size_t to_send = remaining < chunk_size ? remaining : chunk_size;
        sendData(_buffer + i, to_send);
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
