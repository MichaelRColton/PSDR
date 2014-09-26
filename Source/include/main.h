#pragma once

#include "stm32f415xx.h"
#include "stm32f4xx.h"
//#include "core_cmInstr.h"
#include <stdio.h>
#include "stddef.h"
#include "diag/Trace.h"
#include "hal.h"
#include "adc.h"
#include "dds.h"
#include "spi.h"
#include "Timer.h"
#include "BlinkLed.h"
//#include "stm32f4xx_hal_rcc.h"
#include "stm32f4xx_hal_gpio.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9340.h"
#include "stm32f4xx_hal.h"
//#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_def.h"
#include "string.h"
#include "math.h"
#include "arm_math.h"
//#include "stm32f4xx_hal_rtc.h"
//#include "stm32f4xx_hal_adc.h"
#include "stm32f4xx_it.h"
#include "stm32f4xx_hal_tim.h"
#include "stm32f4xx_hal_cortex.h"
#include "misc.h"
#include "stm32f4xx_hal_dac.h"

#include "images.h"

TIM_HandleTypeDef    TimHandle;
TIM_HandleTypeDef    TimHandle4;

#define MASKRED    0b1111100000000000
#define MASKGREEN  0b0000011111100000
#define MASKBLUE   0b0000000000011111
#define MASKYELLOW 0b1111111111100000
#define MASKTEAL   0b0000011111111111
#define MASKWHITE  0b1111111111111111
#define MASKBLACK  0b0000000000000000
