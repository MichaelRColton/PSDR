#include "adc.h"


void initAdc()
	{
	uhADCxConvertedValue = 0;
	uhADCxConvertedValue2 = 0;
	adcConfigured = 0;
	sampleIndex = 0;
	sampleRun = 0;
	wrongThings = 0;


		__ADC1_CLK_ENABLE();
		__ADC2_CLK_ENABLE();
		__GPIOA_CLK_ENABLE(); //PA1 and PA2

		__ADC_FORCE_RESET();
		__ADC_RELEASE_RESET();

		  AdcHandle.Instance          = ADCx;

		  AdcHandle.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2;
		  AdcHandle.Init.Resolution = ADC_RESOLUTION12b;
		  AdcHandle.Init.ScanConvMode = DISABLE;
		  AdcHandle.Init.ContinuousConvMode = ENABLE;
		  AdcHandle.Init.DiscontinuousConvMode = DISABLE;
		  AdcHandle.Init.NbrOfDiscConversion = 0;
		  AdcHandle.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
		  AdcHandle.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
		  AdcHandle.Init.DataAlign = ADC_DATAALIGN_RIGHT;
		  AdcHandle.Init.NbrOfConversion = 1;
		  AdcHandle.Init.DMAContinuousRequests = DISABLE;
		  AdcHandle.Init.EOCSelection = DISABLE;

		  if(HAL_ADC_Init(&AdcHandle) != HAL_OK)
		  {
		    /* Initiliazation Error */
		    //Error_Handler();
			  wrongThings++;
		  }

		  AdcHandle2.Instance          = ADC2;

		  AdcHandle2.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2;
		  AdcHandle2.Init.Resolution = ADC_RESOLUTION12b;
		  AdcHandle2.Init.ScanConvMode = DISABLE;
		  AdcHandle2.Init.ContinuousConvMode = ENABLE;
		  AdcHandle2.Init.DiscontinuousConvMode = DISABLE;
		  AdcHandle2.Init.NbrOfDiscConversion = 0;
		  AdcHandle2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
		  AdcHandle2.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
		  AdcHandle2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
		  AdcHandle2.Init.NbrOfConversion = 1;
		  AdcHandle2.Init.DMAContinuousRequests = DISABLE;
		  AdcHandle2.Init.EOCSelection = DISABLE;
		  if(HAL_ADC_Init(&AdcHandle2) != HAL_OK)
		  {
		    /* Initiliazation Error */
		    //Error_Handler();
			  wrongThings++;
		  }


		  sConfig1.Channel = ADCx_CHANNEL;
		  sConfig1.Rank = 1;
		  sConfig1.SamplingTime = ADC_SAMPLETIME_3CYCLES;
		  sConfig1.Offset = 0;

		  sConfig2.Channel = ADC_CHANNEL_2;
		  sConfig2.Rank = 1;
		  sConfig2.SamplingTime = ADC_SAMPLETIME_3CYCLES;
		  sConfig2.Offset = 0;


		  if(HAL_ADC_ConfigChannel(&AdcHandle, &sConfig1) != HAL_OK)
		  {
		    /* Channel Configuration Error */
		    //Error_Handler();
			  wrongThings++;
		  }

		  if(HAL_ADC_ConfigChannel(&AdcHandle2, &sConfig2) != HAL_OK)
		  {
		    /* Channel Configuration Error */
		    //Error_Handler();
			  wrongThings++;
		  }
	}



	void adcStartConversion()
	{
		  if(HAL_ADC_Start(&AdcHandle) != HAL_OK)
		  {
		    /* Start Conversation Error */
		    //Error_Handler();
		    wrongThings++;
		  }

		  if(HAL_ADC_Start(&AdcHandle2) != HAL_OK)
		  {
		    /* Start Conversation Error */
		    //Error_Handler();
		    wrongThings++;
		  }

		  HAL_ADC_PollForConversion(&AdcHandle, 10);
		  HAL_ADC_PollForConversion(&AdcHandle2, 10);

		    /* Check if the continous conversion of regular channel is finished */
		    if(HAL_ADC_GetState(&AdcHandle) == HAL_ADC_STATE_EOC_REG && HAL_ADC_GetState(&AdcHandle2) == HAL_ADC_STATE_EOC_REG)
		    {
		      /*##-5- Get the converted value of regular channel  ########################*/
		      uhADCxConvertedValue = HAL_ADC_GetValue(&AdcHandle);
		      uhADCxConvertedValue2 = HAL_ADC_GetValue(&AdcHandle2);
		    }



//		  if(HAL_ADC_Start_IT(&AdcHandle) != HAL_OK)
//		  {
//		    /* Start Conversation Error */
//		    //Error_Handler();
//			  wrongThings++;
//		  }
	}


	//__IO uint16_t uhADCxConvertedValue = 0;


	/**
	  * @brief  Conversion complete callback in non blocking mode
	  * @param  AdcHandle : AdcHandle handle
	  * @note   This example shows a simple way to report end of conversion, and
	  *         you can add your own implementation.
	  * @retval None
	  */
	void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* AdcHandle)
	{
	  /* Get the converted value of regular channel */
	  uhADCxConvertedValue = HAL_ADC_GetValue(AdcHandle);
	}




