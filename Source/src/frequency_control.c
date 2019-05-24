#include "frequency_control.h"

#include <stdbool.h>
#include "si5338.h"
#include "stm32f7xx_hal_rcc_ex.h"
#include "diag/Trace.h"
#include "stm32f7xx_hal_i2c.h"


#define SI5338_ADDRESS 0x70
I2C_HandleTypeDef handleI2C;

bool initFrequencySynth()
{
  //I2C_HandleTypeDef hi2c;
  //HAL_I2C_MspInit(&hi2c);

  //__HAL_I2C_DISABLE(I2C2);
  //clearStuckBusyFlag();

    RCC_PeriphCLKInitTypeDef i2cClk;
    i2cClk.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
    i2cClk.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;

  HAL_RCCEx_PeriphCLKConfig(&i2cClk);

  handleI2C.Instance = I2C2;
  HAL_I2C_DeInit(&handleI2C);

  handleI2C.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  //handleI2C.Init.ClockSpeed = 400000;
  //handleI2C.Init. = 400000;
  handleI2C.Init.DualAddressMode = I2C_DUALADDRESS_DISABLED;
  //handleI2C.Init.DutyCycle = I2C_DUTYCYCLE_16_9;
  //handleI2C.Init.Timing = I2C_ .DutyCycle = I2C_DUTYCYCLE_16_9;
  handleI2C.Init.GeneralCallMode = I2C_GENERALCALL_DISABLED;
  handleI2C.Init.NoStretchMode = I2C_NOSTRETCH_DISABLED;
  handleI2C.Init.OwnAddress1 = 0x30F;
  handleI2C.Init.OwnAddress2 = 0xFE;
  handleI2C.Init.Timing = ((uint32_t)0x40912732); //Not sure how this is calculated, taken from EVAL example


  if(HAL_I2C_Init(&handleI2C) != HAL_OK)
    {
      trace_puts("I2C didn't Init correctly");

    }
  //handleI2C.Instance->SR2 = 0; //What the heck is wrong with this thing?!
  //clearStuckBusyFlag();

  HAL_StatusTypeDef result = HAL_ERROR;

  while(result!= HAL_OK)
    result = HAL_I2C_IsDeviceReady(&handleI2C, (0x70 << 1), 100, 100); //We need to shift the address to the left for it to work (because of the R/W bit)

  //HAL_I2C_Master_Transmit(&hi2c, 230, 0x10, 1, 1000);  //write_Si5338(230, 0x10); //OEB_ALL = 1

  //*handleI2C = hi2c;

  //HAL_I2C_Master_Transmit(&handleI2C, 230, 0x4F, 1, 1000);  //write_Si5338(230, 0x10); //OEB_ALL = 1
  //HAL_I2C_Master_Transmit(handleI2C, 230, 0x4F, 1, 1000);  //write_Si5338(230, 0x10); //OEB_ALL = 1
  //handleI2C = &hi2c;
  //HAL_I2C_Master_Transmit(&handleI2C, 230, 0x4F, 1, 1000);  //write_Si5338(230, 0x10); //OEB_ALL = 1
  //HAL_I2C_Master_Transmit(handleI2C, 230, 0x4F, 1, 1000);  //write_Si5338(230, 0x10); //OEB_ALL = 1

  i2cSetup();
  //i2cLoop();


  //trace_puts(( == HAL_OK ? "SI5338 Ready" : "SI5338 Not ready"));

  //HAL_I2C_MspInit(&hi2c);
  return true;
}

bool setFrequency(unsigned long long frequency)
{
  setFreq(frequency);
  return true;
}
