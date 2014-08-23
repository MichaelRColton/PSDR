// https://github.com/PaulStoffregen/ILI9341_t3
// http://forum.pjrc.com/threads/26305-Highly-optimized-ILI9341-(320x240-TFT-color-display)-library

/***************************************************
  This is our library for the Adafruit ILI9341 Breakout and Shield
  ----> http://www.adafruit.com/products/1651

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

#include "ILI9341_t3.h"
#include <SPI.h>

// Teensy 3.1 can only generate 30 MHz SPI when running at 120 MHz (overclock)
// At all other speeds, SPI.beginTransaction() will use the fastest available clock
#define SPICLOCK 30000000

#define WIDTH  ILI9341_TFTWIDTH
#define HEIGHT ILI9341_TFTHEIGHT

// Constructor when using hardware SPI.  Faster, but must use SPI pins
// specific to each board type (e.g. 11,13 for Uno, 51,52 for Mega, etc.)
ILI9341_t3(uint8_t cs, uint8_t dc, uint8_t rst)
{
	_cs   = cs;
	_dc   = dc;
	_rst  = rst;
	_width    = WIDTH;
	_height   = HEIGHT;
	rotation  = 0;
	cursor_y  = cursor_x    = 0;
	textsize  = 1;
	textcolor = textbgcolor = 0xFFFF;
	wrap      = true;
}

void ILI9341_t3_setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
	SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
	setAddr(x0, y0, x1, y1);
	writecommand_last(ILI9341_RAMWR); // write to RAM
	SPI.endTransaction();
}

void ILI9341_t3_pushColor(uint16_t color)
{
	SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
	writedata16_last(color);
	SPI.endTransaction();
}

void ILI9341_t3_drawPixel(int16_t x, int16_t y, uint16_t color) {

	if((x < 0) ||(x >= _width) || (y < 0) || (y >= _height)) return;

	SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
	setAddr(x, y, x, y);
	writecommand_cont(ILI9341_RAMWR);
	writedata16_last(color);
	SPI.endTransaction();
}

void ILI9341_t3_drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
	// Rudimentary clipping
	if((x >= _width) || (y >= _height)) return;
	if((y+h-1) >= _height) h = _height-y;
	SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
	setAddr(x, y, x, y+h-1);
	writecommand_cont(ILI9341_RAMWR);
	while (h-- > 1) {
		writedata16_cont(color);
	}
	writedata16_last(color);
	SPI.endTransaction();
}

void ILI9341_t3_drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
	// Rudimentary clipping
	if((x >= _width) || (y >= _height)) return;
	if((x+w-1) >= _width)  w = _width-x;
	SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
	setAddr(x, y, x+w-1, y);
	writecommand_cont(ILI9341_RAMWR);
	while (w-- > 1) {
		writedata16_cont(color);
	}
	writedata16_last(color);
	SPI.endTransaction();
}

void ILI9341_t3_fillScreen(uint16_t color)
{
	fillRect(0, 0, _width, _height, color);
}

// fill a rectangle
void ILI9341_t3_fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
	// rudimentary clipping (drawChar w/big text requires this)
	if((x >= _width) || (y >= _height)) return;
	if((x + w - 1) >= _width)  w = _width  - x;
	if((y + h - 1) >= _height) h = _height - y;

	// TODO: this can result in a very long transaction time
	// should break this into multiple transactions, even though
	// it'll cost more overhead, so we don't stall other SPI libs
	SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
	setAddr(x, y, x+w-1, y+h-1);
	writecommand_cont(ILI9341_RAMWR);
	for(y=h; y>0; y--) {
		for(x=w; x>1; x--) {
			writedata16_cont(color);
		}
		writedata16_last(color);
	}
	SPI.endTransaction();
}



#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08
#define MADCTL_MH  0x04

void ILI9341_t3_setRotation(uint8_t m)
{
	writecommand_cont(ILI9341_MADCTL);
	rotation = m % 4; // can't be higher than 3
	switch (rotation) {
	case 0:
		writedata8_last(MADCTL_MX | MADCTL_BGR);
		_width  = ILI9341_TFTWIDTH;
		_height = ILI9341_TFTHEIGHT;
		break;
	case 1:
		writedata8_last(MADCTL_MV | MADCTL_BGR);
		_width  = ILI9341_TFTHEIGHT;
		_height = ILI9341_TFTWIDTH;
		break;
	case 2:
		writedata8_last(MADCTL_MY | MADCTL_BGR);
		_width  = ILI9341_TFTWIDTH;
		_height = ILI9341_TFTHEIGHT;
		break;
	case 3:
		writedata8_last(MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR);
		_width  = ILI9341_TFTHEIGHT;
		_height = ILI9341_TFTWIDTH;
		break;
	}
}


void ILI9341_t3_invertDisplay(bool i)
{
	writecommand_last(i ? ILI9341_INVON : ILI9341_INVOFF);
}










/*
uint8_t ILI9341_t3::readdata(void)
{
  uint8_t r;
       // Try to work directly with SPI registers...
       // First wait until output queue is empty
        uint16_t wTimeout = 0xffff;
        while (((SPI0.SR) & (15 << 12)) && (--wTimeout)) ; // wait until empty

//       	SPI0_MCR |= SPI_MCR_CLR_RXF; // discard any received data
//		SPI0_SR = SPI_SR_TCF;

        // Transfer a 0 out...
        writedata8_cont(0);

        // Now wait until completed.
        wTimeout = 0xffff;
        while (((SPI0.SR) & (15 << 12)) && (--wTimeout)) ; // wait until empty
        r = SPI0.POPR;  // get the received byte... should check for it first...
    return r;
}
 */


