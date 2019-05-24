/***************************************************
 This is an Arduino Library for the Adafruit 2.2" SPI display.
 This library works with the Adafruit 2.2" TFT Breakout w/SD card
 ----> http://www.adafruit.com/products/1480

 Check out the links above for our tutorials and wiring diagrams
 These displays use SPI to communicate, 4 or 5 pins are required to
 interface (RST is optional)
 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 Written by Limor Fried/Ladyada for Adafruit Industries.
 MIT license, all text above must be included in any redistribution
 ****************************************************/

/****************************************************
 * Changed pretty drastically by me, Michael Colton, to suit the STM32
 * and the way I want to use it, most notably by using DMA
 *
 * Thanks Ladyada for giving me a working base to build from!
 ****************************************************/

#ifndef _ADAFRUIT_ILI9340H_
#define _ADAFRUIT_ILI9340H_

//#include <stm32f10x.h>
//#include <stm32f415xx.h>
#ifdef PSDR2
#include "stm32f429xx.h"
#endif

#ifdef PSDR257
#include "stm32f746xx.h"
#endif
#include <stdbool.h>
//#include <Adafruit_GFX.h>
#include <main.h>

#define ILI9340_TFTWIDTH  240
#define ILI9340_TFTHEIGHT 320

#define ILI9340_NOP     0x00
#define ILI9340_SWRESET 0x01
#define ILI9340_RDDID   0x04
#define ILI9340_RDDST   0x09

#define ILI9340_SLPIN   0x10
#define ILI9340_SLPOUT  0x11
#define ILI9340_PTLON   0x12
#define ILI9340_NORON   0x13

#define ILI9340_RDMODE  0x0A
#define ILI9340_RDMADCTL  0x0B
#define ILI9340_RDPIXFMT  0x0C
#define ILI9340_RDIMGFMT  0x0A
#define ILI9340_RDSELFDIAG  0x0F

#define ILI9340_INVOFF  0x20
#define ILI9340_INVON   0x21
#define ILI9340_GAMMASET 0x26
#define ILI9340_DISPOFF 0x28
#define ILI9340_DISPON  0x29

#define ILI9340_CASET   0x2A
#define ILI9340_PASET   0x2B
#define ILI9340_RAMWR   0x2C
#define ILI9340_RAMRD   0x2E

#define ILI9340_RAMWRCNT 0x3C

#define ILI9340_PTLAR   0x30
#define ILI9340_MADCTL  0x36

#define ILI9340_MADCTL_MY  0x80
#define ILI9340_MADCTL_MX  0x40
#define ILI9340_MADCTL_MV  0x20
#define ILI9340_MADCTL_ML  0x10
#define ILI9340_MADCTL_RGB 0x00
#define ILI9340_MADCTL_BGR 0x08
#define ILI9340_MADCTL_MH  0x04

#define ILI9340_PIXFMT  0x3A

#define ILI9340_FRMCTR1 0xB1
#define ILI9340_FRMCTR2 0xB2
#define ILI9340_FRMCTR3 0xB3
#define ILI9340_INVCTR  0xB4
#define ILI9340_DFUNCTR 0xB6

#define ILI9340_PWCTR1  0xC0
#define ILI9340_PWCTR2  0xC1
#define ILI9340_PWCTR3  0xC2
#define ILI9340_PWCTR4  0xC3
#define ILI9340_PWCTR5  0xC4
#define ILI9340_VMCTR1  0xC5
#define ILI9340_VMCTR2  0xC7

#define ILI9340_RDID1   0xDA
#define ILI9340_RDID2   0xDB
#define ILI9340_RDID3   0xDC
#define ILI9340_RDID4   0xDD

#define ILI9340_GMCTRP1 0xE0
#define ILI9340_GMCTRN1 0xE1

#define ILI9340_VSCRDEF 0x33
#define ILI9340_VSCRSADD 0x37
/*
 #define ILI9340_PWCTR6  0xFC

 */

