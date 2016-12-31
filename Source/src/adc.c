#include "adc.h"


void initAdc()
	{
	uhADCxConvertedValue1 = 0;
	uhADCxConvertedValue2 = 0;
	adcConfigured = 0;
	sampleIndex = 100; //probably makes more sense for this to be in main for
	sampleRun = 0;
	wrongThings = 0;


		__ADC1_CLK_ENABLE();
		__ADC2_CLK_ENABLE();
		__ADC3_CLK_ENABLE();
		__GPIOA_CLK_ENABLE();
		__GPIOB_CLK_ENABLE();
		__GPIOC_CLK_ENABLE();

		//__ADC_FORCE_RESET();
		//__ADC_RELEASE_RESET();
		__HAL_RCC_ADC_FORCE_RESET();
		__HAL_RCC_ADC_RELEASE_RESET();

		  AdcHandle1.Instance          = ADC1;

		  AdcHandle1.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV4;
		  AdcHandle1.Init.Resolution = ADC_RESOLUTION12b;
		  AdcHandle1.Init.ScanConvMode = DISABLE;
		  AdcHandle1.Init.ContinuousConvMode = ENABLE;
		  AdcHandle1.Init.DiscontinuousConvMode = DISABLE;
		  AdcHandle1.Init.NbrOfDiscConversion = 0;
		  AdcHandle1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
		  AdcHandle1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
		  AdcHandle1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
		  AdcHandle1.Init.NbrOfConversion = 1;
		  AdcHandle1.Init.DMAContinuousRequests = DISABLE;
		  AdcHandle1.Init.EOCSelection = DISABLE;

		  if(HAL_ADC_Init(&AdcHandle1) != HAL_OK)
		  {
		    /* Initiliazation Error */
		    //Error_Handler();
			  wrongThings++;
		  }

		  AdcHandle2.Instance          = ADC2;

		  AdcHandle2.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV4;
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

		  AdcHandle3.Instance          = ADC3;

		  AdcHandle3.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV4;
		  AdcHandle3.Init.Resolution = ADC_RESOLUTION12b;
		  AdcHandle3.Init.ScanConvMode = DISABLE;
		  AdcHandle3.Init.ContinuousConvMode = ENABLE;
		  AdcHandle3.Init.DiscontinuousConvMode = DISABLE;
		  AdcHandle3.Init.NbrOfDiscConversion = 0;
		  AdcHandle3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
		  AdcHandle3.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_CC1;
		  AdcHandle3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
		  AdcHandle3.Init.NbrOfConversion = 1;
		  AdcHandle3.Init.DMAContinuousRequests = DISABLE;
		  AdcHandle3.Init.EOCSelection = DISABLE;
		  if(HAL_ADC_Init(&AdcHandle3) != HAL_OK)
		  {
		    /* Initiliazation Error */
		    //Error_Handler();
			  wrongThings++;
		  }

		  sConfig1.Channel = ADC_MIC_CHANNEL;
		  sConfig1.Rank = 1;
		  sConfig1.SamplingTime = ADC_SAMPLETIME_15CYCLES;
		  sConfig1.Offset = 0;

		  sConfig2.Channel = ADC_RX_I_CHANNEL;
		  sConfig2.Rank = 1;
		  sConfig2.SamplingTime = ADC_SAMPLETIME_15CYCLES;
		  sConfig2.Offset = 0;

		  sConfig3.Channel = ADC_RX_Q_CHANNEL;
		  sConfig3.Rank = 1;
		  sConfig3.SamplingTime = ADC_SAMPLETIME_15CYCLES;
		  sConfig3.Offset = 0;

		  if(HAL_ADC_ConfigChannel(&AdcHandle1, &sConfig1) != HAL_OK)
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

		  if(HAL_ADC_ConfigChannel(&AdcHandle3, &sConfig3) != HAL_OK)
		  {
		    /* Channel Configuration Error */
		    //Error_Handler();
			  wrongThings++;
		  }
	}

	void adcGetConversion(void)
	{
		 HAL_StatusTypeDef result =  HAL_ADC_PollForConversion(&AdcHandle1, 100);
		  result = HAL_ADC_PollForConversion(&AdcHandle2, 100);
		  result = HAL_ADC_PollForConversion(&AdcHandle3, 100);

		  if (result == HAL_OK)
		    {
		      //while(HAL_ADC_GetState(&AdcHandle1) != HAL_ADC_STATE_REG_EOC);

		     		    /* Check if the continous conversion of regular channel is finished */
//		    if(HAL_ADC_GetState(&AdcHandle1) == 0x300 /*HAL_ADC_STATE_REG_EOC*/
//		    		&& HAL_ADC_GetState(&AdcHandle2) == 0x300 /*HAL_ADC_STATE_REG_EOC*/
//		    		&& HAL_ADC_GetState(&AdcHandle3) == 0x300 /*HAL_ADC_STATE_REG_EOC*/)

		      if(HAL_IS_BIT_SET(HAL_ADC_GetState(&AdcHandle1), HAL_ADC_STATE_REG_EOC) &&
		          HAL_IS_BIT_SET(HAL_ADC_GetState(&AdcHandle2), HAL_ADC_STATE_REG_EOC) &&
		          HAL_IS_BIT_SET(HAL_ADC_GetState(&AdcHandle3), HAL_ADC_STATE_REG_EOC))


		    {
		      /*##-5- Get the converted value of regular channel  ########################*/
		      uhADCxConvertedValue1 = HAL_ADC_GetValue(&AdcHandle1);
		      uhADCxConvertedValue2 = HAL_ADC_GetValue(&AdcHandle2);
		      uhADCxConvertedValue3 = HAL_ADC_GetValue(&AdcHandle3);
		    }
		    }


	}


	void adcStartConversion(void)
	{
		if(HAL_ADC_Start(&AdcHandle1) != HAL_OK)
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

		if(HAL_ADC_Start(&AdcHandle3) != HAL_OK)
		{
			/* Start Conversation Error */
			//Error_Handler();
			wrongThings++;
		}
	}


	//__IO uint16_t uhADCxConvertedValue1 = 0;


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
	  uhADCxConvertedValue1 = HAL_ADC_GetValue(AdcHandle);
	}




