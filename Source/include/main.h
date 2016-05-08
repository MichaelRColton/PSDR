#pragma once

//#define PSDR1 1
//#define PSDR2 2
//#define PSDR25 3
#define PSDR257 4
//#define ARM_MATH_CM7


//#define PSDR_BOARD PSDR2

#ifdef PSDR1
#define STM32F415xx
#define HSE_VALUE=8000000 //did I do this right? Both of these were defined in the progect settings before in the several preprocessors

//#include "stm32f415xx.h"
#endif

#ifdef PSDR2
//#define HSE_VALUE 24000000

#include "stm32f429xx.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal_conf.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"
#include "stm32f4xx_hal_def.h"
#include "stm32f4xx_it.h"
#include "stm32f4xx_hal_tim.h"
#include "stm32f4xx_hal_cortex.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_dac.h"
#include "stm32f4xx_hal_uart.h"
#include "stm32f4xx_hal_usart.h"
#endif

#ifdef PSDR257
//#define HSE_VALUE 26000000
#define ARM_MATH_CM7
//#include "stm32f756xx.h"
#include "stm32f7xx.h"
#include "stm32f7xx_hal_conf.h"
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_gpio.h"
#include "stm32f7xx_hal_def.h"
#include "stm32f7xx_it.h"
#include "stm32f7xx_hal_tim.h"
#include "stm32f7xx_hal_cortex.h"
#include "stm32f7xx_hal_dma.h"
#include "stm32f7xx_hal_dac.h"
#include "stm32f7xx_hal_uart.h"
#include "stm32f7xx_hal_usart.h"
#include "stm32f7xx_hal_i2c.h"
#include "stm32f7xx_hal_i2c_ex.h"
#include "stm32f7xx_hal_tim_ex.h"

#endif


#include "arm_math.h"
#include <stdio.h>
#include "stddef.h"
#include "diag/Trace.h"
#include "hal.h"
#include "adc.h"
#include "dds.h"
#include "Timer.h"
#include "BlinkLed.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9340.h"
#include "string.h"
#include "math.h"
#include "images.h"
#include "i2c.h"
//#include "stm32f4xx_hal_rcc.h"
//#include "stm32f4xx_hal.h"
//#include "stm32f4xx_hal_rtc.h"
//#include "stm32f4xx_hal_adc.h"
//#include "misc.h"
//#include "stm32f4xx_hal_def.h"
//#include "core_cmInstr.h"


TIM_HandleTypeDef    TimHandle;
TIM_HandleTypeDef    TimHandle4;

#define MASKRED    0b1111100000000000
#define MASKGREEN  0b0000011111100000
#define MASKBLUE   0b0000000000011111
#define MASKYELLOW 0b1111111111100000
#define MASKTEAL   0b0000011111111111
#define MASKWHITE  0b1111111111111111
#define MASKBLACK  0b0000000000000000

/* Definition for SPIx's DMA */
#define SPIx_TX_DMA_CHANNEL              DMA_CHANNEL_3
#define SPIx_TX_DMA_STREAM               DMA2_Stream5
#define SPIx_RX_DMA_CHANNEL              DMA_CHANNEL_3
#define SPIx_RX_DMA_STREAM               DMA2_Stream2

/* Definition for SPIx's NVIC */
#define SPIx_DMA_TX_IRQn                 DMA2_Stream3_IRQn
#define SPIx_DMA_RX_IRQn                 DMA2_Stream0_IRQn
#define SPIx_DMA_TX_IRQHandler           DMA2_Stream3_IRQHandler
#define SPIx_DMA_RX_IRQHandler           DMA2_Stream0_IRQHandler

SPI_HandleTypeDef SpiHandle;
UART_HandleTypeDef UartHandle;
