#include "drawing.h"
#include "Adafruit_ILI9340.h"
#include "Adafruit_GFX.h"
#include "images.h"
#include "math.h"

void drawNumber(char c, uint16_t x, uint16_t y, uint16_t tintMask)
{
  switch(c)
  {
  case '.':
    Adafruit_GFX_drawColorBitmap(x, y, imagePeriod.imageData, imagePeriod.width, imagePeriod.height, tintMask);
    break;
  case '1':
    Adafruit_GFX_drawColorBitmap(x, y, imageOne.imageData, imageOne.width, imageOne.height, tintMask);
    break;
  case '2':
    Adafruit_GFX_drawColorBitmap(x, y, imageTwo.imageData, imageTwo.width, imageTwo.height, tintMask);
    break;
  case '3':
    Adafruit_GFX_drawColorBitmap(x, y, imageThree.imageData, imageThree.width, imageThree.height, tintMask);
    break;
  case '4':
    Adafruit_GFX_drawColorBitmap(x, y, imageFour.imageData, imageFour.width, imageFour.height, tintMask);
    break;
  case '5':
    Adafruit_GFX_drawColorBitmap(x, y, imageFive.imageData, imageFive.width, imageFive.height, tintMask);
    break;
  case '6':
    Adafruit_GFX_drawColorBitmap(x, y, imageSix.imageData, imageSix.width, imageSix.height, tintMask);
    break;
  case '7':
    Adafruit_GFX_drawColorBitmap(x, y, imageSeven.imageData, imageSeven.width, imageSeven.height, tintMask);
    break;
  case '8':
    Adafruit_GFX_drawColorBitmap(x, y, imageEight.imageData, imageEight.width, imageEight.height, tintMask);
    break;
  case '9':
    Adafruit_GFX_drawColorBitmap(x, y, imageNine.imageData, imageNine.width, imageNine.height,tintMask);
    break;
  case '0':
    Adafruit_GFX_drawColorBitmap(x, y, imageZero.imageData, imageZero.width, imageZero.height, tintMask);
    break;
  default:
    Adafruit_GFX_fillRect(x, y, 15, 19, ILI9340_BLACK);
  }
}

void drawNumberSmall(char c, uint16_t x, uint16_t y, uint16_t tintMask)
{
  switch(c)
  {
  case '1':
    Adafruit_GFX_drawColorBitmap(x, y, imageOneSmall.imageData, imageOneSmall.width, imageOneSmall.height, tintMask);
    break;
  case '2':
    Adafruit_GFX_drawColorBitmap(x, y, imageTwoSmall.imageData, imageTwoSmall.width, imageTwoSmall.height, tintMask);
    break;
  case '3':
    Adafruit_GFX_drawColorBitmap(x, y, imageThreeSmall.imageData, imageThreeSmall.width, imageThreeSmall.height, tintMask);
    break;
  case '4':
    Adafruit_GFX_drawColorBitmap(x, y, imageFourSmall.imageData, imageFourSmall.width, imageFourSmall.height, tintMask);
    break;
  case '5':
    Adafruit_GFX_drawColorBitmap(x, y, imageFiveSmall.imageData , imageFiveSmall.width, imageFiveSmall.height, tintMask);
    break;
  case '6':
    Adafruit_GFX_drawColorBitmap(x, y, imageSixSmall.imageData, imageSixSmall.width, imageSixSmall.height, tintMask);
    break;
  case '7':
    Adafruit_GFX_drawColorBitmap(x, y, imageSevenSmall.imageData, imageSevenSmall.width, imageSevenSmall.height, tintMask);
    break;
  case '8':
    Adafruit_GFX_drawColorBitmap(x, y, imageEightSmall.imageData, imageEightSmall.width , imageEightSmall.height, tintMask);
    break;
  case '9':
    Adafruit_GFX_drawColorBitmap(x, y, imageNineSmall.imageData, imageNineSmall.width, imageNineSmall.height, tintMask);
    break;
  case '0':
    Adafruit_GFX_drawColorBitmap(x, y, imageZeroSmall.imageData, imageZeroSmall.width, imageZeroSmall.height, tintMask);
    break;
  case '-':
    Adafruit_GFX_drawColorBitmap(x, y, imageHyphenSmall.imageData, imageHyphenSmall.width, imageHyphenSmall.height, tintMask);
    break;
  case '~':
    Adafruit_GFX_drawColorBitmap(x, y, imageTildeSmall.imageData, imageTildeSmall.width, imageTildeSmall.height, tintMask);
    break;
  default:
    Adafruit_GFX_fillRect(x, y, 8, 9, ILI9340_BLACK);
  }
}