// Color definitions
#define	ILI9340_BLACK   0x0000
#define	ILI9340_BLUE    0x001F
#define	ILI9340_RED     0xF800
#define	ILI9340_GREEN   0x07E0
#define ILI9340_CYAN    0x07FF
#define ILI9340_MAGENTA 0xF81F
#define ILI9340_YELLOW  0xFFE0
#define ILI9340_WHITE   0xFFFF

//class Adafruit_ILI9340 : public Adafruit_GFX {
//
// public:

//  Adafruit_ILI9340(uint8_t CS, uint8_t RS, uint8_t MOSI, uint8_t SCLK,
//		   uint8_t RST, uint8_t MISO);
void
Adafruit_ILI9340 (uint8_t CS, uint8_t RS, uint8_t RST);

void
Adafruit_ILI9340_begin (void);
void
Adafruit_ILI9340_setAddrWindow (uint16_t x0, uint16_t y0, uint16_t x1,
                                uint16_t y1);
void
Adafruit_ILI9340_pushColor (uint16_t color);
void
Adafruit_ILI9340_fillScreen (uint16_t color);
void
Adafruit_ILI9340_drawPixel (int16_t x, int16_t y, uint16_t color);
void
Adafruit_ILI9340_drawNextPixel (uint16_t color);
void
Adafruit_ILI9340_drawFastVLine (int16_t x, int16_t y, int16_t h,
                                uint16_t color);
void
Adafruit_ILI9340_drawFastHLine (int16_t x, int16_t y, int16_t w,
                                uint16_t color);
void
Adafruit_ILI9340_fillRect (int16_t x, int16_t y, int16_t w, int16_t h,
                           uint16_t color);
void
Adafruit_ILI9340_setRotation (uint8_t r);
void
Adafruit_ILI9340_invertDisplay (bool i);
uint16_t
Adafruit_ILI9340_Color565 (uint8_t r, uint8_t g, uint8_t b);

/* These are not for current use, 8-bit protocol only! */
uint8_t
Adafruit_ILI9340_readdata (void),
Adafruit_ILI9340_readcommand8 (uint8_t);
/*
 uint16_t readcommand16(uint8_t);
 uint32_t readcommand32(uint8_t);
 void     dummyclock(void);
 */

void
Adafruit_ILI9340_spiwrite (uint8_t),
Adafruit_ILI9340_writecommand (uint16_t c),
Adafruit_ILI9340_writedata (uint16_t d),
Adafruit_ILI9340_commandList (uint8_t *addr);
uint8_t
Adafruit_ILI9340_spiread (void);

// private:
//  uint8_t  tabcolor;

bool hwSPI;
#ifdef __AVR__
volatile uint8_t *mosiport, *clkport, *dcport, *rsport, *csport;
uint8_t _cs, _dc, _rst, _mosi, _miso, _sclk,
mosipinmask, clkpinmask, cspinmask, dcpinmask;
#endif //  #ifdef __AVR__
#if defined(__SAM3X8E__)
Pio *mosiport, *clkport, *dcport, *rsport, *csport;
uint32_t _cs, _dc, _rst, _mosi, _miso, _sclk,
mosipinmask, clkpinmask, cspinmask, dcpinmask;
#endif //  #if defined(__SAM3X8E__)
#if defined(__arm__) && defined(CORE_TEENSY)
volatile uint8_t *mosiport, *clkport, *dcport, *rsport, *csport;
uint8_t _cs, _dc, _rst, _mosi, _miso, _sclk,
mosipinmask, clkpinmask, cspinmask, dcpinmask;
#endif
//};

#endif

/* Function declarations to keep the compiler happy */
void
Adafruit_ILI9340_setVerticalScrollDefinition (uint16_t topFixedArea,
                                              uint16_t verticalScrollArea,
                                              uint16_t bottomFixedArea);
void
Adafruit_ILI9340_setVerticalScrollStartAddress (uint16_t address);

