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
//#pragma GCC diagnostic ignored "-Wunused-parameter"
//#pragma GCC diagnostic ignored "-Wmissing-declarations"
//#pragma GCC diagnostic ignored "-Wreturn-type"
#pragma GCC diagnostic ignored "-Wfloat-conversion"

static void CPU_CACHE_Enable(void);
void dac1SetValue(uint16_t value);
void dac2SetValue(uint16_t value);
//void ddsPrefix(void);
//void sendToDds(uint16_t data1, uint16_t data2);
#define FFT_SIZE 256 //supported sizes are 16, 64, 256, 1024
#define FFT_BUFFER_SIZE  512 //double the FFT_SIZE above.
__IO long long millis = 0;


float afGain = 1.0;
float afGainLast = 2.0;
float afGainStep = 0.1;
float afGainMax = 25.0;

uint16_t maxAmplitude = 0;

float fftFilterCoeficient[FFT_BUFFER_SIZE];
float filterTemp[FFT_BUFFER_SIZE];
uint16_t filterKernelLength = 100; //what's a good value? How does it relate to the FFT size?

uint16_t menuPos = 0;
uint16_t menuEncoderTicks = 0;
uint16_t menuLastPos = 1;
uint16_t menuCount = 11;
uint32_t frequencyDialMultiplier = 1;

long vfoAFrequency = 7030000;
long vfoALastFreq = 0;
int encoderPos, encoderLastPos;

int16_t filterUpperLimit = 68;
int16_t filterLastUpperLimit = 2;
int16_t filterLowerLimit = 0;
int16_t filterLastLowerLimit = 2;
uint8_t mode = 0;
uint8_t modeLast = 2;

float agcLevel = 0;
float agcScale = 160; //Higher is lower volume.. for now
float txAgcLevel = 0;


int ifShift = 0;

float fftMaxMaxMax = 20;
float fftMaxMaxMin = 0.2;

int transmitting = 0;
unsigned int tone = 0;

uint8_t displayUpdating = 0;

///** System Clock Configuration
//*/
//void SystemClock_Config(void)
//{
//
//  RCC_ClkInitTypeDef RCC_ClkInitStruct;
//  RCC_OscInitTypeDef RCC_OscInitStruct;
//
//  __PWR_CLK_ENABLE();
//
//  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1); //needed?
//
//  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
//  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
//  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
//  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
//  RCC_OscInitStruct.PLL.PLLM = 26;
//  RCC_OscInitStruct.PLL.PLLN = 432;
//  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
//  RCC_OscInitStruct.PLL.PLLQ = 9;
//  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
//
//  HAL_StatusTypeDef ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
//  if(ret != HAL_OK)
//    {
//      while(1) { ; }
//    }
//
//  /* Activate the OverDrive to reach the 216 MHz Frequency */
//    ret = HAL_PWREx_EnableOverDrive();
//    if(ret != HAL_OK)
//    {
//      while(1) { ; }
//    }
//
//  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK |RCC_CLOCKTYPE_PCLK1
//                              |RCC_CLOCKTYPE_PCLK2;
//  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
//  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
//  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
//  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
//
//  ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7);
//  if(ret != HAL_OK)
//    {
//      while(1) { ; }
//    }
//
//}

void polarToRect(float m, float a, float32_t* x, float32_t* y)
{
	*y = m * arm_sin_f32(a);
	*x = m * arm_cos_f32(a);
}

void populateCoeficients(int bandwidth, int sideband, int offset)
{
	//Chapter 17 of DSP Guide* //TODO: Make a bibliography!

	//1. Take as input, desired filter response in array, both magnitude and phase (it's okay for phase to be zero)
	//	 Looks like magnitude can be any non-negative value. First and last values of Phase must be zero.
	//2. Convert to rectangular form. ***I really wish there was a built in function for this :<
	//3. Run through an inverse FFT.
	//4. Shift
	//5. Truncate
	//6. Window
	//7. Reverse the FFT in preparation for FFT Convolution?

	uint16_t filterKernelLength = 100; //what's a good value? How does it relate to the FFT size?

	//1:
	//sideband: 0 = LSB, 1 = USB, 2 = Both (AM)
	//I think the code below is all wrong. At least for LSB, if the magnitude is zero, then phase doesn't matter, yeah?
	if(sideband > 2) return; //Error
	int i;
	for(i = 0; i < FFT_BUFFER_SIZE; i++)
	{
		switch(sideband)
		{
		case 0:
			if((i > FFT_BUFFER_SIZE - (offset + bandwidth)) && (i < FFT_BUFFER_SIZE - offset))
				fftFilterCoeficient[i] = 1;
			else
				fftFilterCoeficient[i] = 0;
			break;
		case 1:
			if((i > offset) && (i < offset + bandwidth))
				fftFilterCoeficient[i] = 1;
			else
				fftFilterCoeficient[i] = 0;
			break;
		case 2:
			if(((i > FFT_BUFFER_SIZE - (offset + bandwidth)) && (i < FFT_BUFFER_SIZE - offset))
					|| ((i > offset) && (i < offset + bandwidth)))
				fftFilterCoeficient[i] = 1;
			else
				fftFilterCoeficient[i] = 0;
			break;
		}
	}
	fftFilterCoeficient[FFT_BUFFER_SIZE / 2] = 0;
	fftFilterCoeficient[FFT_BUFFER_SIZE - 1] = 0;

	return; //Skipping all the later stuff doesn't seem to make a huge difference yet...

	//2:
//	float x, y;
//	for(i = 0; i < FFT_SIZE; i++)
//	{
//		polarToRect(fftFilterCoeficient[i], fftFilterCoeficient[FFT_BUFFER_SIZE - 1 - i], &x, &y);
//		fftFilterCoeficient[i] = x;
//		fftFilterCoeficient[FFT_BUFFER_SIZE - 1 - i] = y;
//	}

	//3:
	arm_cfft_radix4_instance_f32 fft_co;
	arm_cfft_radix4_init_f32(&fft_co, FFT_SIZE, 1, 1);
	arm_cfft_radix4_f32(&fft_co, fftFilterCoeficient);

	//4:
	int index;


	for (i = 0; i < FFT_BUFFER_SIZE; i++)
	{
		index = i + filterKernelLength/2;
		if(index > FFT_BUFFER_SIZE - 1) index = index - FFT_BUFFER_SIZE;
		filterTemp[index] = fftFilterCoeficient[i];
	}

	for(i = 0; i < FFT_BUFFER_SIZE; i++)
	{
		fftFilterCoeficient[i] = filterTemp[i];
	}

	//5 & 6:
	for(i = 0; i < FFT_BUFFER_SIZE; i++)
	{
		if(i <= filterKernelLength) fftFilterCoeficient[i] =
				fftFilterCoeficient[i] * (0.54 - 0.46 * arm_cos_f32(2* 3.14159265*i/filterKernelLength));
		if(i > filterKernelLength) fftFilterCoeficient[i] = 0;
	}

//	arm_cfft_radix4_instance_f32 fft_co;
	arm_cfft_radix4_init_f32(&fft_co, FFT_SIZE, 0, 1);
	arm_cfft_radix4_f32(&fft_co, fftFilterCoeficient);


//	for(i = 0; i < FFT_SIZE; i++)
//	{
//		filterTemp[i] = fftFilterCoeficient[i * 2];
//		filterTemp[FFT_BUFFER_SIZE - 1 - i] = fftFilterCoeficient[i * 2 + 1];
//	}
//
//	for(i = 0; i < FFT_BUFFER_SIZE; i++)
//	{
//		fftFilterCoeficient[i] = filterTemp[i];
//	}
}


void applyCoeficient(float *samples, int shift)
{
	//See DSP Guide Chapter 9 (Equation 9-1)
	int i;
	for(i = 0; i < FFT_SIZE; i++)
	{
		filterTemp[i * 2] = samples[i * 2] * fftFilterCoeficient[i * 2] - samples[i * 2 + 1] * fftFilterCoeficient[i * 2 + 1];
		filterTemp[i * 2 + 1] = samples[i * 2 + 1] * fftFilterCoeficient[i * 2 + 1] + samples[i * 2] * fftFilterCoeficient[i * 2];
	}

	int shifter;
	for(i = 0; i < FFT_BUFFER_SIZE; i++)
	{
		shifter = i + 2 * shift;
		if(i < 0) samples[i] = 0;
		if(i > FFT_BUFFER_SIZE - 1) samples[i] = 0;
		samples[i] = filterTemp[i + 2 * shift];
	}
}


void setupPeripheralPower()
{
	__GPIOA_CLK_ENABLE();
	__GPIOB_CLK_ENABLE();
	__GPIOC_CLK_ENABLE();
	__GPIOD_CLK_ENABLE();
	__GPIOE_CLK_ENABLE();
	__DMA1_CLK_ENABLE();
	__DMA2_CLK_ENABLE();
	__I2C2_CLK_ENABLE();
}

