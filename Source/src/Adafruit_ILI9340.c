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

#include "Adafruit_ILI9340.h"
//#include <avr/pgmspace.h>
//#include <limits.h>
//#include "pins_arduino.h"
//#include "wiring_private.h"
#include <spi.h>
#include <hal.h>
#include <stdbool.h>
//#include <stm32f4xx_hal.h>

#include "stm32f4xx_hal_spi.h"

#if defined(__SAM3X8E__)
#include <include/pio.h>
  #define SET_BIT(port, bitMask) (port)->PIO_SODR |= (bitMask)
  #define CLEAR_BIT(port, bitMask) (port)->PIO_CODR |= (bitMask)
  #define USE_SPI_LIBRARY
#endif
#ifdef __AVR__
  #define SET_BIT(port, bitMask) *(port) |= (bitMask)
  #define CLEAR_BIT(port, bitMask) *(port) &= ~(bitMask)
#endif
#if defined(__arm__) && defined(CORE_TEENSY)
  #define USE_SPI_LIBRARY
  #define SET_BIT(port, bitMask) digitalWrite(*(port), HIGH);
  #define CLEAR_BIT(port, bitMask) digitalWrite(*(port), LOW);
#endif

// Constructor when using software SPI.  All output pins are configurable.
//Adafruit_ILI9340_Adafruit_ILI9340(uint8_t cs, uint8_t dc, uint8_t mosi,
//				   uint8_t sclk, uint8_t rst, uint8_t miso) : Adafruit_GFX(ILI9340_TFTWIDTH, ILI9340_TFTHEIGHT) {
//  _cs   = cs;
//  _dc   = dc;
//  _mosi  = mosi;
//  _miso = miso;
//  _sclk = sclk;
//  _rst  = rst;
//  hwSPI = false;
//}

//   int  _width = ILI9340_TFTWIDTH; //240;
//   int  _height = ILI9340_TFTHEIGHT; //320
	uint8_t rxBuf[10];
	uint8_t txBuf[2];

// Constructor when using hardware SPI.  Faster, but must use SPI pins
// specific to each board type (e.g. 11,13 for Uno, 51,52 for Mega, etc.)
void Adafruit_ILI9340_Adafruit_ILI9340(uint8_t cs, uint8_t dc, uint8_t rst) /*: Adafruit_GFX(ILI9340_TFTWIDTH, ILI9340_TFTHEIGHT) */ {
  //_cs   = cs;
  //_dc   = dc;
  //_rst  = rst;
  hwSPI = true;
  //_mosi  = _sclk = 0;
}

void Adafruit_ILI9340_spiwrite(uint8_t c) {

  //Serial.print("0x"); Serial.print(c, HEX); Serial.print(", ");

  if (hwSPI) {
#ifdef __AVR__
    SPDR = c;
    while(!(SPSR & _BV(SPIF)));
#endif
#if defined(USE_SPI_LIBRARY)
    SPI.transfer(c);
#endif
  } else {
    // Fast SPI bitbang swiped from LPD8806 library
	  uint8_t bit;
    for(bit = 0x80; bit; bit >>= 1) {
      if(c & bit) {
        //digitalWrite(_mosi, HIGH);
        //SET_BIT(mosiport, mosipinmask);
    	  HAL_GPIO_WritePin(SPI1_MOSI.port, SPI1_MOSI.pin, 1);
      } else {
        //digitalWrite(_mosi, LOW);
        //CLEAR_BIT(mosiport, mosipinmask);
    	  HAL_GPIO_WritePin(SPI1_MOSI.port, SPI1_MOSI.pin, 0);
      }
      //digitalWrite(_sclk, HIGH);
      //SET_BIT(clkport, clkpinmask);
      HAL_GPIO_WritePin(SPI1_SCK.port, SPI1_SCK.pin, 1);
      //digitalWrite(_sclk, LOW);
      //CLEAR_BIT(clkport, clkpinmask);
      HAL_GPIO_WritePin(SPI1_SCK.port, SPI1_SCK.pin, 0);
    }
  }
}