uint8_t ILI9341_t3_readcommand8(uint8_t c, uint8_t index)
{
    uint16_t wTimeout = 0xffff;
    uint8_t r=0;

    SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
    while (((SPI0.SR) & (15 << 12)) && (--wTimeout)) ; // wait until empty

    // Make sure the last frame has been sent...
    SPI0.SR = SPI_SR_TCF;   // dlear it out;
    wTimeout = 0xffff;
    while (!((SPI0.SR) & SPI_SR_TCF) && (--wTimeout)) ; // wait until it says the last frame completed

    // clear out any current received bytes
    wTimeout = 0x10;    // should not go more than 4...
    while ((((SPI0.SR) >> 4) & 0xf) && (--wTimeout))  {
        r = SPI0.POPR;
    }

    //writecommand(0xD9); // sekret command
	SPI0.PUSHR = 0xD9 | (pcs_command << 16) | SPI_PUSHR_CTAS(0) | SPI_PUSHR_CONT;
//	while (((SPI0.SR) & (15 << 12)) > (3 << 12)) ; // wait if FIFO full

    // writedata(0x10 + index);
	SPI0.PUSHR = (0x10 + index) | (pcs_data << 16) | SPI_PUSHR_CTAS(0);
//	while (((SPI0.SR) & (15 << 12)) > (3 << 12)) ; // wait if FIFO full

    // writecommand(c);
   	SPI0.PUSHR = c | (pcs_command << 16) | SPI_PUSHR_CTAS(0) | SPI_PUSHR_CONT;
//	while (((SPI0.SR) & (15 << 12)) > (3 << 12)) ; // wait if FIFO full

    // readdata
	SPI0.PUSHR = 0 | (pcs_data << 16) | SPI_PUSHR_CTAS(0);
//	while (((SPI0.SR) & (15 << 12)) > (3 << 12)) ; // wait if FIFO full

    // Now wait until completed.
    wTimeout = 0xffff;
    while (((SPI0.SR) & (15 << 12)) && (--wTimeout)) ; // wait until empty

    // Make sure the last frame has been sent...
    SPI0.SR = SPI_SR_TCF;   // dlear it out;
    wTimeout = 0xffff;
    while (!((SPI0.SR) & SPI_SR_TCF) && (--wTimeout)) ; // wait until it says the last frame completed

    wTimeout = 0x10;    // should not go more than 4...
    // lets get all of the values on the FIFO
    while ((((SPI0.SR) >> 4) & 0xf) && (--wTimeout))  {
        r = SPI0.POPR;
    }
    SPI.endTransaction();
    return r;  // get the received byte... should check for it first...
}


// KJE Added functions to read pixel data...
uint16_t ILI9341_t3_readPixel(int16_t x, int16_t y)
{
    SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
  writecommand_cont(ILI9341_CASET); // Column addr set
  writedata16_cont(x);  // XSTART
  x++;
  writedata16_cont(x);  // XEND

  writecommand_cont(ILI9341_PASET); // Row addr set
  writedata16_cont(y);     // YSTART
  y++;
  writedata16_cont(y);     // YEND

  writecommand_cont(ILI9341_RAMRD); // write to RAM

  digitalWrite(_dc, HIGH);
  digitalWrite(_cs, LOW);
  uint16_t r = SPI.transfer(0x00);
  r <<= 8;
  r |= SPI.transfer(0x00);
  digitalWrite(_cs, HIGH);
    SPI.endTransaction();
  return r;
}