void configDMA(SPI_HandleTypeDef *hspi)
{
//	  static DMA_HandleTypeDef hdma_tx;
//	  static DMA_HandleTypeDef hdma_rx;
//
//
//	hdma_tx.Instance                 = SPIx_TX_DMA_STREAM;
//
//	  hdma_tx.Init.Channel             = SPIx_TX_DMA_CHANNEL;
//	  hdma_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
//	  hdma_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
//	  hdma_tx.Init.MemInc              = DMA_MINC_ENABLE;
//	  hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
//	  hdma_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
//	  hdma_tx.Init.Mode                = DMA_NORMAL;
//	  hdma_tx.Init.Priority            = DMA_PRIORITY_LOW;
//	  hdma_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
//	  hdma_tx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
//	  hdma_tx.Init.MemBurst            = DMA_MBURST_INC4;
//	  hdma_tx.Init.PeriphBurst         = DMA_PBURST_INC4;
//
//	  HAL_DMA_Init(&hdma_tx);
//
//	  /* Associate the initialized DMA handle to the the SPI handle */
//	  __HAL_LINKDMA(hspi, hdmatx, hdma_tx);
//
//	  /* Configure the DMA handler for Transmission process */
//	  hdma_rx.Instance                 = SPIx_RX_DMA_STREAM;
//
//	  hdma_rx.Init.Channel             = SPIx_RX_DMA_CHANNEL;
//	  hdma_rx.Init.Direction           = DMA_PERIPH_TO_MEMORY;
//	  hdma_rx.Init.PeriphInc           = DMA_PINC_DISABLE;
//	  hdma_rx.Init.MemInc              = DMA_MINC_ENABLE;
//	  hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
//	  hdma_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
//	  hdma_rx.Init.Mode                = DMA_NORMAL;
//	  hdma_rx.Init.Priority            = DMA_PRIORITY_HIGH;
//	  hdma_rx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
//	  hdma_rx.Init.FIFOThreshold       = DMA_FIFO_THRESHOLD_FULL;
//	  hdma_rx.Init.MemBurst            = DMA_MBURST_INC4;
//	  hdma_rx.Init.PeriphBurst         = DMA_PBURST_INC4;
//
//	  HAL_DMA_Init(&hdma_rx);
//
//	  /* Associate the initialized DMA handle to the the SPI handle */
//	  __HAL_LINKDMA(hspi, hdmarx, hdma_rx);
//
//	  /*##-4- Configure the NVIC for DMA #########################################*/
//	  /* NVIC configuration for DMA transfer complete interrupt (SPI3_TX) */
//	  HAL_NVIC_SetPriority(15/*SPIx_DMA_TX_IRQn*/, 0, 1);
//	  HAL_NVIC_EnableIRQ(SPIx_DMA_TX_IRQn);
//
//	  /* NVIC configuration for DMA transfer complete interrupt (SPI3_RX) */
//	  HAL_NVIC_SetPriority(SPIx_DMA_RX_IRQn, 0, 0);
//	  HAL_NVIC_EnableIRQ(SPIx_DMA_RX_IRQn);
//
//
//	  //HAL_DMA_Start();
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
  HAL_IncTick();
  if(timingDelay > 0) timingDelay--;
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
		if(!displayUpdating)
		{
			Position2 = (HAL_GPIO_ReadPin(encoderB.port, encoderB.pin) * 2) + HAL_GPIO_ReadPin(encoderBee.port, encoderBee.pin);;
			if (Position2 != Position)
			{
				isFwd = ((Position == 0) && (Position2 == 1)) || ((Position == 1) && (Position2 == 3)) ||
					((Position == 3) && (Position2 == 2)) || ((Position == 2) && (Position2 == 0));
				if (!HAL_GPIO_ReadPin(encoderP.port, encoderP.pin))
				{
					if (isFwd)
						menuEncoderTicks += 1;
					else
						menuEncoderTicks -= 1;
					menuPos = menuEncoderTicks/2;
					menuPos = menuPos % menuCount;
				}
				else
				{
					if (isFwd) Pos++;
					else Pos--;
				}
				//if (Pos < Min) Pos = Min;
				//if (Pos > Max) Pos = Max;
			}
			Position = Position2;
		}
	}

	int getPos(void)
	{
		return (Pos/2);
	}

