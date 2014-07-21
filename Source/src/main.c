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

void dac1SetValue(uint16_t value);
void dac2SetValue(uint16_t value);
void ddsPrefix(void);
void sendToDds(uint16_t data1, uint16_t data2);
#define FFT_SIZE 256 //supported sizes are 16, 64, 256, 1024
#define FFT_BUFFER_SIZE  512 //double the FFT_SIZE above.
__IO long long millis = 0;


float gain = 1;

float fftFilterCoeficient[FFT_BUFFER_SIZE];
float filterTemp[FFT_BUFFER_SIZE];
uint16_t filterKernelLength = 100; //what's a good value? How does it relate to the FFT size?

uint16_t menuState = 0;
uint16_t menuLastState = 1;
uint16_t menuCount = 10;
uint32_t frequencyDialMultiplier = 1;

long vfoAFrequency = 10001460;
long vfoALastFreq = 0;
int encoderPos, encoderLastPos;

int16_t filterUpperLimit = 68;
int16_t filterLowerLimit = 0;
uint8_t mode = 0;

float agcLevel = 0;
float agcScale = 160; //Higher is lower volume.. for now

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
	float x, y;
	for(i = 0; i < FFT_SIZE; i++)
	{
		polarToRect(fftFilterCoeficient[i], fftFilterCoeficient[FFT_BUFFER_SIZE - 1 - i], &x, &y);
		fftFilterCoeficient[i] = x;
		fftFilterCoeficient[FFT_BUFFER_SIZE - 1 - i] = y;
	}

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


