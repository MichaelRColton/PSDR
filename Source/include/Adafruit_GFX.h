#ifndef _ADAFRUIT_GFX_H
#define _ADAFRUIT_GFX_H

#if ARDUINO >= 100
 //#include "Arduino.h"
 //#include "Print.h"
#else
 //#include "WProgram.h"
#endif

//#include <stm32f10x.h>
//#include <stm32f415xx.h>
#include "stm32f4xx.h"
#ifdef PSDR2
	#include "stm32f429xx.h"
#endif

#include <stdbool.h>
#include <Adafruit_ILI9340.h>
//#include <Adafruit_SSD1306.h>

#define swap(a, b) { int16_t t = a; a = b; b = t; }

//class Adafruit_GFX : public Print {

// public:

  void Adafruit_GFX(int16_t w, int16_t h); // Constructor

  // This MUST be defined by the subclass:
  /*virtual*/ void drawPixel(int16_t x, int16_t y, uint16_t color); // = 0;

  // These MAY be overridden by the subclass to provide device-specific
  // optimized code.  Otherwise 'generic' versions are used.
  /*virtual*/ void
    drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color),
    drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color),
    drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color),
    drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color),
    fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color),
    fillScreen(uint16_t color),
    invertDisplay(bool i);

  // These exist only with Adafruit_GFX (no subclass overrides)
  void
  Adafruit_GFX_drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color),
    Adafruit_GFX_drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername,
      uint16_t color),
      Adafruit_GFX_fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color),
    Adafruit_GFX_fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername,
      int16_t delta, uint16_t color),
      Adafruit_GFX_drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
      int16_t x2, int16_t y2, uint16_t color),
      Adafruit_GFX_fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
      int16_t x2, int16_t y2, uint16_t color),
      Adafruit_GFX_drawRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h,
      int16_t radius, uint16_t color),
      Adafruit_GFX_fillRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h,
      int16_t radius, uint16_t color),
      Adafruit_GFX_drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
      int16_t w, int16_t h, uint16_t color),
      Adafruit_GFX_drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color,
      uint16_t bg, uint8_t size),
      Adafruit_GFX_setCursor(int16_t x, int16_t y),
//    setTextColor(uint16_t c),
    Adafruit_GFX_setTextColor(uint16_t c, uint16_t bg),
    Adafruit_GFX_setTextSize(uint8_t s),
    Adafruit_GFX_setTextWrap(bool w),
    Adafruit_GFX_setRotation(uint8_t r);

#if ARDUINO >= 100
  virtual size_t write(uint8_t);
#else
  /*virtual*/ void   Adafruit_GFX_write(uint8_t);
#endif

  int16_t
  Adafruit_GFX_height(void),
  Adafruit_GFX_width(void);

  uint8_t Adafruit_GFX_getRotation(void);

 //protected:
   int16_t
    WIDTH, HEIGHT;   // This is the 'raw' display w/h - never changes
  int16_t
    _width, _height, // Display w/h as modified by current rotation
    cursor_x, cursor_y;
  uint16_t
    textcolor, textbgcolor;
  uint8_t
    textsize,
    rotation;
  bool
    wrap; // If set, 'wrap' text at right edge of display
//};

#endif // _ADAFRUIT_GFX_H