//	uint16_t getMenuPos(void)
//	{
//		return (menuPos/2);
//	}

	void setMinMax(int _Min, int _Max)
	{
		Min = _Min*4;
		Max = _Max*4;
		if (Pos < Min) Pos = Min;
		if (Pos > Max) Pos = Max;
	}




	float samplesA[FFT_BUFFER_SIZE];
	float samplesB[FFT_BUFFER_SIZE];
	float samplesC[FFT_BUFFER_SIZE];
	float samplesDisplay[FFT_BUFFER_SIZE];
	float samplesDemod[FFT_BUFFER_SIZE];
	float samplesOverlap[200]; //filterkernellength*2
	int     sampleBankAReady = 0;
	int 	sampleBankBReady = 0;
	int		sampleBankCReady = 0;

	uint8_t waterfallBusy = 0;

	//float   outputSamplesA[512];
	//float   outputSamplesB[512];
	int     sampleBank = 0;
	//int 	sampleCounter = 0;
	//const int FFT_SIZE = 256;
	float observerA, observerB, observerC;
	int 	dcOffset1 = 1029;
	int     dcOffset2 = 1535;
	int     dcOffset3 = 1518;

	int dac1OutVal = 0;
	int dac2OutVal = 0;
	int txDacOutValMax = 0;

	void captureSamples()
	{
		if(adcConfigured)
		{
			//if(!sampleRun)
			{
				if(sampleIndex == 100)
							blink_led_on();
				adcGetConversion();
				switch (sampleBank)
				{
				case 0:
					if(transmitting == 0)
					{
						samplesA[sampleIndex*2] = ((uhADCxConvertedValue3 - dcOffset3)/4096.0); // - 2048;
						samplesA[sampleIndex*2 + 1] = ((uhADCxConvertedValue2 - dcOffset2)/4096.0); // - 2048;//0.0;
					} else {
						samplesA[sampleIndex*2] = ((uhADCxConvertedValue1 - dcOffset1)/4096.0); // - 2048;
						samplesA[sampleIndex*2 + 1] = ((uhADCxConvertedValue1 - dcOffset1)/4096.0); // - 2048;//0.0;
					}

					if(uhADCxConvertedValue1 > maxAmplitude) maxAmplitude = uhADCxConvertedValue1;
					if(uhADCxConvertedValue2 > maxAmplitude) maxAmplitude = uhADCxConvertedValue2;

					if(samplesB[sampleIndex*2] > agcLevel) agcLevel = samplesB[sampleIndex*2];
					if(samplesB[sampleIndex*2+1] > agcLevel) agcLevel = samplesB[sampleIndex*2+1];
//					if(sampleIndex < filterKernelLength)
//					{
//						dac1SetValue(samplesB[sampleIndex*2] + samplesA[(FFT_SIZE - filterKernelLength)
//								+ sampleIndex * 2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//						dac2SetValue(samplesB[sampleIndex*2+1] + samplesA[(FFT_SIZE - filterKernelLength)
//						        + sampleIndex * 2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//					} else {

					dac1OutVal = samplesB[(sampleIndex)*2] /*/ (agcLevel * agcScale)*/ * 4096 * afGain + 2048;
					dac2OutVal = samplesB[(sampleIndex)*2+1] /*/ (agcLevel * agcScale)*/ * 4096 * afGain + 2048;

					if(transmitting == 1)
					{
						if(dac1OutVal > txDacOutValMax) txDacOutValMax = dac1OutVal;
						if(dac2OutVal > txDacOutValMax) txDacOutValMax = dac2OutVal;
					}

						dac1SetValue(dac1OutVal);
						dac2SetValue(dac2OutVal);
//					}



					if(sampleIndex >= FFT_SIZE - filterKernelLength - 1)
					{
						samplesOverlap[(sampleIndex - (FFT_SIZE - filterKernelLength))*2] = samplesA[sampleIndex*2];
						samplesOverlap[(sampleIndex - (FFT_SIZE - filterKernelLength)) * 2 +1] = samplesA[sampleIndex*2+1];
					}


					break;

				case 1:
					if(transmitting == 0)
					{
						samplesB[sampleIndex*2] = ((uhADCxConvertedValue3 - dcOffset3)/4096.0); // - 2048;
						samplesB[sampleIndex*2 + 1] = ((uhADCxConvertedValue2 - dcOffset2)/4096.0); // - 2048;//0.0;
					} else {
						samplesB[sampleIndex*2] = ((uhADCxConvertedValue1 - dcOffset1)/4096.0); // - 2048;
						samplesB[sampleIndex*2 + 1] = ((uhADCxConvertedValue1 - dcOffset1)/4096.0); // - 2048;//0.0;
					}

					if(uhADCxConvertedValue1 > maxAmplitude) maxAmplitude = uhADCxConvertedValue1;
					if(uhADCxConvertedValue2 > maxAmplitude) maxAmplitude = uhADCxConvertedValue2;

					if(samplesC[sampleIndex*2] > agcLevel) agcLevel =samplesC[sampleIndex*2];
					if(samplesC[sampleIndex*2+1] > agcLevel) agcLevel = samplesC[sampleIndex*2+1];
//					if(sampleIndex < filterKernelLength)
//					{
//						dac1SetValue(samplesC[sampleIndex*2] + samplesB[(FFT_SIZE - filterKernelLength)
//								+ sampleIndex * 2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//						dac2SetValue(samplesC[sampleIndex*2+1] + samplesB[(FFT_SIZE - filterKernelLength)
//						        + sampleIndex * 2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//					} else {
						//dac1SetValue(samplesC[(sampleIndex)*2] /*/ (agcLevel * agcScale)*/ * 4096 * afGain + 2048);
						//dac2SetValue(samplesC[(sampleIndex)*2+1] /*/ (agcLevel * agcScale)*/ * 4096 * afGain + 2048);

						dac1OutVal = samplesC[(sampleIndex)*2] /*/ (agcLevel * agcScale)*/ * 4096 * afGain + 2048;
						dac2OutVal = samplesC[(sampleIndex)*2+1] /*/ (agcLevel * agcScale)*/ * 4096 * afGain + 2048;

						if(transmitting == 1)
						{
							if(dac1OutVal > txDacOutValMax) txDacOutValMax = dac1OutVal;
							if(dac2OutVal > txDacOutValMax) txDacOutValMax = dac2OutVal;
						}

							dac1SetValue(dac1OutVal);
							dac2SetValue(dac2OutVal);

//					}

						if(sampleIndex >= FFT_SIZE - filterKernelLength - 1)
						{
							samplesOverlap[(sampleIndex - (FFT_SIZE - filterKernelLength))*2] = samplesB[sampleIndex*2];
							samplesOverlap[(sampleIndex - (FFT_SIZE - filterKernelLength)) * 2 +1] = samplesB[sampleIndex*2+1];
						}

					break;

				case 2:
					if(transmitting == 0)
					{
						samplesC[sampleIndex*2] = ((uhADCxConvertedValue3 - dcOffset3)/4096.0); // - 2048;
						samplesC[sampleIndex*2 + 1] = ((uhADCxConvertedValue2 - dcOffset2)/4096.0); // - 2048;//0.0;
					} else {
						samplesC[sampleIndex*2] = ((uhADCxConvertedValue1 - dcOffset1)/4096.0); // - 2048;
						samplesC[sampleIndex*2 + 1] = ((uhADCxConvertedValue1 - dcOffset1)/4096.0); // - 2048;//0.0;
					}

					if(uhADCxConvertedValue1 > maxAmplitude) maxAmplitude = uhADCxConvertedValue1;
					if(uhADCxConvertedValue2 > maxAmplitude) maxAmplitude = uhADCxConvertedValue2;

					if(samplesA[sampleIndex*2] > agcLevel) agcLevel = samplesA[sampleIndex*2];
					if(samplesA[sampleIndex*2+1] > agcLevel) agcLevel = samplesA[sampleIndex*2+1];
//					if(sampleIndex < filterKernelLength)
//					{
//						dac1SetValue(samplesA[sampleIndex*2] + samplesC[(FFT_SIZE - filterKernelLength)
//								+ sampleIndex * 2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//						dac2SetValue(samplesA[sampleIndex*2+1] + samplesC[(FFT_SIZE - filterKernelLength)
//						        + sampleIndex * 2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//					} else {
						//dac1SetValue(samplesA[(sampleIndex)*2] /*/ (agcLevel * agcScale)*/ * 4096 * afGain + 2048);
						//dac2SetValue(samplesA[(sampleIndex)*2+1] /*/ (agcLevel * agcScale)*/ * 4096 * afGain + 2048);

						dac1OutVal = samplesA[(sampleIndex)*2] /*/ (agcLevel * agcScale)*/ * 4096 * afGain + 2048;
						dac2OutVal = samplesA[(sampleIndex)*2+1] /*/ (agcLevel * agcScale)*/ * 4096 * afGain + 2048;

						if(transmitting == 1)
						{
							if(dac1OutVal > txDacOutValMax) txDacOutValMax = dac1OutVal;
							if(dac2OutVal > txDacOutValMax) txDacOutValMax = dac2OutVal;
						}

							dac1SetValue(dac1OutVal);
							dac2SetValue(dac2OutVal);


//					}

						if(sampleIndex >= FFT_SIZE - filterKernelLength - 1)
						{
							samplesOverlap[(sampleIndex - (FFT_SIZE - filterKernelLength))*2] = samplesC[sampleIndex*2];
							samplesOverlap[(sampleIndex - (FFT_SIZE - filterKernelLength)) * 2 +1] = samplesC[sampleIndex*2+1];
						}

					break;
				}
				//dac1SetValue(outputSamplesA[sampleIndex*2]);

				agcLevel = agcLevel * (1 - 0.0001);

				sampleIndex++;
				if(sampleIndex >= FFT_SIZE) //- (filterKernelLength/2))
				{
					blink_led_off();
					sampleRun = 1;
					sampleIndex = filterKernelLength; ///2; //0;
					switch(sampleBank)
					{
					case 0:
						sampleBankAReady = 1;
						sampleBank = 1;
						zeroSampleBank(samplesB);
						break;
					case 1:
						sampleBankBReady = 1;
						sampleBank = 2;
						zeroSampleBank(samplesC);
						break;
					case 2:
						sampleBankCReady = 1;
						sampleBank = 0;
						zeroSampleBank(samplesA);
						break;
					}



					//sampleBank = sampleBank++ % 3;

//					if(sampleBank == 0)
//					{
//						sampleBankAReady = 1;
//						sampleBank = 1;
//
//					} else {
//						sampleBankBReady = 1;
//						sampleBank = 0;
//					}
				}

				adcStartConversion();
			}
		}
	}

void zeroSampleBank(float *samples)
{
	uint16_t i;
	for(i = 0; i < filterKernelLength * 2; i++)
		samples[i] = samplesOverlap[i];
	for(; i < FFT_BUFFER_SIZE; i++)
		samples[i] = 0;
}


//Seems to be working, higher numbers are higher gain. I'll need to work out the math on how much.
void setGainPot(uint8_t a, uint8_t b)
{
	uint8_t i;
	//pull NSS low
	HAL_GPIO_WritePin(GAIN_POT_NSS.port, GAIN_POT_NSS.pin, 0);

	//choose first register
	HAL_GPIO_WritePin(FILTER_GAIN_POT_MOSI.port, FILTER_GAIN_POT_MOSI.pin, 0);
	HAL_GPIO_WritePin(FILTER_GAIN_POT_SCLK.port, FILTER_GAIN_POT_SCLK.pin, 0);
	HAL_GPIO_WritePin(FILTER_GAIN_POT_SCLK.port, FILTER_GAIN_POT_SCLK.pin, 1);


	for(i = 0; i < 8; i++)
	{
		HAL_GPIO_WritePin(FILTER_GAIN_POT_MOSI.port, FILTER_GAIN_POT_MOSI.pin, (a >> (7-i)) & 1);
		HAL_GPIO_WritePin(FILTER_GAIN_POT_SCLK.port, FILTER_GAIN_POT_SCLK.pin, 0);
		HAL_GPIO_WritePin(FILTER_GAIN_POT_SCLK.port, FILTER_GAIN_POT_SCLK.pin, 1);
	}

	HAL_GPIO_WritePin(GAIN_POT_NSS.port, GAIN_POT_NSS.pin, 1);

	HAL_GPIO_WritePin(GAIN_POT_NSS.port, GAIN_POT_NSS.pin, 0);

	//choose second register
	HAL_GPIO_WritePin(FILTER_GAIN_POT_MOSI.port, FILTER_GAIN_POT_MOSI.pin, 1);
	HAL_GPIO_WritePin(FILTER_GAIN_POT_SCLK.port, FILTER_GAIN_POT_SCLK.pin, 0);
	HAL_GPIO_WritePin(FILTER_GAIN_POT_SCLK.port, FILTER_GAIN_POT_SCLK.pin, 1);


	for(i = 0; i < 8; i++)
	{
		HAL_GPIO_WritePin(FILTER_GAIN_POT_MOSI.port, FILTER_GAIN_POT_MOSI.pin, (b >> (7-i)) & 1);
		HAL_GPIO_WritePin(FILTER_GAIN_POT_SCLK.port, FILTER_GAIN_POT_SCLK.pin, 0);
		HAL_GPIO_WritePin(FILTER_GAIN_POT_SCLK.port, FILTER_GAIN_POT_SCLK.pin, 1);
	}

	HAL_GPIO_WritePin(GAIN_POT_NSS.port, GAIN_POT_NSS.pin, 1);

}

//USART1_IRQHandler(void)
//{
//    //blink_led_on();
//    USARTx_IRQHandler();
//
//}