void Adafruit_ILI9340_writecommand(uint8_t c) {
  //CLEAR_BIT(dcport, dcpinmask);
	HAL_GPIO_WritePin(LCD_DC.port, LCD_DC.pin, 0);
  //digitalWrite(_dc, LOW);
  //CLEAR_BIT(clkport, clkpinmask);
	HAL_GPIO_WritePin(SPI1_SCK.port, SPI1_SCK.pin, 0);
  //digitalWrite(_sclk, LOW);
  //CLEAR_BIT(csport, cspinmask);
	HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 0);
  //digitalWrite(_cs, LOW);

  //spiwrite(c);
	txBuf[0] = c;
	spi_readWrite(SpiHandle, rxBuf, txBuf, 1);
	//HAL_SPI_TransmitReceive(&SpiHandle, (uint8_t*)txBuf, (uint8_t *)rxBuf, 1, 1000);

  //SET_BIT(csport, cspinmask);
	HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 1);
  //digitalWrite(_cs, HIGH);
}


void Adafruit_ILI9340_writedata(uint8_t c) {
  //SET_BIT(dcport,  dcpinmask);
	HAL_GPIO_WritePin(LCD_DC.port, LCD_DC.pin, 1);
  //digitalWrite(_dc, HIGH);
  //CLEAR_BIT(clkport, clkpinmask);
	HAL_GPIO_WritePin(SPI1_SCK.port, SPI1_SCK.pin, 0);
  //digitalWrite(_sclk, LOW);
  //CLEAR_BIT(csport, cspinmask);
	HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 0);
  //digitalWrite(_cs, LOW);

  //spiwrite(c);
	txBuf[0] = c;
	spi_readWrite(SpiHandle, rxBuf, txBuf, 1);
	//HAL_SPI_TransmitReceive(&SpiHandle, (uint8_t*)txBuf, (uint8_t *)rxBuf, 1, 1000);

  //digitalWrite(_cs, HIGH);
  //SET_BIT(csport, cspinmask);
	HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 1);
}

// Rather than a bazillion writecommand() and writedata() calls, screen
// initialization commands and arguments are organized in these tables
// stored in PROGMEM.  The table may look bulky, but that's mostly the
// formatting -- storage-wise this is hundreds of bytes more compact
// than the equivalent code.  Companion function follows.
#define DELAY 0x80

// Companion code to the above tables.  Reads and issues
// a series of LCD commands stored in PROGMEM byte array.
//void Adafruit_ILI9340_commandList(uint8_t *addr) {
//
//  uint8_t  numCommands, numArgs;
//  uint16_t ms;
//
//  numCommands = pgm_read_byte(addr++);   // Number of commands to follow
//  while(numCommands--) {                 // For each command...
//    writecommand(pgm_read_byte(addr++)); //   Read, issue command
//    numArgs  = pgm_read_byte(addr++);    //   Number of args to follow
//    ms       = numArgs & DELAY;          //   If hibit set, delay follows args
//    numArgs &= ~DELAY;                   //   Mask out delay bit
//    while(numArgs--) {                   //   For each argument...
//      writedata(pgm_read_byte(addr++));  //     Read, issue argument
//    }
//
//    if(ms) {
//      ms = pgm_read_byte(addr++); // Read post-command delay time (ms)
//      if(ms == 255) ms = 500;     // If 255, delay for 500 ms
//      hal_delay_ms(ms);
//    }
//  }
//}