float passBandRms = 0;
int lastSMeterBarWidth = 0;
void drawSMeter(void)
{

  //Adafruit_GFX_fillRect(150, 160, 170, 3, ILI9340_BLACK);
  int width = 10*log((passBandRms * 1000000) + 1);
  if(width > lastSMeterBarWidth)
    Adafruit_GFX_fillRect(150 + lastSMeterBarWidth, 156, width - lastSMeterBarWidth, 3, ILI9340_RED);
  else
    Adafruit_GFX_fillRect(150 + width, 156, lastSMeterBarWidth - width, 3, ILI9340_BLACK);

  lastSMeterBarWidth = width;
}

int newWaterFallData = 0;
//we want some kind of struct that holds.... the FFT result, and the frequency parameters so it
//knows how to draw it. And whether it was transmitting so it knows what color to make it.
//Do I want the other app to scale/log/etc? I think... not. Just feed it what you got
//and it figures out how to display it. And zoom setting? Or maybe we want the calling app
//to do that part. Just tell drawWaterfaull the start/stop or width/center. I think
//I'm in the mood for center/width/
//Wait. It needs the historical data (from somewhere) which has it's own fixed parameters:
//A start/stop freq, a TX/RX, and the raw data. (also maybe a min/max that the waterfall code
//can use to determine brightness.
//Maybe we have a thing that holds historical FFT structs (some kind of circular buffer)
//and an index so we know which one is the most current.
//Then this function just takes the desired display parameters: center/width/scale/speed?
//do we want it to use the display's scrolling trick? I... don't really want to if I don't
//have to, but I should use it in circumstances where it can be used. As it will save time
//for other things. Let's leave that out for now. I think we have the performance
//to not need it. for now. (Old code had to use it, but it was slower, and didn't vertical
//scroll or scale or anything.
//Also, we might want partial transparency bar(s) for the current bandpass filter,
//or as a way to show hands, etc.
//Storing ~100 samples of 256 wide FFT is 100KB of RAM (1/3 of the total we have on this
//Chip... how much do I need? How much am I using now? How much would other tasks take?
//How do I allocate large amounts of memory in FreeRTOS?
//If I ever want the option of a 512 FFT, I'm going to use up 2/3s of RAM just to store
//the waterfall. But man, I do love me some nice waterfall.
//I mean, I could still oversample to get superior anti-aliasing and stuff.
//If I'm going to save the whole FFT, then... should I use the same storage for the
//sample processing? (Currently using SamplesA/B/C.... hmmm.
//void drawWaterfall(void)
//{
//  if(newWaterFallData == 1)
//    {
//      static float magnitudes[FFT_SIZE];
//      static float mags;
//      static uint8_t waterfallScanLine = 0;
//
//      unsigned short *gradient;
//
//      if(transmitting)
//        gradient = bitmapIronGradient;
//      else
//        gradient = bitmapWebSdrGradient;
//
//      //arm_cmplx_mag_f32(samplesDisplay, magnitudes, FFT_SIZE);
//      arm_cmplx_mag_f32(samplesDisplay, magnitudes, FFT_SIZE);
//
//      float fftMax = 0; //AH! These are being reset each time! Static makes them persistant right? Does it also ensure they are
//      float fftMin = 100; //only initialized once? Have to try it when I get home. It would certainly be nice if the waterfall
//      static float fftMaxMax = 0; //didn't change in brightness so much. Later, I may want to fix these values, or at least, make them
//      //static float logMax; //manually controllable, sorta, you know?
//      uint8_t i;
//      for(i = 1; i < 255; i++) //If bin 0 is the DC offset, should we skip it in this calculation?
//      {
//        float mags = magnitudes[i];
//        if(mags > fftMax) fftMax = mags;
//        if(mags < fftMin) fftMin = mags;
//      }
//      //logMax = log2(fftMax);
//
//      if(fftMax > fftMaxMax) fftMaxMax += fftMax * 0.1;
//      //logMax = log2(fftMaxMax);
//      fftMaxMax *= 0.99;
//      if (fftMaxMax > fftMaxMaxMax) fftMaxMax = fftMaxMaxMax;
//      if (fftMaxMax < fftMaxMaxMin) fftMaxMax = fftMaxMaxMin;
//
//
//      //      TODO: Got rid of the first bin because it's just DC offset, right?
//      //      but now narrow signal can disappear when they are right at the center....
//      //      Will that be better when I lower the sample frequency? Maybe I should do that next.
//
//      Adafruit_ILI9340_setAddrWindow(waterfallScanLine, 0, waterfallScanLine, 120);
//      for(i = 120; i != 0; i--)
//      {
//        mags = (log2(magnitudes[i] + 1)) / fftMaxMax * 100; //Log needs to be at least 1 right? We could do a + (1-fftMin) maybe? Worth it?
//        //mags = magnitudes[i] / fftMaxMax * 32;
//        //Adafruit_ILI9340_drawPixel(waterfallScanLine, (120 - i), gradient[(uint8_t) mags]);
//        Adafruit_ILI9340_pushColor(gradient[(uint8_t) mags]);
//      }
//
//      Adafruit_ILI9340_setAddrWindow(waterfallScanLine, 120, waterfallScanLine, 239);
//      for(i = 255; i > 135; i--)
//      {
//        mags = (log2(magnitudes[i] + 1)) / fftMaxMax * 100;
//        //mags = magnitudes[i] / fftMaxMax * 32;
//        //Adafruit_ILI9340_drawPixel(waterfallScanLine, 359 - (i - 15), gradient[(uint8_t) mags]);
//        Adafruit_ILI9340_pushColor(gradient[(uint8_t) mags]);
//      }
//
//      waterfallScanLine++;
//      if(waterfallScanLine > 119) waterfallScanLine = 0;
//      Adafruit_ILI9340_setVerticalScrollStartAddress((/*119 -*/ waterfallScanLine) /*+ 200*/);
//
//      newWaterFallData = 0;
//    }
//}