__IO ITStatus UartReady = RESET;
uint8_t aTxBuffer[] = "Chris a baby!   ";
uint8_t aRxBuffer[256];
void configUartPeripheral()
{
//	//Enable Clocks
//	__GPIOC_CLK_ENABLE();
//	__USART6_CLK_ENABLE();
//

//	GPIO_InitTypeDef GPIO_InitStruct;
//
////	//Setup TX Pin
//	GPIO_InitStruct.Pin = RX_TO_GPS.pin;
//	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
//	GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
//	HAL_GPIO_Init(RX_TO_GPS.port, &GPIO_InitStruct);
//
//	//Setup RX Pin
//	//It doesn't get set as an input?
//	GPIO_InitStruct.Pin = TX_FROM_GPS.pin;
//	GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
//	HAL_GPIO_Init(TX_FROM_GPS.port, &GPIO_InitStruct);
//
//
////	//Configure NVIC
////	HAL_NVIC_SetPriority(USART1_IRQn, 0, 1);
////	HAL_NVIC_EnableIRQ(USART1_IRQn);
////
//	UartHandle.Instance = USART6;
//	UartHandle.Init.BaudRate = 9600;
//	UartHandle.Init.WordLength = UART_WORDLENGTH_8B;
//	UartHandle.Init.StopBits = UART_STOPBITS_1;
//	UartHandle.Init.Parity = UART_PARITY_NONE;
//	UartHandle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
//	UartHandle.Init.Mode = UART_MODE_TX_RX;
//	UartHandle.Init.OverSampling = UART_OVERSAMPLING_16;
//
//	if(HAL_UART_Init(&UartHandle) != HAL_OK)
//	  {
//	    trace_puts("UART didn't init rightly.");
//	  }
//
//	TinyGPS_init();
//
//	while(1)
//	  {
//    if(HAL_UART_Receive(&UartHandle, (uint8_t *)aRxBuffer, 256, 5000) != HAL_OK)
//      {
//        trace_puts("UART recieve didn't work. No sir.");
//      } else {
//        trace_puts(aRxBuffer);
//        for(int i = 0; i < 256; i++)
//          TinyGPS_encode(aRxBuffer[i]);
//
//      }
//	  }
}


int
main(int argc, char* argv[])
{

  /* Enable the CPU Cache */
  CPU_CACHE_Enable();



	HAL_Init();

	//SystemClock_Config();

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
#ifdef debug
	trace_puts("Hello ARM World!");
	trace_puts("Sweet, this works now!");
#endif

//	for(int i; i > 1000; i++)
//	{
//		trace_printf("%f = %f\n", (float)i, log((double)i));
//	}


	// At this stage the system clock should have already been configured
	// at high speed.
#ifdef debug
	trace_printf("System clock: %uHz\n", SystemCoreClock);
#endif

	setupPeripheralPower();

	//initDdsPins();
	hal_setupPins();
	spi_init();

//	configDMA(&SpiHandle );


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
setFreq(vfoAFrequency);
//i2cLoop();


//trace_puts(( == HAL_OK ? "SI5338 Ready" : "SI5338 Not ready"));

//HAL_I2C_MspInit(&hi2c);



	timer_start();

	blink_led_init();
	blink_led_on();

	populateCoeficients(filterUpperLimit - filterLowerLimit, mode, filterLowerLimit);

	initDac1();
	Encoder();

	Adafruit_ILI9340_begin();
	Adafruit_ILI9340_setRotation(1);
	//Adafruit_GFX_fillScreen(ILI9340_BLACK);
	Adafruit_GFX_fillScreen(ILI9340_BLACK);

	Adafruit_GFX_setTextSize(3);
	Adafruit_GFX_setTextWrap(1);
	Adafruit_GFX_setTextColor(ILI9340_WHITE, ILI9340_BLACK);
	Adafruit_ILI9340_setVerticalScrollDefinition(0,120,200);

	initAdc();
	adcConfigured = 1;
	adcStartConversion();

	//TIM_setup();
	//TIM_Config();
	TIM_Try();

	long long timeMeasurement = 0;

	updateDisplay(1);



	setGainPot(200, 200);

	//testing Uart
	configUartPeripheral();
//	while(1)
//	{
//	      //HAL_UART_Transmit_IT(&UartHandle, (uint8_t*)aTxBuffer, 16);
//
//		hal_delay_ms(100);
//	}

	//MAIN LOOP - Lowest Priority
	while(1)
	{
		//TODO: Should I shift away from 0Hz? to get away from 1/f noise? It didn't LOOK bad, but maybe it is negatively effecting things.
		//I could do something where the dial moves around on screen, but if you get too close to the edge, the DDSs start moving the frequency
		//Hmm, I think that's kind of a cool idea. It would be cool in two ways: it would allow you to shift the IF so you could get away from
		//birdies, and it would mean that while tuning around locally, the waterfall would stay aligned in a useful way. Eventually, when I have
		//sufficient display performance, I'd like to move (and scale, if applicable) the waterfall so it is properly aligned.

		//Speaking of 1/f noise. It doesn't seem to be much of an issue on this radio, I wonder why? Did I design something right?
			//Update: Not sure that the 1/f noise is as small an issue as I thought. It popped up when I was out in the field.
			//Maybe the 1/f noise is masked by all the noise in my neighborhood?
		//Also, early on, I thought it had an issue with microphonics, but it turned out that it was the connection to the computer.
		//Also since this is a form of direct conversion receiver (two of them together) I was worried about AM broadcast interference
		//but I haven't noticed any, again, maybe I did something right? Beginner's luck?

		//HAL_UART_Receive_IT(&UartHandle, (uint8_t*)aRxBuffer, 16);

		int* p = UartHandle.pRxBuffPtr;
		int* q = &aRxBuffer;
		int difference = p- q;


		updateMenu();
		updateDisplay(0);
		drawWaterfall();
		drawSMeter();

		Adafruit_GFX_fillRect(310, 0, 3, 3, HAL_GPIO_ReadPin(PADDLE_THUMB_NC.port, PADDLE_THUMB_NC.pin) ? ILI9340_RED : ILI9340_BLUE);
		Adafruit_GFX_fillRect(310, 4, 3, 3, HAL_GPIO_ReadPin(PADDLE_INDEX_NC.port, PADDLE_INDEX_NC.pin) ? ILI9340_RED : ILI9340_BLUE);
		Adafruit_GFX_fillRect(310, 8, 3, 3, HAL_GPIO_ReadPin(PADDLE_THUMB_NO.port, PADDLE_THUMB_NO.pin) ? ILI9340_RED : ILI9340_BLUE);
		Adafruit_GFX_fillRect(310, 12, 3, 3, HAL_GPIO_ReadPin(PADDLE_INDEX_NO.port, PADDLE_INDEX_NO.pin) ? ILI9340_RED : ILI9340_BLUE);

		HAL_GPIO_WritePin(RED_LED.port, RED_LED.pin, HAL_GPIO_ReadPin(GPS_PPS.port, GPS_PPS.pin));

		if(!HAL_GPIO_ReadPin(PADDLE_THUMB_NO.port, PADDLE_THUMB_NO.pin))
		//if(1) //I am locking it in transmit for some testing.
		{
			transmitting = 1;
	        HAL_GPIO_WritePin(DAC_MUX.port, DAC_MUX.pin, 1); //0 = speaker/earphone. 1=TX Drivers
//	        HAL_GPIO_WritePin(RX_MUX.port, RX_MUX.pin, 1); //Active Low
//	        HAL_GPIO_WritePin(TX_MUX.port, TX_MUX.pin, 0); //Active Low
	        HAL_GPIO_WritePin(AMP_SWITCH_A.port, AMP_SWITCH_A.pin, 1); //Route through amp.
	        HAL_GPIO_WritePin(AMP_SWITCH_B.port, AMP_SWITCH_B.pin, 0); //always reverse of above.
	        HAL_GPIO_WritePin(AMP_POWER.port, AMP_POWER.pin, 0); //0 is on.
	        tone = 200;
		} else {
			transmitting = 0;
	        HAL_GPIO_WritePin(DAC_MUX.port, DAC_MUX.pin, 0); //0 = speaker/earphone. 1=TX Drivers
//	        HAL_GPIO_WritePin(RX_MUX.port, RX_MUX.pin, 0); //Active Low
//	        HAL_GPIO_WritePin(TX_MUX.port, TX_MUX.pin, 1); //Active Low
	        HAL_GPIO_WritePin(AMP_SWITCH_A.port, AMP_SWITCH_A.pin, 0); //Bypass amp.
	        HAL_GPIO_WritePin(AMP_SWITCH_B.port, AMP_SWITCH_B.pin, 1); //always reverse of above.
	        HAL_GPIO_WritePin(AMP_POWER.port, AMP_POWER.pin, 1); //1 is off.
	        tone = 0;
		}
	}
}

float passBandRms = 0;
int lastSMeterBarWidth = 0;
void drawSMeter()
{

	//Adafruit_GFX_fillRect(150, 160, 170, 3, ILI9340_BLACK);
	int width = 10*log((passBandRms * 1000000) + 1);
	if(width > lastSMeterBarWidth)
		Adafruit_GFX_fillRect(150 + lastSMeterBarWidth, 156, width - lastSMeterBarWidth, 3, ILI9340_RED);
	else
		Adafruit_GFX_fillRect(150 + width, 156, lastSMeterBarWidth - width, 3, ILI9340_BLACK);

	lastSMeterBarWidth = width;
}