static const uint8_t init_commands[] = {
	4, 0xEF, 0x03, 0x80, 0x02,
	4, 0xCF, 0x00, 0XC1, 0X30,
	5, 0xED, 0x64, 0x03, 0X12, 0X81,
	4, 0xE8, 0x85, 0x00, 0x78,
	6, 0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02,
	2, 0xF7, 0x20,
	3, 0xEA, 0x00, 0x00,
	2, ILI9341_PWCTR1, 0x23, // Power control
	2, ILI9341_PWCTR2, 0x10, // Power control
	3, ILI9341_VMCTR1, 0x3e, 0x28, // VCM control
	2, ILI9341_VMCTR2, 0x86, // VCM control2
	2, ILI9341_MADCTL, 0x48, // Memory Access Control
	2, ILI9341_PIXFMT, 0x55,
	3, ILI9341_FRMCTR1, 0x00, 0x18,
	4, ILI9341_DFUNCTR, 0x08, 0x82, 0x27, // Display Function Control
	2, 0xF2, 0x00, // Gamma Function Disable
	2, ILI9341_GAMMASET, 0x01, // Gamma curve selected
	16, ILI9341_GMCTRP1, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08,
		0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00, // Set Gamma
	16, ILI9341_GMCTRN1, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07,
		0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F, // Set Gamma
	0
};

void ILI9341_t3_begin(void)
{
	SPI.begin();
	if (SPI.pinIsChipSelect(_cs, _dc)) {
		pcs_data = SPI.setCS(_cs);
		pcs_command = pcs_data | SPI.setCS(_dc);
	} else {
		pcs_data = 0;
		pcs_command = 0;
		return;
	}
	// toggle RST low to reset
	if (_rst < 255) {
		pinMode(_rst, OUTPUT);
		digitalWrite(_rst, HIGH);
		delay(5);
		digitalWrite(_rst, LOW);
		delay(20);
		digitalWrite(_rst, HIGH);
		delay(150);
	}
	/*
	uint8_t x = readcommand8(ILI9341_RDMODE);
	Serial.print("\nDisplay Power Mode: 0x"); Serial.println(x, HEX);
	x = readcommand8(ILI9341_RDMADCTL);
	Serial.print("\nMADCTL Mode: 0x"); Serial.println(x, HEX);
	x = readcommand8(ILI9341_RDPIXFMT);
	Serial.print("\nPixel Format: 0x"); Serial.println(x, HEX);
	x = readcommand8(ILI9341_RDIMGFMT);
	Serial.print("\nImage Format: 0x"); Serial.println(x, HEX);
	x = readcommand8(ILI9341_RDSELFDIAG);
	Serial.print("\nSelf Diagnostic: 0x"); Serial.println(x, HEX);
	*/
	SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
	const uint8_t *addr = init_commands;
	while (1) {
		uint8_t count = *addr++;
		if (count-- == 0) break;
		writecommand_cont(*addr++);
		while (count-- > 0) {
			writedata8_cont(*addr++);
		}
	}
	writecommand_last(ILI9341_SLPOUT);    // Exit Sleep
	SPI.endTransaction();

	delay(120);
	SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
	writecommand_last(ILI9341_DISPON);    // Display on
	SPI.endTransaction();
}




