//
// This file is part of the GNU ARM Eclipse distribution.
// Copyright (c) 2014 Liviu Ionescu.
//

// ----------------------------------------------------------------------------

//#define ARM_MATH_CM4

#include "main.h"


// ----------------------------------------------------------------------------
//
// Standalone STM32F4 led blink sample (trace via NONE).
//
// In debug configurations, demonstrate how to print a greeting message
// on the trace device. In release configurations the message is
// simply discarded.
//
// Then demonstrates how to blink a led with 1Hz, using a
// continuous loop and SysTick delays.
//
// Trace support is enabled by adding the TRACE macro definition.
// By default the trace messages are forwarded to the NONE output,
// but can be rerouted to any device or completely suppressed, by
// changing the definitions required in system/src/diag/trace_impl.c
// (currently OS_USE_TRACE_ITM, OS_USE_TRACE_SEMIHOSTING_DEBUG/_STDOUT).
//

// ----- Timing definitions -------------------------------------------------

// Keep the LED on for 2/3 of a second.
#define BLINK_ON_TICKS  (TIMER_FREQUENCY_HZ * 2 / 3)
#define BLINK_OFF_TICKS (TIMER_FREQUENCY_HZ - BLINK_ON_TICKS)

// ----- main() ---------------------------------------------------------------

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

void ddsPrefix();
void sendToDds(uint16_t data1, uint16_t data2);

__IO long long millis = 0;



void setupPeripheralPower()
{
	__GPIOA_CLK_ENABLE();
	__GPIOB_CLK_ENABLE();
	__GPIOC_CLK_ENABLE();
	__GPIOD_CLK_ENABLE();
}




void doNothing()
{
	static int cap = 0;
	cap++;
}

void
SysTick_Handler (void)
{
	millis++;
  timer_tick ();
  Tick();
}

int clickMultiply;
int Max;
int Min;
int Pos;
int Position;
int Position2;
int isFwd;

#define EncoderPinA 20	// Rotary Encoder Left Pin //
#define EncoderPinB 19	// Rotary Encoder Right Pin //
#define EncoderPinP 21	// Rotary Encoder Click //


	Encoder()
	{
		Position = 0;
		Position2 = 0;
		Max = 127;
		Min = 0;
		clickMultiply = 100;
	}

	void Tick(void)
	{
		Position2 = (HAL_GPIO_ReadPin(encoderB.port, encoderB.pin) * 2) + HAL_GPIO_ReadPin(encoderA.port, encoderA.pin);;
		if (Position2 != Position)
		{
			isFwd = ((Position == 0) && (Position2 == 1)) || ((Position == 1) && (Position2 == 3)) ||
				((Position == 3) && (Position2 == 2)) || ((Position == 2) && (Position2 == 0));
			if (!HAL_GPIO_ReadPin(encoderP.port, encoderP.pin)) { if (isFwd) Pos += clickMultiply; else Pos -= clickMultiply; }
				else { if (isFwd) Pos++; else Pos--; }
			//if (Pos < Min) Pos = Min;
			//if (Pos > Max) Pos = Max;
		}
		Position = Position2;
	}

	int getPos(void)
	{
		return (Pos/2);
	}

	void setMinMax(int _Min, int _Max)
	{
		Min = _Min*4;
		Max = _Max*4;
		if (Pos < Min) Pos = Min;
		if (Pos > Max) Pos = Max;
	}

	void setClickMultiply(int _clickMultiply)
	{
		clickMultiply = _clickMultiply;
	}




	float	samples[512];
	const int FFT_SIZE = 256;

	void captureSamples()
	{
		if(adcConfigured)
		{
			if(!sampleRun)
			{
				adcStartConversion();
				samples[sampleIndex*2] = ((float)(uhADCxConvertedValue - 2048)/4096.0); // - 2048;
				samples[sampleIndex*2 + 1] = ((float)(uhADCxConvertedValue2 - 2048)/4096.0); // - 2048;//0.0;
				sampleIndex++;
				if(sampleIndex >= 256)
				{
					sampleRun = 1;
					sampleIndex = 0;
				}
			}
		}
	}