enum menuItems
{
  volumeMenuItem = 0,
  modeMenuItem,
  megahertzMenuItem,
  hundredKilohertzMenuItem,
  tenKilohertzMenuItem,
  kilohertzMenuItem,
  hundredHertzMenuItem,
  tenHertzMenuItem,
  hertzMenuItem,
  filterLowMenuItem,
  filterHighMenuItem
};

//enum menuItems
//{
//  oneMhzPlace = 0,
//  hundredKhzPlace = 1,
//  tenKhzPlace = 2,
//  oneKhzPlace = 3,
//  hundredHzPlace = 4,
//  tenHzPlace = 5,
//  oneHzPlace = 6,
//  filterLower = 7,
//  filterUpper = 8,
//  modeMenu = 9,
//  volumeMenu = 10,
//};

void updateMenu()
{
	switch(menuPos)
	{
	case megahertzMenuItem:
		frequencyDialMultiplier = 1000000;
		updateVfo();
		break;
	case hundredKilohertzMenuItem:
		frequencyDialMultiplier = 100000;
		updateVfo();
		break;
	case tenKilohertzMenuItem:
		frequencyDialMultiplier = 10000;
		updateVfo();
		break;
	case kilohertzMenuItem:
		frequencyDialMultiplier = 1000;
		updateVfo();
		break;
	case hundredHertzMenuItem:
		frequencyDialMultiplier = 100;
		updateVfo();
		break;
	case tenHertzMenuItem:
		frequencyDialMultiplier = 10;
		updateVfo();
		break;
	case hertzMenuItem:
		frequencyDialMultiplier = 1;
		updateVfo();
		break;
	case filterLowMenuItem:
		encoderPos = getPos();
		if(encoderPos != encoderLastPos)
		{
			filterLowerLimit += 1 * (encoderLastPos - encoderPos);
			if(filterLowerLimit <= 0) filterLowerLimit = 0;
			if(filterLowerLimit >= 200) filterLowerLimit = 100;
			if(filterLowerLimit >= filterUpperLimit) filterLowerLimit = filterUpperLimit - 1;
			encoderLastPos = encoderPos;
			populateCoeficients(filterUpperLimit - filterLowerLimit, mode, filterLowerLimit);
		}
		break;
	case filterHighMenuItem:
		encoderPos = getPos();
		if(encoderPos != encoderLastPos)
		{
			filterUpperLimit += 1 * (encoderLastPos - encoderPos);
			if(filterUpperLimit <= 0) filterUpperLimit = 0;
			if(filterUpperLimit >= 200) filterUpperLimit = 100;
			if(filterUpperLimit <= filterLowerLimit) filterUpperLimit = filterLowerLimit + 1;
			encoderLastPos = encoderPos;
			populateCoeficients(filterUpperLimit - filterLowerLimit, mode, filterLowerLimit);
		}
		break;
	case modeMenuItem:
		encoderPos = getPos();
		if(encoderPos != encoderLastPos)
		{
			mode = (mode + (encoderLastPos - encoderPos)) % 3;
			populateCoeficients(filterUpperLimit - filterLowerLimit, mode, filterLowerLimit);
			encoderLastPos = encoderPos;

			//TODO: CHANGE THE FILTER SO IT MAKES SENSE!
			//Right now all this does is turns the AM decoder on and off, I guess.
		}
		break;
	case volumeMenuItem:
		encoderPos = getPos();
		if(encoderPos != encoderLastPos)
		{
			afGain += afGainStep * (encoderLastPos - encoderPos);
			if(afGain > afGainMax) afGain = afGainMax;
			if(afGain <= 0) afGain = 0;
			encoderLastPos = encoderPos;
		}
		break;
	default:
		break;
	}
}


float calculateRmsOfSample(float* samples, int length)
{
	int i;
	float accumulatedSquares = 0;
	for(i = 0; i < length; i++)
	{
		accumulatedSquares += samples[i] * samples[i];
	}

	accumulatedSquares = accumulatedSquares / length;
	float32_t result;
	arm_sqrt_f32(accumulatedSquares, &result);
	return result;
}

#define freqVOffset 108     //120 - (8*3/2)
#define freqHOffset 142


//TODO: Should I make a menuItem struct? Would that be helpful? The menus are a pain right now...
uint8_t redItems[30];

enum modes
{
	LSB = 0,
	USB = 1,
	AM = 2
};