void Adafruit_ILI9340_begin(void) {
  //pinMode(_rst, OUTPUT);
  //digitalWrite(_rst, LOW);
  //pinMode(_dc, OUTPUT);
  //pinMode(_cs, OUTPUT);
//#ifdef __AVR__
//  csport    = portOutputRegister(digitalPinToPort(_cs));
//  dcport    = portOutputRegister(digitalPinToPort(_dc));
//#endif
//#if defined(__SAM3X8E__)
//  csport    = digitalPinToPort(_cs);
//  dcport    = digitalPinToPort(_dc);
//#endif
//#if defined(__arm__) && defined(CORE_TEENSY)
//  mosiport = &_mosi;
//  clkport = &_sclk;
//  rsport = &_rst;
//  csport    = &_cs;
//  dcport    = &_dc;
//#endif
  //cspinmask = digitalPinToBitMask(_cs);
  //dcpinmask = digitalPinToBitMask(_dc);

//  if(hwSPI) { // Using hardware SPI
//    SPI.begin();
//#ifdef __AVR__
//    SPI.setClockDivider(SPI_CLOCK_DIV2); // 8 MHz (full! speed!)
//#endif
//#if defined(__SAM3X8E__)
//    SPI.setClockDivider(11); // 85MHz / 11 = 7.6 MHz (full! speed!)
//#endif    SPI.setBitOrder(MSBFIRST);
//    SPI.setBitOrder(MSBFIRST);
//    SPI.setDataMode(SPI_MODE0);
//  } else {
//    pinMode(_sclk, OUTPUT);
//    pinMode(_mosi, OUTPUT);
//    pinMode(_miso, INPUT);
//#ifdef __AVR__
//    clkport     = portOutputRegister(digitalPinToPort(_sclk));
//    mosiport    = portOutputRegister(digitalPinToPort(_mosi));
//#endif
//#if defined(__SAM3X8E__)
//    clkport     = digitalPinToPort(_sclk);
//    mosiport    = digitalPinToPort(_mosi);
//#endif
//    clkpinmask  = digitalPinToBitMask(_sclk);
//    mosipinmask = digitalPinToBitMask(_mosi);
//    CLEAR_BIT(clkport, clkpinmask);
//    CLEAR_BIT(mosiport, mosipinmask);
  //}

  // toggle RST low to reset

  //digitalWrite(_rst, HIGH);
	HAL_GPIO_WritePin(LCD_RESET.port, LCD_RESET.pin, 1);
  hal_delay_ms(5);
  //digitalWrite(_rst, LOW);
  HAL_GPIO_WritePin(LCD_RESET.port, LCD_RESET.pin, 0);
  hal_delay_ms(20);
  //digitalWrite(_rst, HIGH);
  HAL_GPIO_WritePin(LCD_RESET.port, LCD_RESET.pin, 1);
  hal_delay_ms(150);

  HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 0); //I'm going to try leaving this low the WHOLE TIME!

  /*
  uint8_t x = readcommand8(ILI9340_RDMODE);
  Serial.print("\nDisplay Power Mode: 0x"); Serial.println(x, HEX);
  x = readcommand8(ILI9340_RDMADCTL);
  Serial.print("\nMADCTL Mode: 0x"); Serial.println(x, HEX);
  x = readcommand8(ILI9340_RDPIXFMT);
  Serial.print("\nPixel Format: 0x"); Serial.println(x, HEX);
  x = readcommand8(ILI9340_RDIMGFMT);
  Serial.print("\nImage Format: 0x"); Serial.println(x, HEX);
  x = readcommand8(ILI9340_RDSELFDIAG);
  Serial.print("\nSelf Diagnostic: 0x"); Serial.println(x, HEX);
  */

  //if(cmdList) commandList(cmdList);

  Adafruit_ILI9340_writecommand(0xEF);
  Adafruit_ILI9340_writedata(0x03);
  Adafruit_ILI9340_writedata(0x80);
  Adafruit_ILI9340_writedata(0x02);

  Adafruit_ILI9340_writecommand(0xCF);
  Adafruit_ILI9340_writedata(0x00);
  Adafruit_ILI9340_writedata(0XC1);
  Adafruit_ILI9340_writedata(0X30);

  Adafruit_ILI9340_writecommand(0xED);
  Adafruit_ILI9340_writedata(0x64);
  Adafruit_ILI9340_writedata(0x03);
  Adafruit_ILI9340_writedata(0X12);
  Adafruit_ILI9340_writedata(0X81);

  Adafruit_ILI9340_writecommand(0xE8);
  Adafruit_ILI9340_writedata(0x85);
  Adafruit_ILI9340_writedata(0x00);
  Adafruit_ILI9340_writedata(0x78);

  Adafruit_ILI9340_writecommand(0xCB);
  Adafruit_ILI9340_writedata(0x39);
  Adafruit_ILI9340_writedata(0x2C);
  Adafruit_ILI9340_writedata(0x00);
  Adafruit_ILI9340_writedata(0x34);
  Adafruit_ILI9340_writedata(0x02);

  Adafruit_ILI9340_writecommand(0xF7);
  Adafruit_ILI9340_writedata(0x20);

  Adafruit_ILI9340_writecommand(0xEA);
  Adafruit_ILI9340_writedata(0x00);
  Adafruit_ILI9340_writedata(0x00);

  Adafruit_ILI9340_writecommand(ILI9340_PWCTR1);    //Power control
  Adafruit_ILI9340_writedata(0x23);   //VRH[5:0]

  Adafruit_ILI9340_writecommand(ILI9340_PWCTR2);    //Power control
  Adafruit_ILI9340_writedata(0x10);   //SAP[2:0];BT[3:0]

  Adafruit_ILI9340_writecommand(ILI9340_VMCTR1);    //VCM control
  Adafruit_ILI9340_writedata(0x3e); //�Աȶȵ���
  Adafruit_ILI9340_writedata(0x28);

  Adafruit_ILI9340_writecommand(ILI9340_VMCTR2);    //VCM control2
  Adafruit_ILI9340_writedata(0x86);  //--

  Adafruit_ILI9340_writecommand(ILI9340_MADCTL);    // Memory Access Control
  Adafruit_ILI9340_writedata(ILI9340_MADCTL_MX | ILI9340_MADCTL_BGR);

  Adafruit_ILI9340_writecommand(ILI9340_PIXFMT);
  Adafruit_ILI9340_writedata(0x55);

  Adafruit_ILI9340_writecommand(ILI9340_FRMCTR1);
  Adafruit_ILI9340_writedata(0x00);
  Adafruit_ILI9340_writedata(0x18);

  Adafruit_ILI9340_writecommand(ILI9340_DFUNCTR);    // Display Function Control
  Adafruit_ILI9340_writedata(0x08);
  Adafruit_ILI9340_writedata(0x82);
  Adafruit_ILI9340_writedata(0x27);

  Adafruit_ILI9340_writecommand(0xF2);    // 3Gamma Function Disable
  Adafruit_ILI9340_writedata(0x00);

  Adafruit_ILI9340_writecommand(ILI9340_GAMMASET);    //Gamma curve selected
  Adafruit_ILI9340_writedata(0x01);

  Adafruit_ILI9340_writecommand(ILI9340_GMCTRP1);    //Set Gamma
  Adafruit_ILI9340_writedata(0x0F);
  Adafruit_ILI9340_writedata(0x31);
  Adafruit_ILI9340_writedata(0x2B);
  Adafruit_ILI9340_writedata(0x0C);
  Adafruit_ILI9340_writedata(0x0E);
  Adafruit_ILI9340_writedata(0x08);
  Adafruit_ILI9340_writedata(0x4E);
  Adafruit_ILI9340_writedata(0xF1);
  Adafruit_ILI9340_writedata(0x37);
  Adafruit_ILI9340_writedata(0x07);
  Adafruit_ILI9340_writedata(0x10);
  Adafruit_ILI9340_writedata(0x03);
  Adafruit_ILI9340_writedata(0x0E);
  Adafruit_ILI9340_writedata(0x09);
  Adafruit_ILI9340_writedata(0x00);

  Adafruit_ILI9340_writecommand(ILI9340_GMCTRN1);    //Set Gamma
  Adafruit_ILI9340_writedata(0x00);
  Adafruit_ILI9340_writedata(0x0E);
  Adafruit_ILI9340_writedata(0x14);
  Adafruit_ILI9340_writedata(0x03);
  Adafruit_ILI9340_writedata(0x11);
  Adafruit_ILI9340_writedata(0x07);
  Adafruit_ILI9340_writedata(0x31);
  Adafruit_ILI9340_writedata(0xC1);
  Adafruit_ILI9340_writedata(0x48);
  Adafruit_ILI9340_writedata(0x08);
  Adafruit_ILI9340_writedata(0x0F);
  Adafruit_ILI9340_writedata(0x0C);
  Adafruit_ILI9340_writedata(0x31);
  Adafruit_ILI9340_writedata(0x36);
  Adafruit_ILI9340_writedata(0x0F);

  Adafruit_ILI9340_writecommand(ILI9340_SLPOUT);    //Exit Sleep
  hal_delay_ms(120);
  Adafruit_ILI9340_writecommand(ILI9340_DISPON);    //Display on
}


