/**
  ******************************************************************************
  * File Name          : gpio.h
  * Description        : This file contains all the functions prototypes for 
  *                      the gpio  
  ******************************************************************************
  * This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * Copyright (c) 2018 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __gpio_H
#define __gpio_H
#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */


typedef struct _Gpio_Pin
{
  GPIO_TypeDef* port;
  uint16_t pin;
} Gpio_Pin;

// gpio pins
extern const Gpio_Pin LCD_NSS;
extern const Gpio_Pin RX_TO_GPS;
extern const Gpio_Pin TX_FROM_GPS;
extern const Gpio_Pin GPS_RESET;
extern const Gpio_Pin GPS_PPS;
extern const Gpio_Pin GPS_POWER;
extern const Gpio_Pin LCD_SCK;
extern const Gpio_Pin LCD_MISO;
extern const Gpio_Pin LCD_MOSI;
extern const Gpio_Pin SPI2_SCK;
extern const Gpio_Pin SPI2_MISO;
extern const Gpio_Pin SPI2_MOSI;
extern const Gpio_Pin LCD_RESET;
extern const Gpio_Pin LCD_DC;
extern const Gpio_Pin RX_Q;
extern const Gpio_Pin RX_I;
extern const Gpio_Pin encoderBee;
extern const Gpio_Pin encoderB;
extern const Gpio_Pin encoderP;
extern const Gpio_Pin dac1;
extern const Gpio_Pin dac2;
extern const Gpio_Pin AMP_SWITCH_A;
extern const Gpio_Pin AMP_SWITCH_B;
extern const Gpio_Pin IMP_BRIDGE_SWITCH_A;
extern const Gpio_Pin IMP_BRIDGE_SWITCH_B;
extern const Gpio_Pin TX_RF_SWITCH_A;
extern const Gpio_Pin TX_RF_SWITCH_B;
extern const Gpio_Pin AMP_POWER;
extern const Gpio_Pin FILTER_GAIN_POT_SCLK;
extern const Gpio_Pin FILTER_GAIN_POT_MOSI;
extern const Gpio_Pin GAIN_POT_NSS;
extern const Gpio_Pin IN_AMP_ENABLE;
extern const Gpio_Pin DAC_MUX;
extern const Gpio_Pin AUDIO_AMP_NSHTDWN;
extern const Gpio_Pin EARPHONE_NOT_INSERTED;
extern const Gpio_Pin SIDETONE; //Should be a timer pin.
extern const Gpio_Pin MIC_IN;
extern const Gpio_Pin MIC_SWITCH;
extern const Gpio_Pin PREAMP_POWER;
extern const Gpio_Pin MIC_BUTTON;
extern const Gpio_Pin PADDLE_THUMB_NO;
extern const Gpio_Pin PADDLE_INDEX_NO;
extern const Gpio_Pin PADDLE_THUMB_NC;
extern const Gpio_Pin PADDLE_INDEX_NC;
extern const Gpio_Pin FILTER_S0;
extern const Gpio_Pin FILTER_S1;
extern const Gpio_Pin TRX_SWITCH;
extern const Gpio_Pin I2C_SCL;
extern const Gpio_Pin I2C_SDA;
extern const Gpio_Pin FLIP_FLOP_ENABLE;
extern const Gpio_Pin RED_LED;
extern const Gpio_Pin CHARGE_POWER_SOURCE;
extern const Gpio_Pin FIVE_VOLT_REGULATOR_ENABLE;


/* USER CODE END Private defines */

void MX_GPIO_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /*__ pinoutConfig_H */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