//void updateDisplay(uint8_t force)
//{
//  displayUpdating = 1;
//  static char freqChar[15];
//  static char lastFreqChar[] = {'$','$','$','$','$','$','$','$','$','$','$','$','$','$','$',};
//
//  //Draw elements that don't normally change. Static icons, etc.
//  if(force)
//  {
//    //Adafruit_GFX_drawColorBitmap(180, 2, psdrLogo, 86,20, MASKWHITE);
//    Adafruit_GFX_drawColorBitmap(150, 90, bitmapMode, 40,12, MASKWHITE);
//    Adafruit_GFX_fillTriangle(126,119,136,124,136,114,ILI9340_WHITE);
//    Adafruit_GFX_drawColorBitmap(150, 136, bitmapFilter, 47,12, MASKWHITE);
//    drawNumber('.', freqHOffset + 16*3, freqVOffset + 0, MASKWHITE);
//    drawNumber('.', freqHOffset + 16*7, freqVOffset + 0, MASKWHITE);
//    Adafruit_GFX_drawColorBitmap(142, 162, bitmapSMeter, 155, 10, MASKWHITE);
//    //Adafruit_GFX_drawColorBitmap(320 - 45 - 2, 240 - 46 - 2, bitmapHadLogo, 45, 46, MASKWHITE);
//  }
//
//  sprintf(freqChar, "%9ld", vfoAFrequency);
//
//  //So on each of these elements, we update when the value changes, when we're forced to, when the item becomes selected, or unselected.
//  if(freqChar[0] != lastFreqChar[0]
//       ||  force
//       || (menuPos != menuLastPos && (menuPos == megahertzMenuItem
//       || menuLastPos == megahertzMenuItem)))
//  {
//    drawNumber(freqChar[0], freqHOffset + 16*0, freqVOffset + 0,
//               menuPos == megahertzMenuItem ? MASKRED : MASKWHITE);
//  }
//  if(freqChar[1] != lastFreqChar[1]
//       || redItems[0]
//       || force
//       || (menuPos != menuLastPos && (menuPos == megahertzMenuItem
//       || menuLastPos == megahertzMenuItem)))
//  {
//    drawNumber(freqChar[1], freqHOffset + 16*1, freqVOffset + 0,
//               menuPos == megahertzMenuItem ? MASKRED : MASKWHITE);
//  }
//  if(freqChar[2] != lastFreqChar[2]
//       || force
//       || (menuPos != menuLastPos && (menuPos == megahertzMenuItem
//       || menuLastPos == megahertzMenuItem)))
//  {
//    drawNumber(freqChar[2], freqHOffset + 16*2, freqVOffset + 0,
//               menuPos == megahertzMenuItem ? MASKRED : MASKWHITE);
//  }
//  if(freqChar[3] != lastFreqChar[3]
//       || force
//       || (menuPos != menuLastPos && (menuPos == hundredKilohertzMenuItem
//       || menuLastPos == hundredKilohertzMenuItem)))
//  {
//    drawNumber(freqChar[3], freqHOffset + 16*4, freqVOffset + 0,
//               menuPos == hundredKilohertzMenuItem ? MASKRED : MASKWHITE);
//  }
//  if(freqChar[4] != lastFreqChar[4]
//       || force
//       || (menuPos != menuLastPos && (menuPos == tenKilohertzMenuItem
//       || menuLastPos == tenKilohertzMenuItem)))
//  {
//    drawNumber(freqChar[4], freqHOffset + 16*5, freqVOffset + 0,
//               menuPos == tenKilohertzMenuItem ? MASKRED : MASKWHITE);
//  }
//  if(freqChar[5] != lastFreqChar[5] || force || (menuPos != menuLastPos && (menuPos == kilohertzMenuItem || menuLastPos == kilohertzMenuItem)))
//  {
//    drawNumber(freqChar[5], freqHOffset + 16*6, freqVOffset + 0, menuPos == kilohertzMenuItem ? MASKRED : MASKWHITE);
//  }
//  if(freqChar[6] != lastFreqChar[6] || force || (menuPos != menuLastPos && (menuPos == hundredHertzMenuItem || menuLastPos == hundredHertzMenuItem)))
//  {
//    drawNumber(freqChar[6], freqHOffset + 16*8, freqVOffset + 0, menuPos == hundredHertzMenuItem ? MASKRED : MASKWHITE);
//  }
//  if(freqChar[7] != lastFreqChar[7] || force || (menuPos != menuLastPos && (menuPos == tenHertzMenuItem || menuLastPos == tenHertzMenuItem)))
//  {
//    drawNumber(freqChar[7], freqHOffset + 16*9, freqVOffset + 0, menuPos == tenHertzMenuItem ? MASKRED : MASKWHITE);
//  }
//  if(freqChar[8] != lastFreqChar[8] || force || (menuPos != menuLastPos && (menuPos == hertzMenuItem || menuLastPos == hertzMenuItem)))
//  {
//    drawNumber(freqChar[8], freqHOffset + 16*10, freqVOffset + 0, menuPos == hertzMenuItem ? MASKRED : MASKWHITE);
//  }
//
//  vfoALastFreq = vfoAFrequency;
//  strcpy(lastFreqChar, freqChar);
//
//  int redrawFilterBar = 0;
//
//  if(mode != modeLast
//      || filterLowerLimit != filterLastLowerLimit
//      || force
//      || (menuPos != menuLastPos && (menuPos == filterLowMenuItem
//      || menuLastPos == filterLowMenuItem)))
//  {
//    sprintf(freqChar, "%4d", filterLowerLimit * 40);
//    //Adafruit_GFX_setTextSize(2);
//    //Adafruit_GFX_setTextColor(menuPos == 7 ? ILI9340_RED : ILI9340_WHITE, ILI9340_BLACK);
//    //Adafruit_GFX_setCursor(200, 135 );
//    int i;
//    for(i = 0; i < 4; i++)
//    {
//      //Adafruit_GFX_write(freqChar[i]);
//      drawNumberSmall(freqChar[i], 205 + (i * 9), 137, menuPos == filterLowMenuItem ? MASKRED : MASKWHITE);
//    }
//    //Adafruit_GFX_setTextSize(3);
//
//    redrawFilterBar = 1;
//    filterLastLowerLimit = filterLowerLimit;
//  }
//
//  if(mode != modeLast
//      || filterUpperLimit != filterLastUpperLimit
//      || force
//      || (menuPos != menuLastPos && (menuPos == filterHighMenuItem
//      || menuLastPos == filterHighMenuItem)))
//  {
//    sprintf(freqChar, "%-4d", filterUpperLimit * 40);
//    //Adafruit_GFX_setTextSize(2);
//    //Adafruit_GFX_setTextColor(menuPos == 8 ? ILI9340_RED : ILI9340_WHITE, ILI9340_BLACK);
//    //Adafruit_GFX_setCursor(265, 135 );
//    int i;
//    for(i = 0; i < 4; i++)
//    {
//      //Adafruit_GFX_write(freqChar[i]);
//      drawNumberSmall(freqChar[i], 250 + (i * 9), 137, menuPos == filterHighMenuItem ? MASKRED : MASKWHITE);
//    }
//    //Adafruit_GFX_setTextSize(3);
//
//    redrawFilterBar = 1;
//    filterLastUpperLimit = filterUpperLimit;
//  }
//
//  if(afGainLast != afGain || force || (menuPos != menuLastPos && (menuPos == volumeMenuItem || menuLastPos == volumeMenuItem)))
//  {
//    sprintf(freqChar, "%4d", (int)(afGain * 10));
//    int i;
//    for(i = 0; i < 4; i++)
//    {
//      //Adafruit_GFX_write(freqChar[i]);
//      drawNumberSmall(freqChar[i], 250 + (i * 9), 10, menuPos == volumeMenuItem ? MASKRED : MASKWHITE);
//    }
//
//    afGainLast = afGain;
//  }
//
//  if(force || (menuPos != menuLastPos && (menuPos == offMenuItem || menuLastPos == offMenuItem)))
//  {
//    Adafruit_GFX_drawColorBitmap(220, 10, bitmapOff, 20, 9, menuPos == offMenuItem ? MASKRED : MASKWHITE);
//  }
//
//  //I think I want to make this more like the Draw S Meter, where it only draws the parts that have changed. Also, I think I want to do gray pixels when half
//  //way between values.
//  if(redrawFilterBar)
//  {
//    switch(mode)
//    {
//      case LSB:
//        Adafruit_GFX_fillRect(121, 0, 3, 240 , ILI9340_BLACK); //erase the old bar
//        Adafruit_GFX_fillRect(121, filterLowerLimit/2 + 120, 3, (filterUpperLimit/2 - filterLowerLimit/2), ILI9340_WHITE); //draw the new one
//        break;
//      case USB:
//        Adafruit_GFX_fillRect(121, 0, 3, 240 , ILI9340_BLACK);
//        Adafruit_GFX_fillRect(121, 120 - (filterUpperLimit/2), 3, (filterUpperLimit/2 - filterLowerLimit/2), ILI9340_WHITE);
//        break;
//      case AM:
//        Adafruit_GFX_fillRect(121, 0, 3, 240 , ILI9340_BLACK);
//        Adafruit_GFX_fillRect(121, filterLowerLimit/2 + 120, 3, (filterUpperLimit/2 - filterLowerLimit/2), ILI9340_WHITE);
//        Adafruit_GFX_fillRect(121, 120 - (filterUpperLimit/2), 3, (filterUpperLimit/2 - filterLowerLimit/2), ILI9340_WHITE);
//        break;
//    }
//    redrawFilterBar = 0;
//  }
//
//
//  if(mode != modeLast
//      || force
//      || (menuPos != menuLastPos && (menuPos == modeMenuItem
//      || menuLastPos == modeMenuItem)))
//  {
//    switch(mode)
//    {
//    case LSB:
//      Adafruit_GFX_drawColorBitmap(196, 91, bitmapLSB, 28, 9, menuPos == modeMenuItem ? MASKRED : MASKWHITE);
//      break;
//    case USB:
//      Adafruit_GFX_drawColorBitmap(196, 91, bitmapUSB, 28, 9, menuPos == modeMenuItem ? MASKRED : MASKWHITE);
//      break;
//    case AM:
//      Adafruit_GFX_drawColorBitmap(196, 91, bitmapAM, 28, 9, menuPos == modeMenuItem ? MASKRED : MASKWHITE);
//      break;
//    }
//
//    modeLast = mode;
//
//  }
//
//  if(afGain * 0.99 )
//
//  menuLastPos = menuPos;
//  displayUpdating = 0;
//}