void Adafruit_ILI9340_setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1,
 uint16_t y1) {

	Adafruit_ILI9340_writecommand(ILI9340_CASET); // Column addr set
	Adafruit_ILI9340_writedata(x0 >> 8);
	Adafruit_ILI9340_writedata(x0 & 0xFF);     // XSTART
	Adafruit_ILI9340_writedata(x1 >> 8);
	Adafruit_ILI9340_writedata(x1 & 0xFF);     // XEND

	Adafruit_ILI9340_writecommand(ILI9340_PASET); // Row addr set
	Adafruit_ILI9340_writedata(y0>>8);
	Adafruit_ILI9340_writedata(y0);     // YSTART
	Adafruit_ILI9340_writedata(y1>>8);
	Adafruit_ILI9340_writedata(y1);     // YEND

	Adafruit_ILI9340_writecommand(ILI9340_RAMWR); // write to RAM
}


void Adafruit_ILI9340_pushColor(uint16_t color) {
  //digitalWrite(_dc, HIGH);
  //SET_BIT(dcport, dcpinmask);
	GPIO_WriteBit(LCD_DC.port, LCD_DC.pin, 1);
  //digitalWrite(_cs, LOW);
  //CLEAR_BIT(csport, cspinmask);
	GPIO_WriteBit(LCD_NSS.port, LCD_NSS.pin, 0);

  //spiwrite(color >> 8);
  //spiwrite(color);
	uint8_t rxBuf[10];
	uint8_t txBuf[2] = {color >> 8, color};
	spi_readWrite(SpiHandle, rxBuf, txBuf, 2);



  //SET_BIT(csport, cspinmask);
	GPIO_WriteBit(LCD_NSS.port, LCD_NSS.pin, 1);
  //digitalWrite(_cs, HIGH);
}

