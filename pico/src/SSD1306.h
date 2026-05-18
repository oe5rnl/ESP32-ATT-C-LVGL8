#ifndef SSD1306_H
#define SSD1306_H

#include <Arduino.h>
#include <Wire.h>

class SSD1306 {
public:
    static const uint8_t WIDTH = 128;
    static const uint8_t HEIGHT = 64;
    static const uint8_t PAGES = HEIGHT / 8;
    
    SSD1306(TwoWire* wire = &Wire, uint8_t addr = 0x3C);
    
    bool init();
    void clear();
    void display();
    void setPixel(uint8_t x, uint8_t y, bool on = true);
    void drawChar(uint8_t x, uint8_t y, char c);
    void drawString(uint8_t x, uint8_t y, const char* str);
    void drawBigDigit(uint8_t x, uint8_t y, uint8_t digit);
    void drawBigNumber(uint8_t x, uint8_t y, uint16_t number);
    void invertDisplay(bool invert);
    uint8_t address() const;
    
private:
    TwoWire* _wire;
    uint8_t _addr;
    uint8_t _buffer[WIDTH * PAGES];
    
    bool probe(uint8_t addr);
    bool sendCommand(uint8_t cmd);
    bool sendData(const uint8_t* data, size_t len);
};

#endif // SSD1306_H