void applyCoeficient(float *samples)
{
	//See DSP Guide Chapter 9 (Equation 9-1)
	int i;
	for(i = 0; i < FFT_SIZE; i++)
	{
		filterTemp[i * 2] = samples[i * 2] * fftFilterCoeficient[i * 2] - samples[i * 2 + 1] * fftFilterCoeficient[i * 2 + 1];
		filterTemp[i * 2 + 1] = samples[i * 2 + 1] * fftFilterCoeficient[i * 2 + 1] + samples[i * 2] * fftFilterCoeficient[i * 2];
	}

	for(i = 0; i < FFT_BUFFER_SIZE; i++)
	{
		samples[i] = filterTemp[i];
	}
}


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
		Position2 = (HAL_GPIO_ReadPin(encoderB.port, encoderB.pin) * 2) + HAL_GPIO_ReadPin(encoderA.port, encoderA.pin);;
		if (Position2 != Position)
		{
			isFwd = ((Position == 0) && (Position2 == 1)) || ((Position == 1) && (Position2 == 3)) ||
				((Position == 3) && (Position2 == 2)) || ((Position == 2) && (Position2 == 0));
			if (!HAL_GPIO_ReadPin(encoderP.port, encoderP.pin))
			{
				if (isFwd) menuState = (menuState + 1);
				else menuState = (menuState - 1);
				menuState = menuState % (menuCount * 2);
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

	int getPos(void)
	{
		return (Pos/2);
	}

	uint16_t getMenuPos(void)
	{
		return (menuState/2);
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





	float samplesA[FFT_BUFFER_SIZE];
	float samplesB[FFT_BUFFER_SIZE];
	float samplesC[FFT_BUFFER_SIZE];
	float samplesDisplay[FFT_BUFFER_SIZE];
	float samplesDemod[FFT_BUFFER_SIZE];
	float samplesOverlap[100];
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

	void captureSamples()
	{
		if(adcConfigured)
		{
			//if(!sampleRun)
			{
				adcGetConversion();
				switch (sampleBank)
				{
				case 0:

					samplesA[sampleIndex*2] = ((uhADCxConvertedValue - 2048)/4096.0); // - 2048;
					samplesA[sampleIndex*2 + 1] = ((uhADCxConvertedValue2 - 2048)/4096.0); // - 2048;//0.0;

					if(samplesB[sampleIndex*2] > agcLevel) agcLevel = samplesB[sampleIndex*2];
					if(samplesB[sampleIndex*2+1] > agcLevel) agcLevel = samplesB[sampleIndex*2+1];
//					if(sampleIndex < filterKernelLength)
//					{
//						dac1SetValue(samplesB[sampleIndex*2] + samplesA[(FFT_SIZE - filterKernelLength)
//								+ sampleIndex * 2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//						dac2SetValue(samplesB[sampleIndex*2+1] + samplesA[(FFT_SIZE - filterKernelLength)
//						        + sampleIndex * 2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//					} else {
						dac1SetValue(samplesB[sampleIndex*2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
						dac2SetValue(samplesB[sampleIndex*2+1] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//					}

					if(sampleIndex > FFT_SIZE - filterKernelLength - 1)
					{
						samplesOverlap[(sampleIndex - (FFT_SIZE - filterKernelLength))*2] = samplesA[sampleIndex*2];
						samplesOverlap[(sampleIndex - (FFT_SIZE - filterKernelLength)) * 2 +1] = samplesA[sampleIndex*2+1];
					}

					break;

				case 1:

					samplesB[sampleIndex*2] = ((uhADCxConvertedValue - 2048)/4096.0); // - 2048;
					samplesB[sampleIndex*2 + 1] = ((uhADCxConvertedValue2 - 2048)/4096.0); // - 2048;//0.0;

					if(samplesC[sampleIndex*2] > agcLevel) agcLevel =samplesC[sampleIndex*2];
					if(samplesC[sampleIndex*2+1] > agcLevel) agcLevel = samplesC[sampleIndex*2+1];
//					if(sampleIndex < filterKernelLength)
//					{
//						dac1SetValue(samplesC[sampleIndex*2] + samplesB[(FFT_SIZE - filterKernelLength)
//								+ sampleIndex * 2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//						dac2SetValue(samplesC[sampleIndex*2+1] + samplesB[(FFT_SIZE - filterKernelLength)
//						        + sampleIndex * 2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//					} else {
						dac1SetValue(samplesC[sampleIndex*2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
						dac2SetValue(samplesC[sampleIndex*2+1] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//					}

						if(sampleIndex > FFT_SIZE - filterKernelLength - 1)
						{
							samplesOverlap[(sampleIndex - (FFT_SIZE - filterKernelLength))*2] = samplesB[sampleIndex*2];
							samplesOverlap[(sampleIndex - (FFT_SIZE - filterKernelLength)) * 2 +1] = samplesB[sampleIndex*2+1];
						}

					break;

				case 2:

					samplesC[sampleIndex*2] = ((uhADCxConvertedValue - 2048)/4096.0); // - 2048;
					samplesC[sampleIndex*2 + 1] = ((uhADCxConvertedValue2 - 2048)/4096.0); // - 2048;//0.0;

					if(samplesA[sampleIndex*2] > agcLevel) agcLevel = samplesA[sampleIndex*2];
					if(samplesA[sampleIndex*2+1] > agcLevel) agcLevel = samplesA[sampleIndex*2+1];
//					if(sampleIndex < filterKernelLength)
//					{
//						dac1SetValue(samplesA[sampleIndex*2] + samplesC[(FFT_SIZE - filterKernelLength)
//								+ sampleIndex * 2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//						dac2SetValue(samplesA[sampleIndex*2+1] + samplesC[(FFT_SIZE - filterKernelLength)
//						        + sampleIndex * 2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//					} else {
						dac1SetValue(samplesA[sampleIndex*2] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
						dac2SetValue(samplesA[sampleIndex*2+1] /*/ (agcLevel * agcScale)*/ * 4096 * gain + 2048);
//					}

						if(sampleIndex > FFT_SIZE - filterKernelLength - 1)
						{
							samplesOverlap[(sampleIndex - (FFT_SIZE - filterKernelLength))*2] = samplesC[sampleIndex*2];
							samplesOverlap[(sampleIndex - (FFT_SIZE - filterKernelLength)) * 2 +1] = samplesC[sampleIndex*2+1];
						}

					break;
				}
				//dac1SetValue(outputSamplesA[sampleIndex*2]);

				agcLevel = agcLevel * (1 - 0.0001);

				sampleIndex++;
				if(sampleIndex >= FFT_SIZE - (filterKernelLength/2))
				{
					sampleRun = 1;
					sampleIndex = filterKernelLength/2; //0;
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
	for(i = 0; i < filterKernelLength; i++) samples[i] = samplesOverlap[i];
	for(; i < FFT_BUFFER_SIZE; i++) samples[i] = 0;
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


	populateCoeficients(filterUpperLimit - filterLowerLimit, 0, filterLowerLimit);


	//  float real = 3, imag = 2;
	//  float mag = 0, angle = 0;
	//  rectToPolar(real, imag, mag, angle);
	//  polarToRect(mag, angle, real, imag);


	initDac1();

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
	char modeChar[3];
	sprintf(&freqChar, "%8d", 28000000);




	char lastFreqChar[] = {'$','$','$','$','$','$','$','$','$','$','$','$','$','$',};



	initAdc();

	adcConfigured = 1;

	adcStartConversion();

	float magnitudes[FFT_SIZE];

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


	Adafruit_GFX_drawTriangle(126,119,136,124,136,114,ILI9340_WHITE);


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

		//********MENU SYSTEM*********************************
		if(getMenuPos() != menuLastState)
		{
			switch(getMenuPos())
			{
			case 0: //1,000,000 place
				frequencyDialMultiplier = 1000000;
				Adafruit_GFX_drawFastHLine(freqHOffset, freqVOffset + 25, 178, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 159, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 179, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(freqHOffset, freqVOffset + 25, 33, ILI9340_RED);
				updateVfo();
				break;
			case 1: //100,000 place
				frequencyDialMultiplier = 100000;
				Adafruit_GFX_drawFastHLine(freqHOffset, freqVOffset + 25, 178, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 159, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 179, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(freqHOffset + 18*3, freqVOffset + 25, 15, ILI9340_RED);
				updateVfo();
				break;
			case 2: //10,000 place
				frequencyDialMultiplier = 10000;
				Adafruit_GFX_drawFastHLine(freqHOffset, freqVOffset + 25, 178, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 159, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 179, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(freqHOffset + 18*4, freqVOffset + 25, 15, ILI9340_RED);
				updateVfo();
				break;
			case 3: //1,000 place
				frequencyDialMultiplier = 1000;
				Adafruit_GFX_drawFastHLine(freqHOffset, freqVOffset + 25, 178, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 159, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 179, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(freqHOffset + 18*5, freqVOffset + 25, 15, ILI9340_RED);
				updateVfo();
				break;
			case 4: //100 place
				frequencyDialMultiplier = 100;
				Adafruit_GFX_drawFastHLine(freqHOffset, freqVOffset + 25, 178, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 159, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 179, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(freqHOffset + 18*7, freqVOffset + 25, 15, ILI9340_RED);
				updateVfo();
				break;
			case 5: //10 place
				frequencyDialMultiplier = 10;
				Adafruit_GFX_drawFastHLine(freqHOffset, freqVOffset + 25, 178, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 159, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 179, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(freqHOffset + 18*8, freqVOffset + 25, 15, ILI9340_RED);
				updateVfo();
				break;
			case 6: //1 place
				frequencyDialMultiplier = 1;
				Adafruit_GFX_drawFastHLine(freqHOffset, freqVOffset + 25, 178, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 159, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 179, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(freqHOffset + 18*9, freqVOffset + 25, 15, ILI9340_RED);
				updateVfo();
				break;
			case 7: //Filter Lower
				Adafruit_GFX_drawFastHLine(freqHOffset, freqVOffset + 25, 178, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 179, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 199, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 159, 30, ILI9340_RED);
				break;
			case 8: //Filter Upper
				Adafruit_GFX_drawFastHLine(freqHOffset, freqVOffset + 25, 178, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 159, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 199, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 179, 30, ILI9340_RED);
				break;
			case 9:
				Adafruit_GFX_drawFastHLine(freqHOffset, freqVOffset + 25, 178, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 159, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 179, 30, ILI9340_BLACK);
				Adafruit_GFX_drawFastHLine(150, 199, 30, ILI9340_RED);
				break;
				//Mode
			default:
				Adafruit_GFX_drawFastHLine(freqHOffset, freqVOffset + 25, 178, ILI9340_BLACK);
				break;
			}



			menuLastState = getMenuPos();
		}

		switch(getMenuPos())
		{
		case 0: //1,000,000 place
			updateVfo();
			break;
		case 1: //100,000 place
			updateVfo();
			break;
		case 2: //10,000 place
			updateVfo();
			break;
		case 3: //1,000 place
			updateVfo();
			break;
		case 4: //100 place
			updateVfo();
			break;
		case 5: //10 place
			updateVfo();
			break;
		case 6: //1 place
			updateVfo();
			break;
		case 7: //Filter Lower
			encoderPos = getPos();
			if(encoderPos != encoderLastPos)
			{
				filterLowerLimit += 1 * (encoderLastPos - encoderPos);
				if(filterLowerLimit <= 0) filterLowerLimit = 0;
				if(filterLowerLimit >= 200) filterLowerLimit = 100;
				if(filterLowerLimit >= filterUpperLimit) filterLowerLimit = filterUpperLimit - 1;
				encoderLastPos = encoderPos;
				populateCoeficients(filterUpperLimit - filterLowerLimit, 0, filterLowerLimit);
				sprintf(&freqChar, "%5d", filterLowerLimit * 40);
				Adafruit_GFX_setTextSize(1);
				Adafruit_GFX_setCursor(150, 150 );
				int i;
				for(i = 0; i < 5; i++)
				{
					Adafruit_GFX_write(freqChar[i]);
				}
				Adafruit_GFX_setTextSize(3);

				Adafruit_GFX_fillRect(120, 120, 4, 100 , ILI9340_BLACK);
				Adafruit_GFX_fillRect(120, filterLowerLimit/2 + 120, 4, (filterUpperLimit - filterLowerLimit)/2, ILI9340_WHITE);
			}
			break;
		case 8: //Filter Upper
			encoderPos = getPos();
			if(encoderPos != encoderLastPos)
			{
				filterUpperLimit += 1 * (encoderLastPos - encoderPos);
				if(filterUpperLimit <= 0) filterUpperLimit = 0;
				if(filterUpperLimit >= 200) filterUpperLimit = 100;
				if(filterUpperLimit <= filterLowerLimit) filterUpperLimit = filterLowerLimit + 1;
				encoderLastPos = encoderPos;
				populateCoeficients(filterUpperLimit - filterLowerLimit, 0, filterLowerLimit);
				sprintf(&freqChar, "%5d", filterUpperLimit * 40);
				Adafruit_GFX_setTextSize(1);
				Adafruit_GFX_setCursor(150, 170 );
				int i;
				for(i = 0; i < 5; i++)
				{
					Adafruit_GFX_write(freqChar[i]);
				}
				Adafruit_GFX_setTextSize(3);

				Adafruit_GFX_fillRect(120, 120, 4, 100 , ILI9340_BLACK);
				Adafruit_GFX_fillRect(120, filterLowerLimit/2 + 120, 4, (filterUpperLimit - filterLowerLimit)/2, ILI9340_WHITE);

			}
			break;
		case 9: //Mode
			encoderPos = getPos();
			if(encoderPos != encoderLastPos)
			{
				mode = (encoderLastPos - encoderPos) % 3;
				Adafruit_GFX_setTextSize(1);
				Adafruit_GFX_setCursor(150, 190);
				int i;
				//TODO: CHANGE THE FILTER SO IT MAKES SENSE!
				//Right now all this does is turns the AM decoder on and off, I guess.

				switch(mode)
				{
				case 0: //LSB
					modeChar[0] = 'L';
					modeChar[1] = 'S';
					modeChar[2] = 'B';
					break;
				case 1: //USB
					modeChar[0] = 'U';
					modeChar[1] = 'S';
					modeChar[2] = 'B';
					break;
				case 2: //AM
					modeChar[0] = 'A';
					modeChar[1] = 'M';
					modeChar[2] = ' ';
				}
				for(i = 0; i < 3; i++)
				{
					Adafruit_GFX_write(modeChar[i]);
				}
				Adafruit_GFX_setTextSize(3);
			}
		default:
			break;
		}



//		float fftMaxMax = 0;
//		if(sampleRun)
//		{
//
//			timeMeasurement = millis;
//			arm_cfft_radix4_instance_f32 fft_inst;
//			//arm_cfft_radix4_init_q31(&fft_inst, FFT_SIZE, 0, 1);
//			//arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 1);
//
//			if (sampleBankAReady == 1)
//			{
//				blink_led_on();
//				arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 1);
//
//
//				//arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 2);
//
//				arm_cfft_radix4_f32(&fft_inst, samplesA);
//				// Calculate magnitude of complex numbers output by the FFT.
//				//arm_cmplx_mag_f32(samplesA, magnitudes, FFT_SIZE);
//
//				//arm_cmplx_mag_f32(samplesA, magnitudes, FFT_SIZE);
//
//				//applyCoeficient(samplesA);
//
//				arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 1, 1);
//				arm_cfft_radix4_f32(&fft_inst, samplesA);
//
//				sampleBankAReady = 0;
//				blink_led_off();
//			}
//			else if(sampleBankBReady == 1)
//			{
//				blink_led_on();
//				arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 1);
//
//				//arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 2);
//
//				arm_cfft_radix4_f32(&fft_inst, samplesB);
//				// Calculate magnitude of complex numbers output by the FFT.
//				//arm_cmplx_mag_f32(samplesB, magnitudes, FFT_SIZE);
//				//applyCoeficient(samplesB);
//
//				arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 1, 1);
//				arm_cfft_radix4_f32(&fft_inst, samplesB);
//				sampleBankBReady = 0;
//				blink_led_off();
//
//			}
//			else if (sampleBankCReady == 1)
//			{
//				blink_led_on();
//				arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 1);
//
//
//				//arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 2);
//
//				arm_cfft_radix4_f32(&fft_inst, samplesC);
//				// Calculate magnitude of complex numbers output by the FFT.
//				//arm_cmplx_mag_f32(samplesA, magnitudes, FFT_SIZE);
//
//				//arm_cmplx_mag_f32(samplesA, magnitudes, FFT_SIZE);
//				//applyCoeficient(samplesC);
//				arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 1, 1);
//				arm_cfft_radix4_f32(&fft_inst, samplesC);
//
//				sampleBankCReady = 0;
//				blink_led_off();
//			}
//			timeMeasurement = millis - timeMeasurement;
//

		arm_cmplx_mag_f32(samplesDisplay, magnitudes, FFT_SIZE);

			float fftMax = 0;
								float fftMin = 100;
								float fftMaxMax = 0;
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


//			TODOne: SWITCH THESE AND FLIP THEM. So that higher frequencies appear higher on screen.
//			TODO: Got rid of the first bin because it's just DC offset, right?
//			but now narrow signal can disappear when they are right at the center....
//			Will that be better when I lower the sample frequency? Maybe I should do that next.
			//uint16_t i;
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
//
//			sampleRun = 0;
//		}

		if(vfoAFrequency != vfoALastFreq)
		{

			setFreq(vfoAFrequency);
			sprintf(&freqChar, "%8d", vfoAFrequency);

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

			vfoALastFreq = vfoAFrequency;
			strcpy(lastFreqChar, freqChar);
		}



	}
}

void processStream()
{



	float fftMaxMax = 0;
	if(sampleRun)
	{

		//timeMeasurement = millis;
		arm_cfft_radix4_instance_f32 fft_inst;
		//arm_cfft_radix4_init_q31(&fft_inst, FFT_SIZE, 0, 1);
		//arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 1);

		if (sampleBankAReady == 1)
		{
			blink_led_on();
			arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 1);


			//arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 2);

			arm_cfft_radix4_f32(&fft_inst, samplesA);
			// Calculate magnitude of complex numbers output by the FFT.
			if(waterfallBusy != 1)
			{
				uint16_t i;
				for(i = 0; i < FFT_BUFFER_SIZE; i++) samplesDisplay[i] = samplesA[i];
				//waterfallBusy = 1;
			}
			//arm_cmplx_mag_f32(samplesA, magnitudes, FFT_SIZE);

			//arm_cmplx_mag_f32(samplesA, magnitudes, FFT_SIZE);

			applyCoeficient(samplesA);

			arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 1, 1);
			arm_cfft_radix4_f32(&fft_inst, samplesA);

			if(mode == 2) //Try to demodulate AM
			{
				arm_cmplx_mag_f32(samplesA, samplesDemod, FFT_SIZE);

				uint16_t i;
				for(i = 0; i < FFT_SIZE; i++)
				{
					samplesA[i * 2] = samplesDemod[i];
					samplesA[i * 2 + 1] = samplesDemod[i];
				}
			}


			sampleBankAReady = 0;
			blink_led_off();
		}
		else if(sampleBankBReady == 1)
		{
			blink_led_on();
			arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 1);

			//arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 2);

			arm_cfft_radix4_f32(&fft_inst, samplesB);
			// Calculate magnitude of complex numbers output by the FFT.
			//arm_cmplx_mag_f32(samplesB, magnitudes, FFT_SIZE);
			applyCoeficient(samplesB);

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

			sampleBankBReady = 0;
			blink_led_off();

		}
		else if (sampleBankCReady == 1)
		{
			blink_led_on();
			arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 1);


			//arm_cfft_radix4_init_f32(&fft_inst, FFT_SIZE, 0, 2);

			arm_cfft_radix4_f32(&fft_inst, samplesC);
			// Calculate magnitude of complex numbers output by the FFT.
			//arm_cmplx_mag_f32(samplesA, magnitudes, FFT_SIZE);

			//arm_cmplx_mag_f32(samplesA, magnitudes, FFT_SIZE);
			applyCoeficient(samplesC);
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

			sampleBankCReady = 0;
			blink_led_off();
		}
		//timeMeasurement = millis - timeMeasurement;

		float fftMax = 0;
		//					float fftMin = 100;
		//					float logMax;
		//					uint8_t i;
		//					for(i = 0; i < 255; i++)
		//					{
		//						float mags = magnitudes[i];
		//						if(mags > fftMax) fftMax = mags;
		//						if(mags < fftMin) fftMin = mags;
		//					}
		//					//logMax = log2(fftMax);
		//
		//					if(fftMax > fftMaxMax) fftMaxMax = fftMax;
		//					logMax = log2(fftMaxMax);


		//TODOne: SWITCH THESE AND FLIP THEM. So that higher frequencies appear higher on screen.
		//TODO: Got rid of the first bin because it's just DC offset, right?
		//but now narrow signal can disappear when they are right at the center....
		//Will that be better when I lower the sample frequency? Maybe I should do that next.
		//				for(i = 1; i < 120; i++)
		//				{
		//					mags = (log2(magnitudes[i] + 1)) / fftMaxMax * 32;
		//					//mags = magnitudes[i] / fftMaxMax * 32;
		//					Adafruit_ILI9340_drawPixel(waterfallScanLine, (120 - i), gradient[(uint8_t) mags]);
		//				}
		//
		//				for(i = 135; i < 255; i++)
		//				{
		//					mags = (log2(magnitudes[i] + 1)) / fftMaxMax * 32;
		//					//mags = magnitudes[i] / fftMaxMax * 32;
		//					Adafruit_ILI9340_drawPixel(waterfallScanLine, 359 - (i - 15), gradient[(uint8_t) mags]);
		//				}
		//
		//				waterfallScanLine++;
		//				if(waterfallScanLine > 119) waterfallScanLine = 0;
		//				Adafruit_ILI9340_setVertialScrollStartAddress((119 - waterfallScanLine) + 200);

		sampleRun = 0;
	}







	//wrongThings++;
    //__HAL_TIM_CLEAR_IT(htim, TIM_IT_UPDATE);
	clearTimUpdateFlag(&TimHandle4);
}

void updateVfo()
{
	encoderPos = getPos();
	if(encoderPos != encoderLastPos)
	{

		vfoAFrequency += frequencyDialMultiplier * (encoderLastPos - encoderPos);

		if(vfoAFrequency < 1) vfoAFrequency = 1;
		if(vfoAFrequency > 37500000) vfoAFrequency = 37500000;

		encoderLastPos = encoderPos;
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
#define TIMx_IRQn                      TIM3_IRQn
#define TIMx_IRQHandler                TIM3_IRQHandler
void TIM_Try(void)
{

	  uwPrescalerValue = (uint32_t) ((SystemCoreClock/2) / 21000000) - 1;

    //NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
	__TIM3_CLK_ENABLE();

	TimHandle.Instance = TIM3;
	TimHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
	TimHandle.Init.Period = 1050;
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
		TimHandle4.Init.Period = 1050;
		TimHandle4.Init.Prescaler = uwPrescalerValue;
		TimHandle4.Init.ClockDivision = 0;

	HAL_TIM_Base_Init(&TimHandle4);
	HAL_TIM_Base_Start_IT(&TimHandle4);

	  /*##-2- Configure the NVIC for TIMx #########################################*/
	  /* Set the TIMx priority */
	  HAL_NVIC_SetPriority(TIM4_IRQn, 2, 4);

	  /* Enable the TIMx global Interrupt */
	  HAL_NVIC_EnableIRQ(TIM4_IRQn);



//	int tim3;
//	while(1)
//	{
//		 tim3 = timTryHandle.Instance->CNT;
//	}

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
	if(HAL_DAC_SetValue(&DacHandle, DAC_CHANNEL_1, DAC_ALIGN_8B_R, 0x88) != HAL_OK)
	{
		/* Setting value Error */
		// Error_Handler();
		doNothing();
	}

	/*##-3- Set DAC Channel1 DHR register ######################################*/
	if(HAL_DAC_SetValue(&DacHandle, DAC_CHANNEL_2, DAC_ALIGN_8B_R, 0x88) != HAL_OK)
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

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