void Adafruit_ILI9340_drawPixel(int16_t x, int16_t y, uint16_t color) {

  if((x < 0) ||(x >= _width) || (y < 0) || (y >= _height)) return;

  Adafruit_ILI9340_setAddrWindow(x,y,x+1,y+1);

  //digitalWrite(_dc, HIGH);
  //SET_BIT(dcport, dcpinmask);
  HAL_GPIO_WritePin(LCD_DC.port, LCD_DC.pin, 1);
  //digitalWrite(_cs, LOW);
  //CLEAR_BIT(csport, cspinmask);
  HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 0);

  //spiwrite(color >> 8);
  //spiwrite(color);
	uint8_t rxBuf[10];
	uint8_t txBuf[2] = {color >> 8, color};
	spi_readWrite(SpiHandle, rxBuf, txBuf, 2);

  //SET_BIT(csport, cspinmask);
	HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 1);
  //digitalWrite(_cs, HIGH);
}


void Adafruit_ILI9340_drawFastVLine(int16_t x, int16_t y, int16_t h,
 uint16_t color) {

  // Rudimentary clipping
  if((x >= _width) || (y >= _height)) return;

  if((y+h-1) >= _height)
    h = _height-y;

  Adafruit_ILI9340_setAddrWindow(x, y, x, y+h-1);

  uint8_t hi = color >> 8, lo = color;

  //SET_BIT(dcport, dcpinmask);
  HAL_GPIO_WritePin(LCD_DC.port, LCD_DC.pin, 1);
  //digitalWrite(_dc, HIGH);
  //CLEAR_BIT(csport, cspinmask);
  HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 0);
  //digitalWrite(_cs, LOW);

	uint8_t rxBuf[10];
	uint8_t txBuf[2];
  while (h--) {
    //spiwrite(hi);
    //spiwrite(lo);
	txBuf[0] = hi;
	txBuf[1] = lo;

	spi_readWrite(SpiHandle, rxBuf, txBuf, 2);
  }
  //SET_BIT(csport, cspinmask);
  HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 1);
  //digitalWrite(_cs, HIGH);
}