void updateDisplay(uint8_t force)
{
	displayUpdating = 1;
	static char freqChar[15];
	static char lastFreqChar[] = {'$','$','$','$','$','$','$','$','$','$','$','$','$','$','$',};

	//Draw elements that don't normally change. Static icons, etc.
	if(force)
	{
		//Adafruit_GFX_drawColorBitmap(180, 2, psdrLogo, 86,20, MASKWHITE);
		Adafruit_GFX_drawColorBitmap(150, 90, bitmapMode, 40,12, MASKWHITE);
		Adafruit_GFX_fillTriangle(126,119,136,124,136,114,ILI9340_WHITE);
		Adafruit_GFX_drawColorBitmap(150, 136, bitmapFilter, 47,12, MASKWHITE);
		drawNumber('.', freqHOffset + 16*3, freqVOffset + 0, MASKWHITE);
		drawNumber('.', freqHOffset + 16*7, freqVOffset + 0, MASKWHITE);
		Adafruit_GFX_drawColorBitmap(142, 162, bitmapSMeter, 155, 10, MASKWHITE);
		//Adafruit_GFX_drawColorBitmap(320 - 45 - 2, 240 - 46 - 2, bitmapHadLogo, 45, 46, MASKWHITE);
	}

	sprintf(&freqChar, "%9d", vfoAFrequency);

	//So on each of these elements, we update when the value changes, when we're forced to, when the item becomes selected, or unselected.
	if(freqChar[0] != lastFreqChar[0]
	     ||  force
	     || (menuPos != menuLastPos && (menuPos == megahertzMenuItem
	     || menuLastPos == megahertzMenuItem)))
	{
		drawNumber(freqChar[0], freqHOffset + 16*0, freqVOffset + 0,
		           menuPos == megahertzMenuItem ? MASKRED : MASKWHITE);
	}
	if(freqChar[1] != lastFreqChar[1]
	     || redItems[0]
	     || force
	     || (menuPos != menuLastPos && (menuPos == megahertzMenuItem
	     || menuLastPos == megahertzMenuItem)))
	{
		drawNumber(freqChar[1], freqHOffset + 16*1, freqVOffset + 0,
		           menuPos == megahertzMenuItem ? MASKRED : MASKWHITE);
	}
	if(freqChar[2] != lastFreqChar[2]
	     || force
	     || (menuPos != menuLastPos && (menuPos == megahertzMenuItem
	     || menuLastPos == megahertzMenuItem)))
	{
		drawNumber(freqChar[2], freqHOffset + 16*2, freqVOffset + 0,
		           menuPos == megahertzMenuItem ? MASKRED : MASKWHITE);
	}
	if(freqChar[3] != lastFreqChar[3]
	     || force
	     || (menuPos != menuLastPos && (menuPos == hundredKilohertzMenuItem
	     || menuLastPos == hundredKilohertzMenuItem)))
	{
		drawNumber(freqChar[3], freqHOffset + 16*4, freqVOffset + 0,
		           menuPos == hundredKilohertzMenuItem ? MASKRED : MASKWHITE);
	}
	if(freqChar[4] != lastFreqChar[4]
	     || force
	     || (menuPos != menuLastPos && (menuPos == tenKilohertzMenuItem
	     || menuLastPos == tenKilohertzMenuItem)))
	{
		drawNumber(freqChar[4], freqHOffset + 16*5, freqVOffset + 0,
		           menuPos == tenKilohertzMenuItem ? MASKRED : MASKWHITE);
	}
	if(freqChar[5] != lastFreqChar[5] || force || (menuPos != menuLastPos && (menuPos == kilohertzMenuItem || menuLastPos == kilohertzMenuItem)))
	{
		drawNumber(freqChar[5], freqHOffset + 16*6, freqVOffset + 0, menuPos == kilohertzMenuItem ? MASKRED : MASKWHITE);
	}
	if(freqChar[6] != lastFreqChar[6] || force || (menuPos != menuLastPos && (menuPos == hundredHertzMenuItem || menuLastPos == hundredHertzMenuItem)))
	{
		drawNumber(freqChar[6], freqHOffset + 16*8, freqVOffset + 0, menuPos == hundredHertzMenuItem ? MASKRED : MASKWHITE);
	}
	if(freqChar[7] != lastFreqChar[7] || force || (menuPos != menuLastPos && (menuPos == tenHertzMenuItem || menuLastPos == tenHertzMenuItem)))
	{
		drawNumber(freqChar[7], freqHOffset + 16*9, freqVOffset + 0, menuPos == tenHertzMenuItem ? MASKRED : MASKWHITE);
	}
  if(freqChar[8] != lastFreqChar[8] || force || (menuPos != menuLastPos && (menuPos == hertzMenuItem || menuLastPos == hertzMenuItem)))
  {
    drawNumber(freqChar[8], freqHOffset + 16*10, freqVOffset + 0, menuPos == hertzMenuItem ? MASKRED : MASKWHITE);
  }

	vfoALastFreq = vfoAFrequency;
	strcpy(lastFreqChar, freqChar);

	int redrawFilterBar = 0;

	if(mode != modeLast
	    || filterLowerLimit != filterLastLowerLimit
	    || force
	    || (menuPos != menuLastPos && (menuPos == filterLowMenuItem
	    || menuLastPos == filterLowMenuItem)))
	{
		sprintf(&freqChar, "%4d", filterLowerLimit * 40);
		//Adafruit_GFX_setTextSize(2);
		//Adafruit_GFX_setTextColor(menuPos == 7 ? ILI9340_RED : ILI9340_WHITE, ILI9340_BLACK);
		//Adafruit_GFX_setCursor(200, 135 );
		int i;
		for(i = 0; i < 4; i++)
		{
			//Adafruit_GFX_write(freqChar[i]);
			drawNumberSmall(freqChar[i], 205 + (i * 9), 137, menuPos == filterLowMenuItem ? MASKRED : MASKWHITE);
		}
		//Adafruit_GFX_setTextSize(3);

		redrawFilterBar = 1;
		filterLastLowerLimit = filterLowerLimit;
	}

	if(mode != modeLast
	    || filterUpperLimit != filterLastUpperLimit
	    || force
	    || (menuPos != menuLastPos && (menuPos == filterHighMenuItem
	    || menuLastPos == filterHighMenuItem)))
	{
		sprintf(&freqChar, "%-4d", filterUpperLimit * 40);
		//Adafruit_GFX_setTextSize(2);
		//Adafruit_GFX_setTextColor(menuPos == 8 ? ILI9340_RED : ILI9340_WHITE, ILI9340_BLACK);
		//Adafruit_GFX_setCursor(265, 135 );
		int i;
		for(i = 0; i < 4; i++)
		{
			//Adafruit_GFX_write(freqChar[i]);
			drawNumberSmall(freqChar[i], 250 + (i * 9), 137, menuPos == filterHighMenuItem ? MASKRED : MASKWHITE);
		}
		//Adafruit_GFX_setTextSize(3);

		redrawFilterBar = 1;
		filterLastUpperLimit = filterUpperLimit;
	}


	if(afGainLast != afGain || force || (menuPos != menuLastPos && (menuPos == volumeMenuItem || menuLastPos == volumeMenuItem)))
	{
		sprintf(&freqChar, "%-4f", afGain * 100);
		int i;
		for(i = 0; i < 4; i++)
		{
			//Adafruit_GFX_write(freqChar[i]);
			drawNumberSmall(freqChar[i], 250 + (i * 9), 10, menuPos == volumeMenuItem ? MASKRED : MASKWHITE);
		}

		afGainLast = afGain;
	}

	//I think I want to make this more like the Draw S Meter, where it only draws the parts that have changed. Also, I think I want to do gray pixels when half
	//way between values.
	if(redrawFilterBar)
	{
		switch(mode)
		{
			case LSB:
				Adafruit_GFX_fillRect(121, 0, 3, 240 , ILI9340_BLACK); //erase the old bar
				Adafruit_GFX_fillRect(121, filterLowerLimit/2 + 120, 3, (filterUpperLimit/2 - filterLowerLimit/2), ILI9340_WHITE); //draw the new one
				break;
			case USB:
				Adafruit_GFX_fillRect(121, 0, 3, 240 , ILI9340_BLACK);
				Adafruit_GFX_fillRect(121, 120 - (filterUpperLimit/2), 3, (filterUpperLimit/2 - filterLowerLimit/2), ILI9340_WHITE);
				break;
			case AM:
				Adafruit_GFX_fillRect(121, 0, 3, 240 , ILI9340_BLACK);
				Adafruit_GFX_fillRect(121, filterLowerLimit/2 + 120, 3, (filterUpperLimit/2 - filterLowerLimit/2), ILI9340_WHITE);
				Adafruit_GFX_fillRect(121, 120 - (filterUpperLimit/2), 3, (filterUpperLimit/2 - filterLowerLimit/2), ILI9340_WHITE);
				break;
		}
		redrawFilterBar = 0;
	}


	if(mode != modeLast
	    || force
	    || (menuPos != menuLastPos && (menuPos == modeMenuItem
	    || menuLastPos == modeMenuItem)))
	{
		switch(mode)
		{
		case LSB:
			Adafruit_GFX_drawColorBitmap(196, 91, bitmapLSB, 28, 9, menuPos == modeMenuItem ? MASKRED : MASKWHITE);
			break;
		case USB:
			Adafruit_GFX_drawColorBitmap(196, 91, bitmapUSB, 28, 9, menuPos == modeMenuItem ? MASKRED : MASKWHITE);
			break;
		case AM:
			Adafruit_GFX_drawColorBitmap(196, 91, bitmapAM, 28, 9, menuPos == modeMenuItem ? MASKRED : MASKWHITE);
			break;
		}

		modeLast = mode;

	}

	if(afGain * 0.99 )

	menuLastPos = menuPos;
	displayUpdating = 0;
}

int newWaterFallData = 0;
void drawWaterfall()
{
  if(newWaterFallData == 1)
    {
      static float magnitudes[FFT_SIZE];
      static float mags;
      static uint8_t waterfallScanLine = 0;

      unsigned short *gradient;

      if(transmitting)
        gradient = &bitmapIronGradient;
      else
        gradient = &bitmapWebSdrGradient;

      //arm_cmplx_mag_f32(samplesDisplay, magnitudes, FFT_SIZE);
      arm_cmplx_mag_f32(samplesDisplay, magnitudes, FFT_SIZE);

      float fftMax = 0; //AH! These are being reset each time! Static makes them persistant right? Does it also ensure they are
      float fftMin = 100; //only initialized once? Have to try it when I get home. It would certainly be nice if the waterfall
      static float fftMaxMax = 0; //didn't change in brightness so much. Later, I may want to fix these values, or at least, make them
      static float logMax; //manually controllable, sorta, you know?
      uint8_t i;
      for(i = 1; i < 255; i++) //If bin 0 is the DC offset, should we skip it in this calculation?
      {
        float mags = magnitudes[i];
        if(mags > fftMax) fftMax = mags;
        if(mags < fftMin) fftMin = mags;
      }
      //logMax = log2(fftMax);

      if(fftMax > fftMaxMax) fftMaxMax += fftMax * 0.1;
      logMax = log2(fftMaxMax);
      fftMaxMax *= 0.99;
      if (fftMaxMax > fftMaxMaxMax) fftMaxMax = fftMaxMaxMax;
      if (fftMaxMax < fftMaxMaxMin) fftMaxMax = fftMaxMaxMin;


      //			TODO: Got rid of the first bin because it's just DC offset, right?
      //			but now narrow signal can disappear when they are right at the center....
      //			Will that be better when I lower the sample frequency? Maybe I should do that next.

      Adafruit_ILI9340_setAddrWindow(waterfallScanLine, 0, waterfallScanLine, 120);
      for(i = 120; i != 0; i--)
      {
        mags = (log2(magnitudes[i] + 1)) / fftMaxMax * 100; //Log needs to be at least 1 right? We could do a + (1-fftMin) maybe? Worth it?
        //mags = magnitudes[i] / fftMaxMax * 32;
        //Adafruit_ILI9340_drawPixel(waterfallScanLine, (120 - i), gradient[(uint8_t) mags]);
        Adafruit_ILI9340_pushColor(gradient[(uint8_t) mags]);
      }

      Adafruit_ILI9340_setAddrWindow(waterfallScanLine, 120, waterfallScanLine, 239);
      for(i = 255; i > 135; i--)
      {
        mags = (log2(magnitudes[i] + 1)) / fftMaxMax * 100;
        //mags = magnitudes[i] / fftMaxMax * 32;
        //Adafruit_ILI9340_drawPixel(waterfallScanLine, 359 - (i - 15), gradient[(uint8_t) mags]);
        Adafruit_ILI9340_pushColor(gradient[(uint8_t) mags]);
      }

      waterfallScanLine++;
      if(waterfallScanLine > 119) waterfallScanLine = 0;
      Adafruit_ILI9340_setVertialScrollStartAddress((/*119 -*/ waterfallScanLine) /*+ 200*/);

      newWaterFallData = 0;
    }

}

void fillSamepleWithTone(int tone, float *samples)
{
	int i;
	for(i = 0; i < FFT_BUFFER_SIZE; i++)
	{
		samples[i] = 0;
	}

	samples[tone*2] = 0.9;
	samples[tone*2+1] = 0.9;
}


