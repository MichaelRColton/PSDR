/* hal.h
 *
 * misc. h/w interfaces definition
 *
 * Copyright 2013 RPH Engineering, VPI Engineering
 */

#pragma once

#ifndef ADC_H_
#define ADC_H_

    #include "hal.h"
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_adc.h"
#include "stm32f4xx_hal_rcc.h"


__IO uint16_t uhADCxConvertedValue;
__IO uint16_t uhADCxConvertedValue2;
uint8_t adcConfigured;
uint16_t sampleIndex;
volatile uint8_t sampleRun;

	/* Definition for ADCx clock resources */
	#define ADCx                            ADC1
	#define ADCx_CLK_ENABLE()               __ADC1_CLK_ENABLE();
	#define ADCx_CHANNEL_GPIO_CLK_ENABLE()  __GPIOA_CLK_ENABLE()

	#define ADCx_FORCE_RESET()              __ADC_FORCE_RESET()
	#define ADCx_RELEASE_RESET()            __ADC_RELEASE_RESET()

	/* Definition for ADCx Channel Pin */
	#define ADCx_CHANNEL_PIN                GPIO_PIN_3
	#define ADCx_CHANNEL_GPIO_PORT          GPIOA

	/* Definition for ADCx's Channel */
	#define ADCx_CHANNEL                    ADC_CHANNEL_3

	/* Definition for ADCx's NVIC */
	#define ADCx_IRQn                      ADC_IRQn


	ADC_HandleTypeDef    AdcHandle;
	ADC_HandleTypeDef    AdcHandle2;
	ADC_ChannelConfTypeDef sConfig1;
	ADC_ChannelConfTypeDef sConfig2;
	uint8_t wrongThings;


	void initAdc(void);


#endif /* ADC_H_ */
