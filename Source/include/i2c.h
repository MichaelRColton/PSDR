#ifndef I2C_H_
    #define I2C_H_

#include <hal.h>
//#include "register_map.h"
//#include "stm32f4xx_hal_i2c.h"

#define SI5338_ADDRESS 0x70
I2C_HandleTypeDef handleI2C;

#endif /* DDS_H_ */

void clearStuckBusyFlag(void);
void i2cSetup(void);
void setFreq(unsigned long frequency);