void processStream()
{
	if(sampleRun)
	{

		arm_cfft_radix4_instance_f32 fft_inst;

		//blink_led_on();

		if (sampleBankAReady == 1)
		{
			if(tone == 0)
			{
				arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 1);

				arm_cfft_radix4_f32(&fft_inst, samplesA);
				// Calculate magnitude of complex numbers output by the FFT.
				if(transmitting == 0)
				{
					if(waterfallBusy != 1)
					{
						uint16_t i;
						for(i = 0; i < FFT_BUFFER_SIZE; i++) samplesDisplay[i] = samplesA[i];
					}
				}

				applyCoeficient(samplesA, ifShift);

				if(transmitting == 1)
				{
					if(waterfallBusy != 1)
					{
						uint16_t i;
						for(i = 0; i < FFT_BUFFER_SIZE; i++) samplesDisplay[i] = samplesA[i];
					}
				}
			} else {
				fillSamepleWithTone(tone, samplesA);
			}

			arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 1, 1);
			arm_cfft_radix4_f32(&fft_inst, samplesA);



			if(mode == 2) //Try to demodulate AM
			{
				arm_cmplx_mag_f32(samplesA, samplesDemod, FFT_SIZE);

				uint16_t i;
				for(i = 0; i < FFT_SIZE; i++)
				{
					samplesA[i * 2] = samplesDemod[i];
					samplesA[i * 2 + 1] = /*1.0 -*/ samplesDemod[i]; //invert one of them so they don't cancel
					//Isn't working yet...
				}
			}

			passBandRms = calculateRmsOfSample(samplesA, FFT_BUFFER_SIZE);

			sampleBankAReady = 0;
		}
		else if(sampleBankBReady == 1)
		{
			if(tone == 0)
			{
				arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 1);


				arm_cfft_radix4_f32(&fft_inst, samplesB);
				// Calculate magnitude of complex numbers output by the FFT.
				//arm_cmplx_mag_f32(samplesB, magnitudes, FFT_SIZE);
				applyCoeficient(samplesB, ifShift);
			} else {
				fillSamepleWithTone(tone, samplesB);
			}

			arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 1, 1);
			arm_cfft_radix4_f32(&fft_inst, samplesB);

			if(mode == 2) //Try to demodulate AM
			{
				arm_cmplx_mag_f32(samplesB, samplesDemod, FFT_SIZE);

				uint16_t i;
				for(i = 0; i < FFT_SIZE; i++)
				{
					samplesB[i * 2] = samplesDemod[i];
					samplesB[i * 2 + 1] = samplesDemod[i];
				}
			}

			passBandRms = calculateRmsOfSample(samplesB, FFT_BUFFER_SIZE);

			sampleBankBReady = 0;

		}
		else if (sampleBankCReady == 1)
		{
			if(tone == 0)
			{
				arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 1);



				arm_cfft_radix4_f32(&fft_inst, samplesC);
				// Calculate magnitude of complex numbers output by the FFT.




				applyCoeficient(samplesC, ifShift);
			} else {
				fillSamepleWithTone(tone, samplesC);
			}

			arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 1, 1);
			arm_cfft_radix4_f32(&fft_inst, samplesC);

			if(mode == 2) //Try to demodulate AM
			{
				arm_cmplx_mag_f32(samplesC, samplesDemod, FFT_SIZE);

				uint16_t i;
				for(i = 0; i < FFT_SIZE; i++)
				{
					samplesC[i * 2] = samplesDemod[i];
					samplesC[i * 2 + 1] = samplesDemod[i];
				}
			}

			passBandRms = calculateRmsOfSample(samplesC, FFT_BUFFER_SIZE);

			sampleBankCReady = 0;

		}

		//blink_led_off();
		sampleRun = 0;
		newWaterFallData = 1;
	}

	//clearTimUpdateFlag(&TimHandle4);
}

void updateVfo()
{
	encoderPos = getPos();
	if(encoderPos != encoderLastPos)
	{

		vfoAFrequency += frequencyDialMultiplier * (encoderLastPos - encoderPos);

		if(vfoAFrequency < 2500000) vfoAFrequency = 2500000; //Currently, the code can only corectly drive down to 5MHz/2 = 2.5MHz
		if(vfoAFrequency > 175000000) vfoAFrequency = 175000000; //Currently, the code can only correctly drive up to 350MHz/2 = 175MHz

		encoderLastPos = encoderPos;
	}

	if(vfoAFrequency != vfoALastFreq)
	{
		setFreq(vfoAFrequency);
	}
}

void drawNumber(char c, uint16_t x, uint16_t y, uint16_t tintMask)
{
	switch(c)
	{
	case '.':
		Adafruit_GFX_drawColorBitmap(x, y, bitmapPeriod, 15, 19, tintMask);
		break;
	case '1':
		Adafruit_GFX_drawColorBitmap(x, y, bitmapOne, 15, 19, tintMask);
		break;
	case '2':
		Adafruit_GFX_drawColorBitmap(x, y, bitmapTwo, 15, 19, tintMask);
		break;
	case '3':
		Adafruit_GFX_drawColorBitmap(x, y, bitmapThree, 15, 19, tintMask);
		break;
	case '4':
		Adafruit_GFX_drawColorBitmap(x, y, bitmapFour, 15, 19, tintMask);
		break;
	case '5':
		Adafruit_GFX_drawColorBitmap(x, y, bitmapFive, 15, 19, tintMask);
		break;
	case '6':
		Adafruit_GFX_drawColorBitmap(x, y, bitmapSix, 15, 19, tintMask);
		break;
	case '7':
		Adafruit_GFX_drawColorBitmap(x, y, bitmapSeven, 15, 19, tintMask);
		break;
	case '8':
		Adafruit_GFX_drawColorBitmap(x, y, bitmapEight, 15, 19, tintMask);
		break;
	case '9':
		Adafruit_GFX_drawColorBitmap(x, y, bitmapNine, 15, 19,tintMask);
		break;
	case '0':
		Adafruit_GFX_drawColorBitmap(x, y, bitmapZero, 15, 19, tintMask);
		break;
	default:
		Adafruit_GFX_fillRect(x, y, 15, 19, ILI9340_BLACK);
	}
}

void drawNumberSmall(char c, uint16_t x, uint16_t y, uint16_t tintMask)
{
	switch(c)
	{
	case '1':
		Adafruit_GFX_drawColorBitmap(x, y, oneSmall, 8, 9, tintMask);
		break;
	case '2':
		Adafruit_GFX_drawColorBitmap(x, y, twoSmall, 8, 9, tintMask);
		break;
	case '3':
		Adafruit_GFX_drawColorBitmap(x, y, threeSmall, 8, 9, tintMask);
		break;
	case '4':
		Adafruit_GFX_drawColorBitmap(x, y, fourSmall, 8, 9, tintMask);
		break;
	case '5':
		Adafruit_GFX_drawColorBitmap(x, y, fiveSmall, 8, 9, tintMask);
		break;
	case '6':
		Adafruit_GFX_drawColorBitmap(x, y, sixSmall, 8, 9, tintMask);
		break;
	case '7':
		Adafruit_GFX_drawColorBitmap(x, y, sevenSmall, 8, 9, tintMask);
		break;
	case '8':
		Adafruit_GFX_drawColorBitmap(x, y, eightSmall, 8, 9, tintMask);
		break;
	case '9':
		Adafruit_GFX_drawColorBitmap(x, y, nineSmall, 8, 9, tintMask);
		break;
	case '0':
		Adafruit_GFX_drawColorBitmap(x, y, zeroSmall, 8, 9, tintMask);
		break;
	case '-':
		Adafruit_GFX_drawColorBitmap(x, y, hyphenSmall, 8, 9, tintMask);
		break;
	case '~':
		Adafruit_GFX_drawColorBitmap(x, y, tildeSmall, 8, 9, tintMask);
		break;
	default:
		Adafruit_GFX_fillRect(x, y, 8, 9, ILI9340_BLACK);
	}
}