/*
This is the core graphics library for all our displays, providing a common
set of graphics primitives (points, lines, circles, etc.).  It needs to be
paired with a hardware-specific library for each display device we carry
(to handle the lower-level functions).

Adafruit invests time and resources providing this open source code, please
support Adafruit & open-source hardware by purchasing products from Adafruit!

Copyright (c) 2013 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include "glcdfont.c"

// Draw a circle outline
void ILI9341_t3_drawCircle(int16_t x0, int16_t y0, int16_t r,
    uint16_t color) {
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  drawPixel(x0  , y0+r, color);
  drawPixel(x0  , y0-r, color);
  drawPixel(x0+r, y0  , color);
  drawPixel(x0-r, y0  , color);

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    drawPixel(x0 + x, y0 + y, color);
    drawPixel(x0 - x, y0 + y, color);
    drawPixel(x0 + x, y0 - y, color);
    drawPixel(x0 - x, y0 - y, color);
    drawPixel(x0 + y, y0 + x, color);
    drawPixel(x0 - y, y0 + x, color);
    drawPixel(x0 + y, y0 - x, color);
    drawPixel(x0 - y, y0 - x, color);
  }
}

void ILI9341_t3_drawCircleHelper( int16_t x0, int16_t y0,
               int16_t r, uint8_t cornername, uint16_t color) {
  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x     = 0;
  int16_t y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;
    if (cornername & 0x4) {
      drawPixel(x0 + x, y0 + y, color);
      drawPixel(x0 + y, y0 + x, color);
    }
    if (cornername & 0x2) {
      drawPixel(x0 + x, y0 - y, color);
      drawPixel(x0 + y, y0 - x, color);
    }
    if (cornername & 0x8) {
      drawPixel(x0 - y, y0 + x, color);
      drawPixel(x0 - x, y0 + y, color);
    }
    if (cornername & 0x1) {
      drawPixel(x0 - y, y0 - x, color);
      drawPixel(x0 - x, y0 - y, color);
    }
  }
}

void ILI9341_t3_fillCircle(int16_t x0, int16_t y0, int16_t r,
			      uint16_t color) {
  drawFastVLine(x0, y0-r, 2*r+1, color);
  fillCircleHelper(x0, y0, r, 3, 0, color);
}

// Used to do circles and roundrects
void ILI9341_t3_fillCircleHelper(int16_t x0, int16_t y0, int16_t r,
    uint8_t cornername, int16_t delta, uint16_t color) {

  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x     = 0;
  int16_t y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;

    if (cornername & 0x1) {
      drawFastVLine(x0+x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0+y, y0-x, 2*x+1+delta, color);
    }
    if (cornername & 0x2) {
      drawFastVLine(x0-x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0-y, y0-x, 2*x+1+delta, color);
    }
  }
}


// Bresenham's algorithm - thx wikpedia
void ILI9341_t3_drawLine(int16_t x0, int16_t y0,
	int16_t x1, int16_t y1, uint16_t color)
{
	if (y0 == y1) {
		if (x1 > x0) {
			drawFastHLine(x0, y0, x1 - x0 + 1, color);
		} else if (x1 < x0) {
			drawFastHLine(x1, y0, x0 - x1 + 1, color);
		} else {
			drawPixel(x0, y0, color);
		}
		return;
	} else if (x0 == x1) {
		if (y1 > y0) {
			drawFastVLine(x0, y0, y1 - y0 + 1, color);
		} else {
			drawFastVLine(x0, y1, y0 - y1 + 1, color);
		}
		return;
	}

	bool steep = abs(y1 - y0) > abs(x1 - x0);
	if (steep) {
		swap(x0, y0);
		swap(x1, y1);
	}
	if (x0 > x1) {
		swap(x0, x1);
		swap(y0, y1);
	}

	int16_t dx, dy;
	dx = x1 - x0;
	dy = abs(y1 - y0);

	int16_t err = dx / 2;
	int16_t ystep;

	if (y0 < y1) {
		ystep = 1;
	} else {
		ystep = -1;
	}

	SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
	int16_t xbegin = x0;
	if (steep) {
		for (; x0<=x1; x0++) {
			err -= dy;
			if (err < 0) {
				int16_t len = x0 - xbegin;
				if (len) {
					VLine(y0, xbegin, len + 1, color);
				} else {
					Pixel(y0, x0, color);
				}
				xbegin = x0 + 1;
				y0 += ystep;
				err += dx;
			}
		}
		if (x0 > xbegin + 1) {
			VLine(y0, xbegin, x0 - xbegin, color);
		}

	} else {
		for (; x0<=x1; x0++) {
			err -= dy;
			if (err < 0) {
				int16_t len = x0 - xbegin;
				if (len) {
					HLine(xbegin, y0, len + 1, color);
				} else {
					Pixel(x0, y0, color);
				}
				xbegin = x0 + 1;
				y0 += ystep;
				err += dx;
			}
		}
		if (x0 > xbegin + 1) {
			HLine(xbegin, y0, x0 - xbegin, color);
		}
	}
	writecommand_last(ILI9341_NOP);
	SPI.endTransaction();
}

// Draw a rectangle
void ILI9341_t3_drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
	SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
	HLine(x, y, w, color);
	HLine(x, y+h-1, w, color);
	VLine(x, y, h, color);
	VLine(x+w-1, y, h, color);
	writecommand_last(ILI9341_NOP);
	SPI.endTransaction();
}

// Draw a rounded rectangle
void ILI9341_t3_drawRoundRect(int16_t x, int16_t y, int16_t w,
  int16_t h, int16_t r, uint16_t color) {
  // smarter version
  drawFastHLine(x+r  , y    , w-2*r, color); // Top
  drawFastHLine(x+r  , y+h-1, w-2*r, color); // Bottom
  drawFastVLine(x    , y+r  , h-2*r, color); // Left
  drawFastVLine(x+w-1, y+r  , h-2*r, color); // Right
  // draw four corners
  drawCircleHelper(x+r    , y+r    , r, 1, color);
  drawCircleHelper(x+w-r-1, y+r    , r, 2, color);
  drawCircleHelper(x+w-r-1, y+h-r-1, r, 4, color);
  drawCircleHelper(x+r    , y+h-r-1, r, 8, color);
}

// Fill a rounded rectangle
void ILI9341_t3_fillRoundRect(int16_t x, int16_t y, int16_t w,
				 int16_t h, int16_t r, uint16_t color) {
  // smarter version
  fillRect(x+r, y, w-2*r, h, color);

  // draw four corners
  fillCircleHelper(x+w-r-1, y+r, r, 1, h-2*r-1, color);
  fillCircleHelper(x+r    , y+r, r, 2, h-2*r-1, color);
}

// Draw a triangle
void ILI9341_t3_drawTriangle(int16_t x0, int16_t y0,
				int16_t x1, int16_t y1,
				int16_t x2, int16_t y2, uint16_t color) {
  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color);
}

// Fill a triangle
void ILI9341_t3_fillTriangle ( int16_t x0, int16_t y0,
				  int16_t x1, int16_t y1,
				  int16_t x2, int16_t y2, uint16_t color) {

  int16_t a, b, y, last;

  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1) {
    swap(y0, y1); swap(x0, x1);
  }
  if (y1 > y2) {
    swap(y2, y1); swap(x2, x1);
  }
  if (y0 > y1) {
    swap(y0, y1); swap(x0, x1);
  }

  if(y0 == y2) { // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if(x1 < a)      a = x1;
    else if(x1 > b) b = x1;
    if(x2 < a)      a = x2;
    else if(x2 > b) b = x2;
    drawFastHLine(a, y0, b-a+1, color);
    return;
  }

  int16_t
    dx01 = x1 - x0,
    dy01 = y1 - y0,
    dx02 = x2 - x0,
    dy02 = y2 - y0,
    dx12 = x2 - x1,
    dy12 = y2 - y1,
    sa   = 0,
    sb   = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if(y1 == y2) last = y1;   // Include y1 scanline
  else         last = y1-1; // Skip it

  for(y=y0; y<=last; y++) {
    a   = x0 + sa / dy01;
    b   = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
    a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) swap(a,b);
    drawFastHLine(a, y, b-a+1, color);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for(; y<=y2; y++) {
    a   = x1 + sa / dy12;
    b   = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
    a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) swap(a,b);
    drawFastHLine(a, y, b-a+1, color);
  }
}

void ILI9341_t3_drawBitmap(int16_t x, int16_t y,
			      const uint8_t *bitmap, int16_t w, int16_t h,
			      uint16_t color) {

  int16_t i, j, byteWidth = (w + 7) / 8;

  for(j=0; j<h; j++) {
    for(i=0; i<w; i++ ) {
      if(pgm_read_byte(bitmap + j * byteWidth + i / 8) & (128 >> (i & 7))) {
	drawPixel(x+i, y+j, color);
      }
    }
  }
}

size_t ILI9341_t3_write(uint8_t c) {
  if (c == '\n') {
    cursor_y += textsize*8;
    cursor_x  = 0;
  } else if (c == '\r') {
    // skip em
  } else {
    drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize);
    cursor_x += textsize*6;
    if (wrap && (cursor_x > (_width - textsize*6))) {
      cursor_y += textsize*8;
      cursor_x = 0;
    }
  }
  return 1;
}

// Draw a character
void ILI9341_t3_drawChar(int16_t x, int16_t y, unsigned char c,
			    uint16_t fgcolor, uint16_t bgcolor, uint8_t size)
{
	if((x >= _width)            || // Clip right
	   (y >= _height)           || // Clip bottom
	   ((x + 6 * size - 1) < 0) || // Clip left  TODO: is this correct?
	   ((y + 8 * size - 1) < 0))   // Clip top   TODO: is this correct?
		return;

	if (fgcolor == bgcolor) {
		// This transparent approach is only about 20% faster
		if (size == 1) {
			uint8_t mask = 0x01;
			int16_t xoff, yoff;
			for (yoff=0; yoff < 8; yoff++) {
				uint8_t line = 0;
				for (xoff=0; xoff < 5; xoff++) {
					if (font[c * 5 + xoff] & mask) line |= 1;
					line <<= 1;
				}
				line >>= 1;
				xoff = 0;
				while (line) {
					if (line == 0x1F) {
						drawFastHLine(x + xoff, y + yoff, 5, fgcolor);
						break;
					} else if (line == 0x1E) {
						drawFastHLine(x + xoff, y + yoff, 4, fgcolor);
						break;
					} else if ((line & 0x1C) == 0x1C) {
						drawFastHLine(x + xoff, y + yoff, 3, fgcolor);
						line <<= 4;
						xoff += 4;
					} else if ((line & 0x18) == 0x18) {
						drawFastHLine(x + xoff, y + yoff, 2, fgcolor);
						line <<= 3;
						xoff += 3;
					} else if ((line & 0x10) == 0x10) {
						drawPixel(x + xoff, y + yoff, fgcolor);
						line <<= 2;
						xoff += 2;
					} else {
						line <<= 1;
						xoff += 1;
					}
				}
				mask = mask << 1;
			}
		} else {
			uint8_t mask = 0x01;
			int16_t xoff, yoff;
			for (yoff=0; yoff < 8; yoff++) {
				uint8_t line = 0;
				for (xoff=0; xoff < 5; xoff++) {
					if (font[c * 5 + xoff] & mask) line |= 1;
					line <<= 1;
				}
				line >>= 1;
				xoff = 0;
				while (line) {
					if (line == 0x1F) {
						fillRect(x + xoff * size, y + yoff * size,
							5 * size, size, fgcolor);
						break;
					} else if (line == 0x1E) {
						fillRect(x + xoff * size, y + yoff * size,
							4 * size, size, fgcolor);
						break;
					} else if ((line & 0x1C) == 0x1C) {
						fillRect(x + xoff * size, y + yoff * size,
							3 * size, size, fgcolor);
						line <<= 4;
						xoff += 4;
					} else if ((line & 0x18) == 0x18) {
						fillRect(x + xoff * size, y + yoff * size,
							2 * size, size, fgcolor);
						line <<= 3;
						xoff += 3;
					} else if ((line & 0x10) == 0x10) {
						fillRect(x + xoff * size, y + yoff * size,
							size, size, fgcolor);
						line <<= 2;
						xoff += 2;
					} else {
						line <<= 1;
						xoff += 1;
					}
				}
				mask = mask << 1;
			}
		}
	} else {
		// This solid background approach is about 5 time faster
		SPI.beginTransaction(SPISettings(SPICLOCK, MSBFIRST, SPI_MODE0));
		setAddr(x, y, x + 6 * size - 1, y + 8 * size);
		writecommand_cont(ILI9341_RAMWR);
		uint8_t xr, yr;
		uint8_t mask = 0x01;
		uint16_t color;
		for (y=0; y < 8; y++) {
			for (yr=0; yr < size; yr++) {
				for (x=0; x < 5; x++) {
					if (font[c * 5 + x] & mask) {
						color = fgcolor;
					} else {
						color = bgcolor;
					}
					for (xr=0; xr < size; xr++) {
						writedata16_cont(color);
					}
				}
				for (xr=0; xr < size; xr++) {
					writedata16_cont(bgcolor);
				}
			}
			mask = mask << 1;
		}
		uint32_t n = 6 * size * size;
		do {
			writedata16_cont(bgcolor);
			n--;
		} while (n > 1);
		writedata16_last(bgcolor);
		SPI.endTransaction();
	}
}

void ILI9341_t3_setCursor(int16_t x, int16_t y) {
  cursor_x = x;
  cursor_y = y;
}

void ILI9341_t3_setTextSize(uint8_t s) {
  textsize = (s > 0) ? s : 1;
}

void ILI9341_t3_setTextColor(uint16_t c) {
  // For 'transparent' background, we'll set the bg
  // to the same as fg instead of using a flag
  textcolor = textbgcolor = c;
}

void ILI9341_t3_setTextColor(uint16_t c, uint16_t b) {
  textcolor   = c;
  textbgcolor = b;
}

void ILI9341_t3_setTextWrap(bool w) {
  wrap = w;
}

uint8_t ILI9341_t3_getRotation(void) {
  return rotation;
}