void Adafruit_ILI9340_drawFastHLine(int16_t x, int16_t y, int16_t w,
  uint16_t color) {

  // Rudimentary clipping
  if((x >= _width) || (y >= _height)) return;
  if((x+w-1) >= _width)  w = _width-x;
  Adafruit_ILI9340_setAddrWindow(x, y, x+w-1, y);

  uint8_t hi = color >> 8, lo = color;
  //SET_BIT(dcport, dcpinmask);
  HAL_GPIO_WritePin(LCD_DC.port, LCD_DC.pin, 1);
  //CLEAR_BIT(csport, cspinmask);
  HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 0);
  //digitalWrite(_dc, HIGH);
  //digitalWrite(_cs, LOW);

  while (w--) {
    //spiwrite(hi);
    //spiwrite(lo);
	txBuf[0] = hi;
	txBuf[1] = lo;

	spi_readWrite(SpiHandle, rxBuf, txBuf, 2);
  }
  //SET_BIT(csport, cspinmask);
  HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 1);
  //digitalWrite(_cs, HIGH);
}

void Adafruit_ILI9340_fillScreen(uint16_t color) {
	Adafruit_ILI9340_fillRect(0, 0,  _width, _height, color);
}

// fill a rectangle
void Adafruit_ILI9340_fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
  uint16_t color) {

  // rudimentary clipping (drawChar w/big text requires this)
  if((x >= _width) || (y >= _height)) return;
  if((x + w - 1) >= _width)  w = _width  - x;
  if((y + h - 1) >= _height) h = _height - y;

  Adafruit_ILI9340_setAddrWindow(x, y, x+w-1, y+h-1);

  uint8_t hi = color >> 8, lo = color;

  //SET_BIT(dcport, dcpinmask);
  HAL_GPIO_WritePin(LCD_DC.port, LCD_DC.pin, 1);
  //digitalWrite(_dc, HIGH);
  //CLEAR_BIT(csport, cspinmask);
  HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 0);
  //digitalWrite(_cs, LOW);

  for(y=h; y>0; y--) {
    for(x=w; x>0; x--) {
      //spiwrite(hi);
      //spiwrite(lo);
  	txBuf[0] = hi;
  	txBuf[1] = lo;

  	spi_readWrite(SpiHandle, rxBuf, txBuf, 2);
  	//HAL_SPI_TransmitReceive(&SpiHandle, (uint8_t*)txBuf, (uint8_t*)rxBuf, 2, 1000);
    }
  }
  //digitalWrite(_cs, HIGH);
  //SET_BIT(csport, cspinmask);
  HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 1);
}


// Pass 8-bit (each) R,G,B, get back 16-bit packed color
uint16_t Adafruit_ILI9340_Color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}


void Adafruit_ILI9340_setVertialScrollStartAddress(uint16_t address)
{
	Adafruit_ILI9340_writecommand(ILI9340_VSCRSADD);
	Adafruit_ILI9340_writedata((address >> 8) & 0xFF);
	Adafruit_ILI9340_writedata(address & 0xFF);
}

void Adafruit_ILI9340_setVerticalScrollDefinition(uint16_t topFixedArea,
		uint16_t verticalScrollArea, uint16_t bottomFixedArea)
{
	if (topFixedArea + verticalScrollArea + bottomFixedArea != 320) return;

	Adafruit_ILI9340_writecommand(ILI9340_VSCRDEF);
	Adafruit_ILI9340_writedata((topFixedArea >> 8) & 0xFF);
	Adafruit_ILI9340_writedata(topFixedArea & 0xFF);
	Adafruit_ILI9340_writedata((verticalScrollArea >> 8) & 0xFF);
	Adafruit_ILI9340_writedata(verticalScrollArea & 0xFF);
	Adafruit_ILI9340_writedata((bottomFixedArea >> 8) & 0xFF);
	Adafruit_ILI9340_writedata(bottomFixedArea & 0xFF);
}