//TIM_TimeBaseInitTypeDef timeBaseStructure;
//
//TIM_OC_InitTypeDef   tsConfig;
//#define  PULSE1_VALUE       40961       /* Capture Compare 1 Value  */
uint32_t uwPrescalerValue = 0;
//void TIM_setup()
//{
//	  /*##-1- Configure the TIM peripheral #######################################*/
//  /* -----------------------------------------------------------------------
//    In this example TIM3 input clock (TIM3CLK) is set to 2 * APB1 clock (PCLK1),
//    since APB1 prescaler is different from 1.
//      TIM3CLK = 2 * PCLK1
//      PCLK1 = HCLK / 4
//      => TIM3CLK = HCLK / 2 = SystemCoreClock /2
//    To get TIM3 counter clock at 60 KHz, the Prescaler is computed as following:
//    Prescaler = (TIM3CLK / TIM3 counter clock) - 1
//    Prescaler = ((SystemCoreClock /2) /60 KHz) - 1
//
//    Note:
//     SystemCoreClock variable holds HCLK frequency and is defined in system_stm32f4xx.c file.
//     Each time the core clock (HCLK) changes, user had to update SystemCoreClock
//     variable value. Otherwise, any configuration based on this variable will be incorrect.
//     This variable is updated in three ways:
//      1) by calling CMSIS function SystemCoreClockUpdate()
//      2) by calling HAL API function HAL_RCC_GetSysClockFreq()
//      3) each time HAL_RCC_ClockConfig() is called to configure the system clock frequency
//  ----------------------------------------------------------------------- */
//
//  /* Compute the prescaler value to have TIM3 counter clock equal to 60 KHz */
//  uwPrescalerValue = (uint32_t) ((SystemCoreClock/2) / 60000) - 1;
//
//  /* Set TIMx instance */
//    TimHandle.Instance = TIM3; //TIMx;
//
//  /* Initialize TIM3 peripheral as follow:
//       + Period = 65535
//       + Prescaler = (SystemCoreClock/2)/60000
//       + ClockDivision = 0
//       + Counter direction = Up
//  */
//  TimHandle.Init.Period = 65535;
//  TimHandle.Init.Prescaler = uwPrescalerValue;
//  TimHandle.Init.ClockDivision = 0;
//  TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
//  if(HAL_TIM_OC_Init(&TimHandle) != HAL_OK)
//  {
//    /* Initialization Error */
//    //Error_Handler();
//	  doNothing();
//  }
//
//  /*##-2- Configure the PWM channels #########################################*/
//  /* Common configuration */
//  tsConfig.OCMode = TIM_OCMODE_TIMING;
//  tsConfig.OCPolarity = TIM_OCPOLARITY_HIGH;
//  tsConfig.OCFastMode = TIM_OCFAST_DISABLE;
//
//  /* Set the pulse value for channel 1 */
//  tsConfig.Pulse = PULSE1_VALUE;
//  if(HAL_TIM_OC_ConfigChannel(&TimHandle, &tsConfig, TIM_CHANNEL_1) != HAL_OK)
//  {
//    /* Initialization Error */
//    //Error_Handler();
//	  doNothing();
//  }
//
//  /*##-4- Start the Output Compare mode in interrupt mode ####################*/
//  /* Start Channel1 */
//  if(HAL_TIM_OC_Start_IT(&TimHandle, TIM_CHANNEL_1) != HAL_OK)
//  {
//    /* Initialization Error */
//    //Error_Handler();
//	  doNothing();
//  }
//
//}


/////**
////  * @brief  Configures the TIM IRQ Handler.
////  * @param  None
////  * @retval None
////  */
//void TIM_Config(void)
//{
//  NVIC_InitTypeDef NVIC_InitStructure;
//
//  /* TIM3 clock enable */
//  //RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
//  __TIM3_CLK_ENABLE();
//
//  /* Enable the TIM3 gloabal Interrupt */
//  NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
//  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
//  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
//  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
//  NVIC_Init(&NVIC_InitStructure);
//
//
//}

TIM_TypeDef timTimBase;
//TIM_HandleTypeDef timHandle;
/* Definition for TIMx's NVIC */
#define TIMx_IRQn                      TIM3_IRQn //TIM3_IRQn
#define TIMx_IRQHandler                TIM3_IRQHandler
void TIM_Try(void)
{

	  uwPrescalerValue = (uint32_t) ((SystemCoreClock/2) / 21000000) - 1;

    //NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
	__TIM3_CLK_ENABLE();

	__HAL_DBGMCU_FREEZE_TIM3();
	__HAL_DBGMCU_FREEZE_TIM4();

	TimHandle.Instance = TIM3;
	TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
	TimHandle.Init.Period = 1050; //was 1050, 800 works okay, not in AM though. Can't remember what that works out to. I think 44kHz or so
	TimHandle.Init.Prescaler = uwPrescalerValue;
	TimHandle.Init.ClockDivision = 0;
	HAL_TIM_Base_Init(&TimHandle);



	HAL_TIM_Base_Start_IT(&TimHandle);

	  /*##-2- Configure the NVIC for TIMx #########################################*/
	  /* Set the TIMx priority */
	  HAL_NVIC_SetPriority(TIMx_IRQn, 0, 1);

	  /* Enable the TIMx global Interrupt */
HAL_NVIC_EnableIRQ(TIMx_IRQn);




	  __TIM4_CLK_ENABLE();
		TimHandle4.Instance = TIM4;
		TimHandle4.Init.CounterMode = TIM_COUNTERMODE_UP;
		TimHandle4.Init.Period = 1050; //was 1050, 800 works okay, not in AM though.
		TimHandle4.Init.Prescaler = uwPrescalerValue;
		TimHandle4.Init.ClockDivision = 0;

	HAL_TIM_Base_Init(&TimHandle4);
	HAL_TIM_Base_Start_IT(&TimHandle4);

	  /*##-2- Configure the NVIC for TIMx #########################################*/
	  /* Set the TIMx priority */
	  HAL_NVIC_SetPriority(30 /*TIM4_IRQn*/, 2, 4);

	  /* Enable the TIMx global Interrupt */
HAL_NVIC_EnableIRQ(TIM4_IRQn);



//	int tim3;
//	while(1)
//	{
//		 tim3 = timTryHandle.Instance->CNT;
//	}

}

TIM3_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&TimHandle);
}

TIM4_IRQHandler(void)
{
  processStream();
  HAL_TIM_IRQHandler(&TimHandle4);
}

int ledState = 0;
HAL_TIM_PeriodElapsedCallback(htim)
{
	captureSamples();
//	doNothing();
//	if(ledState)
//	{
//		blink_led_off();
//		ledState = 0;
//	}
//	else
//	{
//		blink_led_on();
//		ledState = 1;
//	}
}

//void rectToPolar(float real, float imag, float mag, float angle)
//{
//	mag = sqrtf((real * real) + (imag * imag));
//	angle = atan2f(imag,real);
//}
//
//void polarToRect(float mag, float angle, float real, float imag)
//{
//	real = mag * cosf(angle);
//	imag = mag * sinf(angle);
//}

/* Definition for DACx clock resources */
#define DACx                            DAC
#define DACx_CLK_ENABLE()               __DAC_CLK_ENABLE()
#define DACx_CHANNEL_GPIO_CLK_ENABLE()  __GPIOA_CLK_ENABLE()

#define DACx_FORCE_RESET()              __DAC_FORCE_RESET()
#define DACx_RELEASE_RESET()            __DAC_RELEASE_RESET()

/* Definition for ADCx Channel Pin */
#define DACx_CHANNEL_PIN                GPIO_PIN_4
#define DACx_CHANNEL_GPIO_PORT          GPIOA

/* Definition for ADCx's Channel */
#define DACx_CHANNEL                    DAC_CHANNEL_1

DAC_HandleTypeDef    DacHandle;
static DAC_ChannelConfTypeDef dacSConfig;

void initDac1()
{
	DACx_CLK_ENABLE();


	/*##-1- Configure the DAC peripheral #######################################*/
	DacHandle.Instance = DACx;

	if(HAL_DAC_Init(&DacHandle) != HAL_OK)
	{
		/* Initiliazation Error */
		// Error_Handler();
		doNothing();
	}

	/*##-2- Configure DAC channel1 #############################################*/
	dacSConfig.DAC_Trigger = DAC_TRIGGER_NONE;
	dacSConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;

	if(HAL_DAC_ConfigChannel(&DacHandle, &dacSConfig, DAC_CHANNEL_1) != HAL_OK)
	{
		/* Channel configuration Error */
		// Error_Handler();
		doNothing();
	}


	if(HAL_DAC_ConfigChannel(&DacHandle, &dacSConfig, DAC_CHANNEL_2) != HAL_OK)
	{
		/* Channel configuration Error */
		// Error_Handler();
		doNothing();
	}

	/*##-3- Set DAC Channel1 DHR register ######################################*/
	if(HAL_DAC_SetValue(&DacHandle, DAC_CHANNEL_1, DAC_ALIGN_12B_R/*DAC_ALIGN_8B_R*/, 0x88) != HAL_OK)
	{
		/* Setting value Error */
		// Error_Handler();
		doNothing();
	}

	/*##-3- Set DAC Channel1 DHR register ######################################*/
	if(HAL_DAC_SetValue(&DacHandle, DAC_CHANNEL_2, DAC_ALIGN_12B_R/*DAC_ALIGN_8B_R*/, 0x88) != HAL_OK)
	{
		/* Setting value Error */
		// Error_Handler();
		doNothing();
	}

	/*##-4- Enable DAC Channel1 ################################################*/
	if(HAL_DAC_Start(&DacHandle, DAC_CHANNEL_1) != HAL_OK)
	{
		/* Start Error */
		// Error_Handler();
		doNothing();
	}

	/*##-4- Enable DAC Channel1 ################################################*/
	if(HAL_DAC_Start(&DacHandle, DAC_CHANNEL_2) != HAL_OK)
	{
		/* Start Error */
		// Error_Handler();
		doNothing();
	}
}

void dac1SetValue(uint16_t value)
{
	value = value & 0b0000111111111111;
	HAL_DAC_SetValue(&DacHandle, DAC_CHANNEL_1, DAC_ALIGN_12B_R, value);
}

void dac2SetValue(uint16_t value)
{
	value = value & 0b0000111111111111;
	HAL_DAC_SetValue(&DacHandle, DAC_CHANNEL_2, DAC_ALIGN_12B_R, value);
}

/**
  * @brief  CPU L1-Cache enable.
  * @param  None
  * @retval None
  */
static void CPU_CACHE_Enable(void)
{
  /* Enable I-Cache */
  SCB_EnableICache();

  /* Enable D-Cache */
  SCB_EnableDCache();
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
