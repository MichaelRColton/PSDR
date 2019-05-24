/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __drawing_H
#define __drawing_H

#ifdef __cplusplus
 extern "C" {
#endif


#include "stdint.h"

   void drawNumber(char c, uint16_t x, uint16_t y, uint16_t tintMask);
   void drawNumberSmall(char c, uint16_t x, uint16_t y, uint16_t tintMask);
   void drawSMeter(void);

#endif /* __drawing_H */