int
main(int argc, char* argv[])
{
	HAL_Init();
	//HAL_RCC_OscConfig()
//	RCC_ClkInitStruct clockInitStructure;
//	clockInitStructure.
//	HAL_RCC_ClockConfig()
//	SystemClock_Config();

//	void (*systicker)();
//	systicker = &doNothing();
//
//
//	HAL_SYSTICK_Callback(systicker);
	//hal_setSysTickCallback(doNothing());

  // Send a greeting to the trace device (skipped on Release).
  trace_puts("Hello ARM World!");

  // At this stage the system clock should have already been configured
  // at high speed.
  trace_printf("System clock: %uHz\n", SystemCoreClock);

  setupPeripheralPower();

//initDdsPins();
hal_setupPins();
spi_init();

  timer_start();

  blink_led_init();
        blink_led_on();
  uint32_t seconds = 0;


Adafruit_ILI9340_begin();
Adafruit_ILI9340_setRotation(3);
Adafruit_GFX_fillScreen(ILI9340_BLACK);
Adafruit_GFX_fillScreen(ILI9340_BLACK);

Encoder();

				char chrisABaby[] = "Chris a baby!";
				int j;
				Adafruit_GFX_setTextSize(3);
				Adafruit_GFX_setTextWrap(1);
				Adafruit_GFX_setTextColor(ILI9340_WHITE, ILI9340_BLACK);
				char freqChar[14];
				sprintf(&freqChar, "%8d", 28000000);
				  long counter = 9700000;


				  long lastCounter = counter;
				  int encoderPos, encoderLastPos;
				  char lastFreqChar[] = {'$','$','$','$','$','$','$','$','$','$','$','$','$','$',};



					 initAdc();

					 adcConfigured = 1;

					adcStartConversion();

					float magnitudes[256];

						uint8_t rawGradient[96] =
								{
										30,30,11,
										13,13,100,
										18,13,107,
										27,13,122,
										40,13,140,
										56,13,160,
										72,13,175,
										88,13,188,
										106,13,201,
										125,13,214,
										145,13,226,
										163,13,234,
										177,15,238,
										185,25,237,
										192,41,233,
										198,58,227,
										205,78,219,
										209,98,210,
										215,120,202,
										219,141,193,
										224,162,186,
										228,181,180,
										231,197,176,
										236,210,175,
										240,225,180,
										244,234,190,
										247,242,203,
										249,246,217,
										251,249,232,
										253,252,245,
										255,254,253,
										255,255,254
								};

								uint16_t gradient[32];

								int k;
								for(k = 0; k <32; k++)
								{
									gradient[k] = Adafruit_ILI9340_Color565(0.5 * rawGradient[k*3], 0.5 * rawGradient[k*3+1], 0.5 * rawGradient[k*3+2]);
								}


								float mags;
											uint8_t waterfallScanLine = 0;


Adafruit_GFX_drawTriangle(121,119,131,124,131,114,ILI9340_WHITE);


	uint16_t freqVOffset = 120 - (8*3/2);
	uint16_t freqHOffset = 142;


	Adafruit_GFX_setCursor(freqHOffset + 18*2, freqVOffset + 0);
	Adafruit_GFX_write('.');
	Adafruit_GFX_setCursor(freqHOffset + 18*6, freqVOffset + 0);
	Adafruit_GFX_write('.');


//TIM_setup();
//TIM_Config();
TIM_Try();
	Adafruit_ILI9340_setVerticalScrollDefinition(200,120,0);
	long long timeMeasurement = 0;
while(1)
{

	captureSamples();

float fftMaxMax = 0;
	if(sampleRun)
				{

					timeMeasurement = millis;
					arm_cfft_radix4_instance_f32 fft_inst;
					//arm_cfft_radix4_init_q31(&fft_inst, FFT_SIZE, 0, 1);
					arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 1);

					//arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 2);
					arm_cfft_radix4_f32(&fft_inst, samples);
					// Calculate magnitude of complex numbers output by the FFT.
					arm_cmplx_mag_f32(samples, magnitudes, FFT_SIZE);
					timeMeasurement = millis - timeMeasurement;

					float fftMax = 0;
					float fftMin = 100;
					float logMax;
					uint8_t i;
					for(i = 0; i < 255; i++)
					{
						float mags = magnitudes[i];
						if(mags > fftMax) fftMax = mags;
						if(mags < fftMin) fftMin = mags;
					}
					//logMax = log2(fftMax);

					if(fftMax > fftMaxMax) fftMaxMax = fftMax;
					logMax = log2(fftMaxMax);


					//TODOne: SWITCH THESE AND FLIP THEM. So that higher frequencies appear higher on screen.
					//TODO: Got rid of the first bin because it's just DC offset, right?
					//but now narrow signal can disappear when they are right at the center....
					//Will that be better when I lower the sample frequency? Maybe I should do that next.
				for(i = 1; i < 120; i++)
				{
					mags = (log2(magnitudes[i] + 1)) / fftMaxMax * 32;
					//mags = magnitudes[i] / fftMaxMax * 32;
					Adafruit_ILI9340_drawPixel(waterfallScanLine, (120 - i), gradient[(uint8_t) mags]);
				}

				for(i = 135; i < 255; i++)
				{
					mags = (log2(magnitudes[i] + 1)) / fftMaxMax * 32;
					//mags = magnitudes[i] / fftMaxMax * 32;
					Adafruit_ILI9340_drawPixel(waterfallScanLine, 359 - (i - 15), gradient[(uint8_t) mags]);
				}

				waterfallScanLine++;
				if(waterfallScanLine > 119) waterfallScanLine = 0;
				Adafruit_ILI9340_setVertialScrollStartAddress((119 - waterfallScanLine) + 200);




	//			Adafruit_GFX_setCursor(3, 200);
	//			Adafruit_GFX_setTextColor(ILI9340_CYAN, ILI9340_RED);
	//			Adafruit_GFX_setTextSize(1);
	//			//char chrisABaby[] = {'C','h','r','i','s',' ','A',' ','B','a','b','y'};
	//			char chrisABaby[] = "Chris a baby!";
	//			int j;
	//			for(j = 0; j < 13; j++)
	//			{
	//				Adafruit_GFX_write(chrisABaby[j]);
	//			}



				sampleRun = 0;

				}



	//Adafruit_ILI9340_fillScreen(ILI9340_BLUE);


//counter = 9700000;
	//Adafruit_GFX_drawLine(oscilloscope,50,oscilloscope, 240, ILI9340_BLACK);
	//Adafruit_ILI9340_drawFastVLine(oscilloscope, 50, 210, ILI9340_BLACK);
//adcStartConversion();
//pixV = (uhADCxConvertedValue - (4096/2));
//Adafruit_ILI9340_drawPixel(oscilloscope++, pixV + 100, ILI9340_CYAN);


	if(counter != lastCounter)
	{

		setFreq(counter);
		sprintf(&freqChar, "%8d", counter);

		if(freqChar[0] != lastFreqChar[0])
		{
			Adafruit_GFX_setCursor(freqHOffset + 18*0, freqVOffset + 0);
			Adafruit_GFX_write(freqChar[0]);
		}
		if(freqChar[1] != lastFreqChar[1])
		{
			Adafruit_GFX_setCursor(freqHOffset + 18*1, freqVOffset + 0);
			Adafruit_GFX_write(freqChar[1]);
		}

		if(freqChar[2] != lastFreqChar[2])
		{
			Adafruit_GFX_setCursor(freqHOffset + 18*3, freqVOffset + 0);
			Adafruit_GFX_write(freqChar[2]);
		}
		if(freqChar[3] != lastFreqChar[3])
		{
			Adafruit_GFX_setCursor(freqHOffset + 18*4, freqVOffset + 0);
			Adafruit_GFX_write(freqChar[3]);
		}
		if(freqChar[4] != lastFreqChar[4])
		{
			Adafruit_GFX_setCursor(freqHOffset + 18*5, freqVOffset + 0);
			Adafruit_GFX_write(freqChar[4]);
		}

		if(freqChar[5] != lastFreqChar[5])
		{
			Adafruit_GFX_setCursor(freqHOffset + 18*7, freqVOffset + 0);
			Adafruit_GFX_write(freqChar[5]);
		}
		if(freqChar[6] != lastFreqChar[6])
		{
			Adafruit_GFX_setCursor(freqHOffset + 18*8, freqVOffset + 0);
			Adafruit_GFX_write(freqChar[6]);
		}
		if(freqChar[7] != lastFreqChar[7])
		{
			Adafruit_GFX_setCursor(freqHOffset + 18*9, freqVOffset + 0);
			Adafruit_GFX_write(freqChar[7]);
		}

		lastCounter = counter;
		strcpy(lastFreqChar, freqChar);
	}


encoderPos = getPos();
if(encoderPos != encoderLastPos)
{

	counter += 1000 * (encoderLastPos - encoderPos);

	//Adafruit_GFX_setCursor(0, 100);
	//char encoderChar[5];
	//sprintf(&encoderChar, "%4d", encoderPos);
	//Adafruit_GFX_write(encoderChar[0]);
	//Adafruit_GFX_write(encoderChar[1]);
	//Adafruit_GFX_write(encoderChar[2]);
	//Adafruit_GFX_write(encoderChar[3]);
	encoderLastPos = encoderPos;
}

//Adafruit_ILI9340_fillScreen(ILI9340_YELLOW);
				//char chrisABaby[] = {'C','h','r','i','s',' ','A',' ','B','a','b','y'};

//				for(j = 0; j < 13; j++)
//				{
//					Adafruit_GFX_write(freqChar[j]);
//				}

//while(1);

}











  while(1)
  {

  }