void Adafruit_ILI9340_setRotation(uint8_t m) {

	Adafruit_ILI9340_writecommand(ILI9340_MADCTL);
  int rotation = m % 4; // can't be higher than 3
  switch (rotation) {
   case 0:
	   Adafruit_ILI9340_writedata(ILI9340_MADCTL_MX | ILI9340_MADCTL_BGR);
     _width  = ILI9340_TFTWIDTH;
     _height = ILI9340_TFTHEIGHT;
     break;
   case 1:
	   Adafruit_ILI9340_writedata(ILI9340_MADCTL_MV | ILI9340_MADCTL_BGR);
     _width  = ILI9340_TFTHEIGHT;
     _height = ILI9340_TFTWIDTH;
     break;
  case 2:
	  Adafruit_ILI9340_writedata(ILI9340_MADCTL_MY | ILI9340_MADCTL_BGR);
     _width  = ILI9340_TFTWIDTH;
     _height = ILI9340_TFTHEIGHT;
    break;
   case 3:
	   Adafruit_ILI9340_writedata(ILI9340_MADCTL_MV | ILI9340_MADCTL_MY | ILI9340_MADCTL_MX | ILI9340_MADCTL_BGR);
     _width  = ILI9340_TFTHEIGHT;
     _height = ILI9340_TFTWIDTH;
     break;
  }
}


void Adafruit_ILI9340_invertDisplay(bool i) {
	Adafruit_ILI9340_writecommand(i ? ILI9340_INVON : ILI9340_INVOFF);
}


////////// stuff not actively being used, but kept for posterity


//uint8_t Adafruit_ILI9340_spiread(void) {
//  uint8_t r = 0;
//
//  if (hwSPI) {
//#ifdef __AVR__
//    SPDR = 0x00;
//    while(!(SPSR & _BV(SPIF)));
//    r = SPDR;
//#endif
//#if defined(USE_SPI_LIBRARY)
//    r = SPI.transfer(0x00);
//#endif
//  } else {
//
//    for (uint8_t i=0; i<8; i++) {
//      digitalWrite(_sclk, LOW);
//      digitalWrite(_sclk, HIGH);
//      r <<= 1;
//      if (digitalRead(_miso))
//	r |= 0x1;
//    }
//  }
//  //Serial.print("read: 0x"); Serial.print(r, HEX);
//
//  return r;
//}
//
// uint8_t Adafruit_ILI9340_readdata(void) {
//   digitalWrite(_dc, HIGH);
//   digitalWrite(_cs, LOW);
//   uint8_t r = spiread();
//   digitalWrite(_cs, HIGH);
//
//   return r;
//}
//
//
// uint8_t Adafruit_ILI9340_readcommand8(uint8_t c) {
//   digitalWrite(_dc, LOW);
//   digitalWrite(_sclk, LOW);
//   digitalWrite(_cs, LOW);
//   spiwrite(c);
//
//   digitalWrite(_dc, HIGH);
//   uint8_t r = spiread();
//   digitalWrite(_cs, HIGH);
//   return r;
//}



/*

 uint16_t Adafruit_ILI9340::readcommand16(uint8_t c) {
 digitalWrite(_dc, LOW);
 if (_cs)
 digitalWrite(_cs, LOW);

 spiwrite(c);
 pinMode(_sid, INPUT); // input!
 uint16_t r = spiread();
 r <<= 8;
 r |= spiread();
 if (_cs)
 digitalWrite(_cs, HIGH);

 pinMode(_sid, OUTPUT); // back to output
 return r;
 }

 uint32_t Adafruit_ILI9340::readcommand32(uint8_t c) {
 digitalWrite(_dc, LOW);
 if (_cs)
 digitalWrite(_cs, LOW);
 spiwrite(c);
 pinMode(_sid, INPUT); // input!

 dummyclock();
 dummyclock();

 uint32_t r = spiread();
 r <<= 8;
 r |= spiread();
 r <<= 8;
 r |= spiread();
 r <<= 8;
 r |= spiread();
 if (_cs)
 digitalWrite(_cs, HIGH);

 pinMode(_sid, OUTPUT); // back to output
 return r;
 }

 */
