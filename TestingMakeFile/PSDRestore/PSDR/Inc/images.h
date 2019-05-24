/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __images_H
#define __images_H

#include <stdint.h>

typedef struct
{
  int width;
  int height;
  uint8_t *imageData;
} Image;

extern Image imagePsdrLogo;
extern Image imageMode;
extern Image imageLsb;
extern Image imageUsb;
extern Image imageAm;
extern Image imagePeriod;
extern Image imageOne;
extern Image imageTwo;
extern Image imageThree;
extern Image imageFour;
extern Image imageFive;
extern Image imageSix;
extern Image imageSeven;
extern Image imageEight;
extern Image imageNine;
extern Image imageZero;
extern unsigned short bitmapIronGradient[];
extern unsigned short bitmapWebSdrGradient[];
extern Image imageFilter;
extern unsigned short bitmapSMeter[];
extern unsigned short bitmapOff[];

extern Image imageOneSmall;
extern Image imageTwoSmall;
extern Image imageThreeSmall;
extern Image imageFourSmall;
extern Image imageFiveSmall;
extern Image imageSixSmall;
extern Image imageSevenSmall;
extern Image imageEightSmall;
extern Image imageNineSmall;
extern Image imageZeroSmall;
extern Image imageHyphenSmall;
extern Image imageTildeSmall;

#endif /* __images_H */