counter = 0;

  // Infinite loop
  while (1)
    {
      blink_led_on();
      timer_sleep(BLINK_ON_TICKS);
      //HAL_GPIO_WritePin(ddsReset.port, ddsReset.pin, 1);

      blink_led_off();
      timer_sleep(BLINK_OFF_TICKS);
      //HAL_GPIO_WritePin(ddsReset.port, ddsReset.pin, 0);

      //sendToDds(counter++);


      //++seconds;

      // Count seconds on the trace device.
      //trace_printf("Second %u\n", seconds);
    }
  // Infinite loop, never return.
}

//TIM_TimeBaseInitTypeDef timeBaseStructure;

TIM_OC_InitTypeDef   tsConfig;
#define  PULSE1_VALUE       40961       /* Capture Compare 1 Value  */
uint32_t uwPrescalerValue = 0;
void TIM_setup()
{
	  /*##-1- Configure the TIM peripheral #######################################*/
  /* -----------------------------------------------------------------------
    In this example TIM3 input clock (TIM3CLK) is set to 2 * APB1 clock (PCLK1),
    since APB1 prescaler is different from 1.
      TIM3CLK = 2 * PCLK1
      PCLK1 = HCLK / 4
      => TIM3CLK = HCLK / 2 = SystemCoreClock /2
    To get TIM3 counter clock at 60 KHz, the Prescaler is computed as following:
    Prescaler = (TIM3CLK / TIM3 counter clock) - 1
    Prescaler = ((SystemCoreClock /2) /60 KHz) - 1

    Note:
     SystemCoreClock variable holds HCLK frequency and is defined in system_stm32f4xx.c file.
     Each time the core clock (HCLK) changes, user had to update SystemCoreClock
     variable value. Otherwise, any configuration based on this variable will be incorrect.
     This variable is updated in three ways:
      1) by calling CMSIS function SystemCoreClockUpdate()
      2) by calling HAL API function HAL_RCC_GetSysClockFreq()
      3) each time HAL_RCC_ClockConfig() is called to configure the system clock frequency
  ----------------------------------------------------------------------- */

  /* Compute the prescaler value to have TIM3 counter clock equal to 60 KHz */
  uwPrescalerValue = (uint32_t) ((SystemCoreClock/2) / 60000) - 1;

  /* Set TIMx instance */
    TimHandle.Instance = TIM3; //TIMx;

  /* Initialize TIM3 peripheral as follow:
       + Period = 65535
       + Prescaler = (SystemCoreClock/2)/60000
       + ClockDivision = 0
       + Counter direction = Up
  */
  TimHandle.Init.Period = 65535;
  TimHandle.Init.Prescaler = uwPrescalerValue;
  TimHandle.Init.ClockDivision = 0;
  TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
  if(HAL_TIM_OC_Init(&TimHandle) != HAL_OK)
  {
    /* Initialization Error */
    //Error_Handler();
	  doNothing();
  }

  /*##-2- Configure the PWM channels #########################################*/
  /* Common configuration */
  tsConfig.OCMode = TIM_OCMODE_TIMING;
  tsConfig.OCPolarity = TIM_OCPOLARITY_HIGH;
  tsConfig.OCFastMode = TIM_OCFAST_DISABLE;

  /* Set the pulse value for channel 1 */
  tsConfig.Pulse = PULSE1_VALUE;
  if(HAL_TIM_OC_ConfigChannel(&TimHandle, &tsConfig, TIM_CHANNEL_1) != HAL_OK)
  {
    /* Initialization Error */
    //Error_Handler();
	  doNothing();
  }

  /*##-4- Start the Output Compare mode in interrupt mode ####################*/
  /* Start Channel1 */
  if(HAL_TIM_OC_Start_IT(&TimHandle, TIM_CHANNEL_1) != HAL_OK)
  {
    /* Initialization Error */
    //Error_Handler();
	  doNothing();
  }

}


///**
//  * @brief  Configures the TIM IRQ Handler.
//  * @param  None
//  * @retval None
//  */
void TIM_Config(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;

  /* TIM3 clock enable */
  //RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
  __TIM3_CLK_ENABLE();

  /* Enable the TIM3 gloabal Interrupt */
  NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);


}

TIM_TypeDef timTimBase;
//TIM_HandleTypeDef timHandle;
/* Definition for TIMx's NVIC */
#define TIMx_IRQn                      TIM3_IRQn
#define TIMx_IRQHandler                TIM3_IRQHandler
void TIM_Try(void)
{

	  uwPrescalerValue = (uint32_t) ((SystemCoreClock/2) / 60000) - 1;

    //NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
	__TIM3_CLK_ENABLE();
	TimHandle.Instance = TIM3;
	TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
	TimHandle.Init.Period = 65535;
	TimHandle.Init.Prescaler = uwPrescalerValue;
	TimHandle.Init.ClockDivision = 0;
	HAL_TIM_Base_Init(&TimHandle);

	HAL_TIM_Base_Start_IT(&TimHandle);

	  /*##-2- Configure the NVIC for TIMx #########################################*/
	  /* Set the TIMx priority */
	  HAL_NVIC_SetPriority(TIMx_IRQn, 0, 1);

	  /* Enable the TIMx global Interrupt */
	  HAL_NVIC_EnableIRQ(TIMx_IRQn);


//	int tim3;
//	while(1)
//	{
//		 tim3 = timTryHandle.Instance->CNT;
//	}

}

int ledState = 0;
HAL_TIM_PeriodElapsedCallback(htim)
{
	doNothing();
	if(ledState)
	{
		blink_led_off();
		ledState = 0;
	}
	else
	{
		blink_led_on();
		ledState = 1;
	}
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
