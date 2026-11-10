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


float afGain = 0.0;   // default muted for bench testing (was 1.0)
float afGainLast = 2.0;
float afGainStep = 0.1;
float afGainMax = 25.0;

uint16_t maxAmplitude = 0;

float fftFilterCoeficient[FFT_BUFFER_SIZE];
float filterTemp[FFT_BUFFER_SIZE];
uint16_t filterKernelLength = 100; //what's a good value? How does it relate to the FFT size?

/* Shared with Tick() in the SysTick ISR. MUST be volatile: without it, -Os
   dead-store-eliminates the `displayUpdating = 1` guard in updateDisplay (it has
   no read between =1 and =0 in that function), which lets the encoder change
   menuPos mid-redraw - the item drawn red never gets redrawn white and stays
   red when you move off it. (The stale-red menu-highlight bug.) */
volatile uint16_t menuPos = 0;
volatile uint16_t menuEncoderTicks = 0;
volatile uint16_t menuLastPos = 1;
uint16_t menuCount = 13;   /* number of menu items (see enum menuItems) */
uint32_t frequencyDialMultiplier = 1;

long vfoAFrequency = 7030000;
long vfoALastFreq = 0;
int encoderPos, encoderLastPos;

int16_t filterUpperLimit = 68;
int16_t filterLastUpperLimit = 2;
int16_t filterLowerLimit = 0;
int16_t filterLastLowerLimit = 2;
uint8_t mode = 0;
float amDcBlock = 0.0f;   /* AM: running carrier-level estimate for envelope DC removal */
float amAudioLp = 0.0f;   /* AM: one-pole audio lowpass on the recovered envelope */
float amDcI = 0.0f;       /* AM: per-channel DC blockers (kill LO self-mix offset so the */
float amDcQ = 0.0f;       /*     carrier can sit anywhere in the passband - mistune-tolerant) */
float amHpState = 0.0f;   /* AM: audio high-pass state (filterLowerLimit edge) */
/* Audio-path self-test: a pure sine driven straight to the speaker DACs through
   the SAME anti-phase x1.5 path AM uses, bypassing all demod/AGC. Lets us prove
   whether the DAC->mux->amp->speaker chain is clean independent of the demod.
   g_testTone = tone frequency in Hz (0 = off); volume via the normal 'v'/afGain. */
volatile float g_testTone = 0.0f;   /* console 'T<hz>' (0 = off) */
volatile float g_testAmp  = 0.08f;  /* pre-afGain amplitude, ~the AM envelope level */
float g_tonePhase = 0.0f;
uint8_t modeLast = 2;

float agcLevel = 0;
float agcScale = 160; //Higher is lower volume.. for now
float txAgcLevel = 0;


int ifShift = 0;

float fftMaxMaxMax = 20;
float fftMaxMaxMin = 0.2;

int transmitting = 0;
unsigned int tone = 0;

volatile uint8_t displayUpdating = 0;   /* ISR/main handshake - see menuPos note above; must be volatile */

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
			/* AM: envelope detection needs the CARRIER (at DC) plus both
			   sidebands, so keep a LOWPASS band around DC out to the LPF edge
			   (offset+bandwidth = filterUpperLimit). The HPF edge is ignored for
			   AM - notching DC would remove the carrier and break the envelope. */
			{
				int bw = offset + bandwidth;          /* = filterUpperLimit, in bins */
				if((i < bw) || (i > FFT_BUFFER_SIZE - bw))
					fftFilterCoeficient[i] = 1;
				else
					fftFilterCoeficient[i] = 0;
			}
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

	//int shifter;
	for(i = 0; i < FFT_BUFFER_SIZE; i++)
	{
		//shifter = i + 2 * shift;
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

void teardownPeripheralPower()
{
	/* Audio amp OFF before we drop the GPIO clocks. Driven here while the GPIOE
	   clock is still on; the level then HOLDS through STOP mode (see shutDown),
	   so once U199 (TPA0253) is fed from the battery (LIPO) instead of
	   +3V3_CPU it can't drain the pack while the radio is "off". nSHDN low =
	   shutdown (<1 uA). */
	HAL_GPIO_WritePin(AUDIO_AMP_NSHTDWN.port, AUDIO_AMP_NSHTDWN.pin, 0);

	/* Blank the display + indicator LEDs for low-power sleep. Like the amp line
	   above, these levels are driven while the GPIO clocks are still on and then
	   HOLD through STOP mode. The LCD backlight is a P-FET high-side switch on
	   PC13 (LCD_LED): 0 = on, so driving it HIGH cuts the backlight and its
	   supply to the panel (J170.8). The panel's logic VCC (J170.1) is hardwired
	   to +3V3_CPU and can't be switched in firmware - small load. The indicator
	   LEDs are GPIO -> R -> LED -> GND, so LOW = off. */
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, 1);          /* LCD_LED: backlight + panel feed OFF */
	HAL_GPIO_WritePin(RED_LED.port, RED_LED.pin, 0);   /* red status LED OFF */
	{   /* LED_WHITE (PE12) and LED_3/orange (PE15, U121.45) aren't configured in
	       normal run (no CubeMX label), so force them to outputs driven low = off. */
		GPIO_InitTypeDef led = {0};
		led.Pin = GPIO_PIN_12 | GPIO_PIN_15; led.Mode = GPIO_MODE_OUTPUT_PP;
		led.Pull = GPIO_NOPULL; led.Speed = GPIO_SPEED_LOW;
		HAL_GPIO_Init(GPIOE, &led);
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12 | GPIO_PIN_15, 0);
	}

	/* Power down every RF/analog rail the firmware can reach, before STOP. These
	   levels (like the amp/backlight/LED lines above) are driven while the GPIO +
	   I2C2 clocks are still on and then HOLD through STOP mode. We cold-boot on
	   wake (shutDown -> SystemReset), which re-runs hal init + i2cSetup + setFreq
	   and brings everything back, so it's safe to be aggressive here. Polarities
	   are from hal.c. NOTE: this only reaches chips with a firmware enable line;
	   the Si5338 input/PLL, the DDS 75 MHz oscillator and the SY10EP52 ECL LO
	   dividers sit on always-on +3V3_CPU and need a hardware power switch (next
	   rev) to get below the resulting floor. FLIP_FLOP_ENABLE (PE8) is left alone:
	   its init write targets the wrong pin (hal.c) so its polarity is unverified. */
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, 1);                                      /* LPF_POWER off  (PE11 high = Q208 off -> HMC1023 LPF_5V down) */
	HAL_GPIO_WritePin(FIVE_VOLT_REGULATOR_ENABLE.port, FIVE_VOLT_REGULATOR_ENABLE.pin, 0); /* ADP1613 5V boost off (PE2 0 = off) */
	HAL_GPIO_WritePin(AMP_POWER.port, AMP_POWER.pin, 1);                           /* HMC482 RF amp off (PE3 1 = off) */
	HAL_GPIO_WritePin(PREAMP_POWER.port, PREAMP_POWER.pin, 0);                     /* mic preamp off (PD14 0 = off) */
	HAL_GPIO_WritePin(IN_AMP_ENABLE.port, IN_AMP_ENABLE.pin, 0);                   /* MCP6N11 baseband in-amps off (PC2 0 = off) */

	/* Si5338 (U241): disable all clock outputs (reg 230 OEB_ALL = 1). Stops the LO
	   feeding the ECL dividers + mixers and the CLK1A cal clock, dropping both the
	   Si5338 output-driver current and the dividers' switching current. I2C2 is
	   still clocked here; i2cSetup() has run. Fully reversed by i2cSetup() on wake. */
	write_Si5338(230, 0x10);

	__GPIOA_CLK_DISABLE();
	__GPIOB_CLK_DISABLE();
	__GPIOC_CLK_DISABLE();
	__GPIOD_CLK_DISABLE();
	__GPIOE_CLK_DISABLE();
	__DMA1_CLK_DISABLE();
	__DMA2_CLK_DISABLE();
	__I2C2_CLK_DISABLE();
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


void Encoder(void)
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

	/* ---- USB sound-card mic FIFO --------------------------------------- *
	 * captureSamples() (TIM3 ISR, 24 kHz) pushes demod audio here; the USB iso
	 * IN endpoint drains it ~once per 1 ms frame. Lock-free single-producer
	 * (TIM3 ISR) / single-consumer (OTG_FS ISR); strong overrides of the weak
	 * stubs in usbd_cdc_audio.c. */
	extern int usb_audio_streaming(void);   /* 1 once the host has opened the mic */
	#define USB_AUDIO_RING 512U    /* power of two, ~21 ms at 24 kHz */
	static volatile int16_t  usbAudioRing[USB_AUDIO_RING];
	static volatile uint16_t usbAudioHead = 0;
	static volatile uint16_t usbAudioTail = 0;
	volatile uint32_t g_ring_push = 0;   /* SWD diag: samples pushed (producer)  */
	volatile uint32_t g_ring_drop = 0;   /* SWD diag: samples dropped (ring full) */
	float usbAudioGain = 10.0f;          /* gain on the USB mic feed (console 'Z'); demod float is small */

	void usb_audio_push(int16_t s)
	{
		uint16_t h  = usbAudioHead;
		uint16_t nh = (uint16_t)((h + 1U) & (USB_AUDIO_RING - 1U));
		if(nh != usbAudioTail)           /* drop if the host has fallen behind */
		{
			usbAudioRing[h] = s;
			__DMB();                      /* publish the sample before the index */
			usbAudioHead = nh;
			g_ring_push++;
		}
		else
		{
			g_ring_drop++;
		}
	}

	uint32_t usb_audio_get_samples(int16_t *dst, uint32_t max)
	{
		uint32_t n = 0;
		uint16_t t = usbAudioTail;
		uint16_t h = usbAudioHead;
		while(n < max && t != h)
		{
			dst[n++] = usbAudioRing[t];
			t = (uint16_t)((t + 1U) & (USB_AUDIO_RING - 1U));
		}
		__DMB();
		usbAudioTail = t;
		return n;
	}

	/* ---- USB stereo I/Q FIFO (second audio device, L=I R=Q) ------------ *
	 * Same lock-free SPSC pattern, but each slot is an I,Q pair; indices count
	 * pairs. Fed with the raw pre-FFT baseband; drained by the I/Q iso endpoint. */
	extern int usb_iq_streaming(void);
	#define USB_IQ_RING 512U       /* pairs (power of two) */
	static volatile int16_t  usbIqRing[USB_IQ_RING * 2];
	static volatile uint16_t usbIqHead = 0;
	static volatile uint16_t usbIqTail = 0;
	float usbIqGain = 6.0f;        /* gain on the raw I/Q USB feed (console 'Q') */

	/* ---- adaptive (blind) I/Q imbalance correction on the USB feed -------- *
	 * Gram-Schmidt: continuously de-correlate and power-match I and Q. For a
	 * genuinely balanced signal I and Q are already uncorrelated with equal
	 * power, so c1->0, c2->1 and nothing changes; only imbalance-induced
	 * correlation/gain-mismatch is removed. Leaky-integrator stats adapt over
	 * ~0.2 s. Console 'B<0/1>' toggles it. Applied to the USB I/Q only (the
	 * device demod path is untouched). */
	volatile int iqCorrEnable = 0;   /* default OFF: native rejection ~38 dB is good; the
	                                    adaptive correction over-corrected a single tone
	                                    (converged ~11x high) - needs rework before default-on.
	                                    Toggle with console 'B1' to experiment. */
	#define IQ_MU   1.0e-4f                 /* correction adaptation rate (~0.2s TC) */
	#define IQ_DCMU 2.0e-4f                 /* DC-blocker rate (must precede the stats) */
	static float iq_dci = 0.0f, iq_dcq = 0.0f;   /* adaptive DC estimate, I and Q   */
	static float iq_pii = 1.0e-3f;          /* <I*I>  (on DC-free signal) */
	static float iq_piq = 0.0f;             /* <I*Q>  */
	static float iq_pqq = 1.0e-3f;          /* <Q*Q>  */
	float iq_c1 = 0.0f;                      /* correlation (phase) correction, clamp +-0.5 */
	float iq_c2 = 1.0f;                      /* amplitude normalization, clamp 0.7..1.4     */

	volatile uint32_t g_iq_push = 0;    /* SWD diag: I/Q pairs pushed (producer)  */
	volatile uint32_t g_iq_drop = 0;    /* SWD diag: I/Q pairs dropped (ring full) */
	volatile int32_t  g_iq_i16max = 0;  /* SWD diag: max |int16| pushed (post-scale) */
	volatile int32_t  g_iq_rawmax = 0;  /* SWD diag: max |ADC-dcOffset| at the tap   */

	void usb_iq_push(int16_t i_samp, int16_t q_samp)
	{
		int32_t ai = (i_samp < 0) ? -i_samp : i_samp;   /* SWD diag: track magnitude */
		int32_t aq = (q_samp < 0) ? -q_samp : q_samp;
		if(ai > g_iq_i16max) g_iq_i16max = ai;
		if(aq > g_iq_i16max) g_iq_i16max = aq;
		uint16_t h  = usbIqHead;
		uint16_t nh = (uint16_t)((h + 1U) & (USB_IQ_RING - 1U));
		if(nh != usbIqTail)
		{
			usbIqRing[h * 2U]      = i_samp;   /* L = I */
			usbIqRing[h * 2U + 1U] = q_samp;   /* R = Q */
			__DMB();
			usbIqHead = nh;
			g_iq_push++;
		}
		else
		{
			g_iq_drop++;
		}
	}

	uint32_t usb_iq_get_samples(int16_t *dst, uint32_t frames)
	{
		uint32_t n = 0;
		uint16_t t = usbIqTail;
		uint16_t h = usbIqHead;
		while(n < frames && t != h)
		{
			dst[n * 2U]      = usbIqRing[t * 2U];
			dst[n * 2U + 1U] = usbIqRing[t * 2U + 1U];
			t = (uint16_t)((t + 1U) & (USB_IQ_RING - 1U));
			n++;
		}
		__DMB();
		usbIqTail = t;
		return n;
	}

	void captureSamples()
	{
		if(adcConfigured)
		{
			//if(!sampleRun)
			{
				if(sampleIndex == 100)
							blink_led_on();
				adcGetConversion();

				/* AM audio, computed once per sample (used by both the speaker DAC
				   and the USB demod): envelope of the RAW I/Q with per-channel DC
				   blocking (kills the LO self-mix offset, so the carrier can sit
				   anywhere in the passband - no precise tuning needed), then the
				   HPF/LPF edges applied as AUDIO filters (bins x 93.75 Hz). */
				float amAudio = 0.0f;
				if(mode == 2 && transmitting == 0)
				{
					float qi = (uhADCxConvertedValue2 - dcOffset2) / 4096.0f;
					float qq = (uhADCxConvertedValue3 - dcOffset3) / 4096.0f;
					amDcI += 0.0008f * (qi - amDcI);
					amDcQ += 0.0008f * (qq - amDcQ);
					float ii = qi - amDcI, qa = qq - amDcQ;
					float env = sqrtf(ii * ii + qa * qa);
					amDcBlock += 0.0008f * (env - amDcBlock);   /* carrier level estimate */
					/* AGC: normalize the envelope AC by the carrier level -> the
					   modulation index (independent of signal strength, like a real
					   AM radio's AGC). A floor stops weak/absent carriers blowing up
					   the noise. 0.3 maps ~100% modulation toward full scale. */
					float carrier = amDcBlock;
					if(carrier < 0.004f) carrier = 0.004f;   /* floor: don't over-amplify a weak carrier's noise */
					float a = (env - amDcBlock) / carrier * 0.08f;   /* ~50% mod -> ~0.04 */
					float khp = filterLowerLimit * 0.012272f;   /* 2*pi*bin*93.75/48000 */
					float klp = filterUpperLimit * 0.012272f;
					if(khp > 1.0f) khp = 1.0f; else if(khp < 0.0f) khp = 0.0f;
					if(klp > 1.0f) klp = 1.0f; else if(klp < 0.02f) klp = 0.02f;
					amHpState += khp * (a - amHpState);
					a = a - amHpState;                          /* audio HPF (flo edge) */
					amAudioLp += klp * (a - amAudioLp);         /* audio LPF (fhi edge) */
					amAudio = amAudioLp;
					/* hard clamp: a weak-carrier AGC spike must never deafen / clip hard */
					if(amAudio >  0.10f) amAudio =  0.10f;
					else if(amAudio < -0.10f) amAudio = -0.10f;
				}

				/* audio self-test tone: overrides the demod, pure sine @ g_testTone Hz
				   straight into the AM output path (anti-phase DACs). Volume = afGain. */
				if(g_testTone > 0.0f && transmitting == 0)
				{
					g_tonePhase += 6.2831853f * g_testTone / 48000.0f;
					if(g_tonePhase >= 6.2831853f) g_tonePhase -= 6.2831853f;
					amAudio = g_testAmp * sinf(g_tonePhase);
				}
				int amPath = (mode == 2 || g_testTone > 0.0f);   /* use the anti-phase amAudio DAC path */

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

					/* speaker is bridge-tied (DAC1-DAC2); AM must drive the two DACs
					   anti-phase or they cancel to silence. SSB uses real/imag as-is. */
					dac1OutVal = (amPath ? amAudio *  1.5f : samplesB[(sampleIndex)*2]) * 4096 * afGain + 2048;
					dac2OutVal = (amPath ? amAudio * -1.5f : samplesB[(sampleIndex)*2+1]) * 4096 * afGain + 2048;

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

						dac1OutVal = (amPath ? amAudio *  1.5f : samplesC[(sampleIndex)*2]) * 4096 * afGain + 2048;
						dac2OutVal = (amPath ? amAudio * -1.5f : samplesC[(sampleIndex)*2+1]) * 4096 * afGain + 2048;

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

						dac1OutVal = (amPath ? amAudio *  1.5f : samplesA[(sampleIndex)*2]) * 4096 * afGain + 2048;
						dac2OutVal = (amPath ? amAudio * -1.5f : samplesA[(sampleIndex)*2+1]) * 4096 * afGain + 2048;

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

				/* feed the USB sound-card mic: demod audio, RX only. Output bank
				   is B/C/A for sampleBank 0/1/2; real part [*2] = mono demod.
				   Independent of afGain (the speaker knob) so the PC level holds
				   even with the bench speaker turned down - set it with usbAudioGain
				   (console 'Z'). The raw demod float is small, hence the big scale.
				   Only push while the host is listening. */
				if(transmitting == 0 && usb_audio_streaming())
				{
					/* real part = SSB demod; for AM take the envelope of the complex
					   output sample (time domain, after the linear filter + overlap),
					   then DC-block out the carrier level to leave the audio. */
					float afOut;
					if(mode == 2)
					{
						afOut = amAudio;                 /* AM envelope (computed once, above) */
					}
					else
					{
						switch(sampleBank)               /* SSB: real part of the FFT-filtered bank */
						{
						case 0:  afOut = samplesB[sampleIndex * 2]; break;
						case 1:  afOut = samplesC[sampleIndex * 2]; break;
						default: afOut = samplesA[sampleIndex * 2]; break;
						}
					}
					int32_t v = (int32_t)(afOut * usbAudioGain * 32767.0f);
					if(v > 32767) v = 32767; else if(v < -32768) v = -32768;
					usb_audio_push((int16_t)v);
				}

				/* feed the USB stereo I/Q device: raw baseband before the FFT.
				   L = I (ADC2), R = Q (ADC3), same DC removal as the DSP input. */
				if(transmitting == 0 && usb_iq_streaming())
				{
					int32_t ri = uhADCxConvertedValue2 - dcOffset2;   /* SWD diag */
					int32_t rq = uhADCxConvertedValue3 - dcOffset3;
					int32_t rabs = (ri < 0 ? -ri : ri);
					int32_t rqa  = (rq < 0 ? -rq : rq);
					if(rabs > g_iq_rawmax) g_iq_rawmax = rabs;
					if(rqa  > g_iq_rawmax) g_iq_rawmax = rqa;
					float fi = (uhADCxConvertedValue2 - dcOffset2) / 4096.0f;
					float fq = (uhADCxConvertedValue3 - dcOffset3) / 4096.0f;
					/* iqCorrEnable: 0=off, 1=adaptive, 2=manual (c1/c2 set over console Y/U) */
					if(iqCorrEnable)
					{
						/* DC blocker FIRST (both modes) - residual DC otherwise dominates
						   <I*Q> on weak signals and the estimate runs away; also removes the
						   LO-leakage center spike from the feed. */
						iq_dci += IQ_DCMU * (fi - iq_dci);
						iq_dcq += IQ_DCMU * (fq - iq_dcq);
						float ai = fi - iq_dci;
						float aq = fq - iq_dcq;
						if(iqCorrEnable == 1)
						{
							/* adaptive: Gram-Schmidt from slow-adapted 2nd-order stats */
							iq_pii += IQ_MU * (ai * ai - iq_pii);
							iq_piq += IQ_MU * (ai * aq - iq_piq);
							iq_pqq += IQ_MU * (aq * aq - iq_pqq);
							if(iq_pii > 1.0e-7f)
							{
								float c1 = iq_piq / iq_pii;              /* orthogonalize Q against I */
								float v  = iq_pqq - iq_piq * iq_piq / iq_pii;  /* var of corrected Q */
								if(v > 1.0e-10f)
								{
									float c2 = sqrtf(iq_pii / v);        /* match Q power to I power */
									if(c1 >  0.5f) c1 =  0.5f; else if(c1 < -0.5f) c1 = -0.5f;
									if(c2 >  1.4f) c2 =  1.4f; else if(c2 <  0.7f) c2 =  0.7f;
									iq_c1 = c1; iq_c2 = c2;
								}
							}
						}
						/* mode 2 keeps the manual iq_c1/iq_c2. Apply + output DC-free pair. */
						fi = ai;
						fq = (aq - iq_c1 * ai) * iq_c2;
					}
					int32_t li = (int32_t)(fi * usbIqGain * 32767.0f);
					int32_t lq = (int32_t)(fq * usbIqGain * 32767.0f);
					if(li > 32767) li = 32767; else if(li < -32768) li = -32768;
					if(lq > 32767) lq = 32767; else if(lq < -32768) lq = -32768;
					usb_iq_push((int16_t)li, (int16_t)lq);
				}

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

// --- HMC1023 LPF presence probe (bit-banged on the shared SPI2 lines) -------
// SCK=FILTER_GAIN_POT_SCLK(PB13), MOSI=FILTER_GAIN_POT_MOSI(PB15),
// MISO=PB14, CS=FILTER_CS(PB12). Protocol per the UHSDR driver: 32-bit frames,
// last byte = 0x5 | (regaddr<<3); reading register 0 must return 0x114080.
volatile uint32_t g_hmc1023_readback = 0xDEADBEEF;  // reg0 presence word (expect 0x114080)
volatile uint32_t g_hmc1023_reg1     = 0xDEADBEEF;  // reg1 read-back after config
volatile uint32_t g_hmc1023_reg2     = 0xDEADBEEF;  // reg2 read-back after config
volatile uint8_t  g_hmc1023_r3       = 0xAA;
volatile int      g_filter_in        = 0;           // 0 = bypass (B2, stock), 1 = route RX through HMC1023 (mux B4). Toggle over serial.

volatile int      g_amp_in           = 0;           // 1 = power + insert the HMC482 RF gain block in RX
volatile int      g_amp_pol          = 0;           // AMP_SWITCH sense (flip if "insert" is the wrong way)
volatile int      g_bench            = 0;           // 0 = stock RX path (no GPIO overrides); 1 = apply bench forcing
volatile int      g_trx              = 0;           // TRX_SWITCH: 0 = receive
volatile int      g_txrf             = 1;           // TX_RF_SWITCH_A: 1 = antenna/normal, 0 = VNA-out path
volatile int      g_imp              = 0;           // IMP_BRIDGE_SWITCH_A: 0 = bypass
volatile int      g_filt_ch          = 1;           // filter mux channel (S1<<1)|S0: 1=bypass(stock), 0=ext filter(DNF), 2/3=through HMC1023 (opposite dirs: RX vs TX)

/* USB CDC serial console (Source/usb/usb_serial.c). */
void usb_serial_init(void);
void usb_serial_poll(void);
void usb_printf(const char *fmt, ...);

static void hmc_sck(int v){ HAL_GPIO_WritePin(FILTER_GAIN_POT_SCLK.port, FILTER_GAIN_POT_SCLK.pin, v); }
static void hmc_mosi(int v){ HAL_GPIO_WritePin(FILTER_GAIN_POT_MOSI.port, FILTER_GAIN_POT_MOSI.pin, v); }
static int  hmc_miso(void){ return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) ? 1 : 0; }
static void hmc_cs(int v){ HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, v); }

static void hmc_tx_byte(uint8_t b){            // CPHA0: latch on SCK rising edge
	for(int i = 7; i >= 0; i--){ hmc_mosi((b >> i) & 1); hmc_sck(0); hmc_sck(1); }
}
static uint8_t hmc_rx_byte(void){              // sample SDO while SCK high
	uint8_t b = 0;
	for(int i = 7; i >= 0; i--){ hmc_mosi(0); hmc_sck(1); hmc_sck(0); if(hmc_miso()) b |= (1 << i); }  // CPHA1: sample after the falling edge
	return b;
}

// Write a 24-bit value to register regaddr (last byte tags chip id 0x5 + regaddr).
static void hmc_write_reg(uint32_t regaddr, uint32_t value)
{
	hmc_cs(0);
	hmc_tx_byte((value >> 16) & 0xFF);
	hmc_tx_byte((value >> 8) & 0xFF);
	hmc_tx_byte(value & 0xFF);
	hmc_tx_byte(0x5 | (regaddr << 3));
	hmc_cs(1);
}

// Read a register: point the read mux at it (write reg0 = regaddr), then clock
// out 4 bytes (one dummy clock first to align the frame). Stashes chip id in r3.
static uint32_t hmc_read_reg(uint32_t regaddr)
{
	hmc_write_reg(0, regaddr);
	hmc_cs(0);
	hmc_mosi(0); hmc_sck(1); hmc_sck(0);
	uint8_t r0 = hmc_rx_byte(), r1 = hmc_rx_byte(), r2 = hmc_rx_byte(), r3 = hmc_rx_byte();
	hmc_cs(1);
	g_hmc1023_r3 = r3;
	return ((uint32_t)r0 << 16) | ((uint32_t)r1 << 8) | r2;
}

void hmc1023_probe(void)
{
	GPIO_InitTypeDef g;
	g.Pin = GPIO_PIN_12; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(GPIOB, &g);                  // CS output
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 1);  // idle high
	g.Pin = GPIO_PIN_14; g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOB, &g);                  // MISO input
	hmc_sck(0);

	// 1) presence check (expect 0x114080)
	g_hmc1023_readback = hmc_read_reg(0);

	// 2) reg1: enable BOTH I/Q channels + apply SPI bandwidth. filter_I_enable (b2)
	//    and filter_Q_enable (b3) default high and gate the whole analog path (gain
	//    stage + filter + driver). The old value 0x12 cleared them -- it held both
	//    channels in reset, which is why the chip passed no signal. force_cal_code
	//    (b4) tells the chip to use the SPI bandwidth settings.  => 0x1C
	hmc_write_reg(1, (1u << 2) | (1u << 3) | (1u << 4));

	// 3) reg2: coarse=4 (20 MHz), bypass OFF (bit5=0), GAIN ON (bit4=1), drvr=1, opamp=1  => 0x115
	hmc_write_reg(2, (4u << 6) | (0u << 5) | (1u << 4) | (1u << 2) | (1u << 0));

	// 4) read config back to prove the writes landed
	g_hmc1023_reg1 = hmc_read_reg(1);
	g_hmc1023_reg2 = hmc_read_reg(2);

	// 5) route the RX signal THROUGH the filter: mux channel B4 (S0=1, S1=1).
	//    Stock firmware parks on B2 = straight-through bypass (S0=1, S1=0).
	HAL_GPIO_WritePin(FILTER_S0.port, FILTER_S0.pin, 1);
	HAL_GPIO_WritePin(FILTER_S1.port, FILTER_S1.pin, 1);
}

/* Public wrappers so the serial console can read/write HMC1023 registers live. */
void hmc1023_write(uint32_t reg, uint32_t val) { hmc_write_reg(reg, val); }
uint32_t hmc1023_read(uint32_t reg) { return hmc_read_reg(reg); }

/* Configure only the bit-bang CS (PB12) and MISO (PB14) lines so the serial
   console can talk to the HMC1023 live. SCK/MOSI are already set up in hal.c.
   This is NOT the full probe: no register writes, no mux switch, so boot stays
   on the clean stock RX path. */
void hmc1023_io_init(void)
{
	GPIO_InitTypeDef g;
	g.Pin = GPIO_PIN_12; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(GPIOB, &g);                  // CS output
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, 1);  // idle high
	g.Pin = GPIO_PIN_14; g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOB, &g);                  // MISO input
	hmc_sck(0);
}

/* Console control of LPF_POWER (PE11): 1 = powered (drive low -> Q208 -> LPF_5V),
   0 = off. Lets us re-power the HMC1023 AFTER the Si5338 cal clock (CLK1A) is up,
   so the filter's power-on auto-calibration runs with a valid CALCK present. */
void lpf_power(int on){ HAL_GPIO_WritePin(GPIOE, GPIO_PIN_11, on ? 0 : 1); }

/* One-shot clean RX configuration for the HMC1023: power it, enable both I/Q
   channels, +10 dB gain, 20 MHz Butterworth corner, filter active, and read the
   config back. This is the config proven on the bench to pass the RX signal WITH
   gain (it beats the hardware bypass path). The whole point: reg1 bits 2/3 are the
   channel enables -- leaving them clear (as the old 0x12 did) keeps the analog path
   dead. Uncalibrated bandwidth (+/-20%) is fine here: the corner sits far above the
   QSD baseband, so the filter only anti-aliases and drives the ADC. Switch the chip
   into the path separately (bench mux ch2, i.e. 'g1' then 'M2'). */
void hmc1023_config_rx(void)
{
	lpf_power(1);                                                   // ensure LPF_5V is up
	HAL_Delay(2);
	hmc_write_reg(1, (1u << 2) | (1u << 3) | (1u << 4));            // 0x1C: I_en | Q_en | force_cal
	hmc_write_reg(2, (4u << 6) | (1u << 4) | (1u << 2) | (1u << 0)); // 0x115: 20MHz | gain10dB | drvr01 | opamp01
	g_hmc1023_reg1 = hmc_read_reg(1);
	g_hmc1023_reg2 = hmc_read_reg(2);
}

int g_hmcGainOn = 0;     /* on-device HMC1023 RX-preamp state (menu toggle) */
int hmcGainLast = -1;    /* last drawn state, so the display redraws on change */

/* Toggle the whole RX-through-HMC1023 path as one on-device control.
   ON  = configure the chip (I/Q channels + ~20 dB gain), route RX through it
         (mux ch2), pin the RF switches to plain antenna RX, and enable the
         bench overrides so the routing holds. OFF = back to the stock bypass
         RX path and power the LPF down. Matches the console H + M2 + g1 combo
         that measured a clean ~66 dB on the bench. */
void hmcGainSet(int on)
{
	if(on)
	{
		hmc1023_config_rx();                 /* reg1=0x1C, reg2=0x115, LPF powered */
		g_filt_ch = 2;                       /* filter mux -> RX direction through the HMC1023 */
		g_amp_in  = 0;                       /* keep the HMC482 TX amp out of RX */
		g_trx = 0; g_txrf = 1; g_imp = 0;    /* plain antenna receive (not TX / not VNA) */
		g_bench = 1;                         /* apply the RX-path GPIO overrides each loop */
	}
	else
	{
		g_bench   = 0;                       /* stock bypass RX path (signalPath owns the mux) */
		g_filt_ch = 1;                       /* bypass */
		lpf_power(0);                        /* power the HMC1023 LPF down */
	}
	g_hmcGainOn = on ? 1 : 0;
}

/* --- Display readback (bit-bang) ------------------------------------------
   The LCD writes go over HW SPI1 (SCK/MOSI as AF, fast); MISO/PB4 is a plain
   GPIO input. To READ the ILI9340 we bit-bang: make SCK/MOSI GPIO outputs,
   clock the transaction by hand (slow enough for the panel's read timing,
   SCK idle-high / sample-on-rising to match CPOL=1,CPHA=1), read MISO, then
   restore SCK/MOSI to SPI1 AF. lcd_read_regs reads `cmd` then `n` bytes
   (first byte is usually a dummy). Proof: lcd_read_id() should contain the
   0x93 0x40 ILI9340 signature. */
static void lcd_bb_begin(void)
{
	GPIO_InitTypeDef g = {0};
	g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FAST;
	g.Pin = LCD_SCK.pin;  HAL_GPIO_Init(LCD_SCK.port, &g);
	g.Pin = LCD_MOSI.pin; HAL_GPIO_Init(LCD_MOSI.port, &g);
	HAL_GPIO_WritePin(LCD_SCK.port, LCD_SCK.pin, 1);   /* idle high (CPOL=1) */
}
static void lcd_bb_end(void)
{
	GPIO_InitTypeDef g = {0};
	g.Mode = GPIO_MODE_AF_PP; g.Alternate = GPIO_AF5_SPI1; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FAST;
	g.Pin = LCD_SCK.pin;  HAL_GPIO_Init(LCD_SCK.port, &g);
	g.Pin = LCD_MOSI.pin; HAL_GPIO_Init(LCD_MOSI.port, &g);
}
static uint8_t lcd_bb_rdbyte(void)
{
	uint8_t r = 0;
	for(int i = 0; i < 8; i++){
		HAL_GPIO_WritePin(LCD_SCK.port, LCD_SCK.pin, 0);
		HAL_GPIO_WritePin(LCD_SCK.port, LCD_SCK.pin, 1);   /* rising edge -> sample */
		r = (uint8_t)((r << 1) | (HAL_GPIO_ReadPin(LCD_MISO.port, LCD_MISO.pin) ? 1 : 0));
	}
	return r;
}
static void lcd_bb_wrbyte(uint8_t c)
{
	for(int i = 7; i >= 0; i--){
		HAL_GPIO_WritePin(LCD_SCK.port, LCD_SCK.pin, 0);                 /* falling -> set data */
		HAL_GPIO_WritePin(LCD_MOSI.port, LCD_MOSI.pin, (c >> i) & 1);
		HAL_GPIO_WritePin(LCD_SCK.port, LCD_SCK.pin, 1);                 /* rising -> slave samples */
	}
}
/* read `cmd` then n (<=8) bytes into out; returns via out[] */
void lcd_read_regs(uint8_t cmd, uint8_t *out, int n)
{
	lcd_bb_begin();
	HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 0);    /* CS low */
	HAL_GPIO_WritePin(LCD_DC.port,  LCD_DC.pin, 0);     /* command */
	lcd_bb_wrbyte(cmd);
	HAL_GPIO_WritePin(LCD_DC.port,  LCD_DC.pin, 1);     /* data */
	/* one dummy clock: ILI9340 read commands shift the first real bit out
	   only after a dummy cycle following the command byte */
	HAL_GPIO_WritePin(LCD_SCK.port, LCD_SCK.pin, 0);
	HAL_GPIO_WritePin(LCD_SCK.port, LCD_SCK.pin, 1);
	for(int i = 0; i < n; i++) out[i] = lcd_bb_rdbyte();
	HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 1);    /* CS high */
	lcd_bb_end();
}

static void lcd_bb_cmd(uint8_t c){ HAL_GPIO_WritePin(LCD_DC.port, LCD_DC.pin, 0); lcd_bb_wrbyte(c); HAL_GPIO_WritePin(LCD_DC.port, LCD_DC.pin, 1); }
static void lcd_bb_dat(uint8_t d){ HAL_GPIO_WritePin(LCD_DC.port, LCD_DC.pin, 1); lcd_bb_wrbyte(d); }
extern uint8_t usb_cdc_tx(uint8_t *buf, uint16_t len);

/* Stream the whole 320x240 screen as RGB565 (big-endian, 2 bytes/pixel) over
   the CDC console. Bit-bangs CASET/PASET + RAMRD with CS held low for the
   whole frame; sends it a row at a time in 64-byte CDC packets. The caller
   prints an "IMG 320 240 565" header first so the host can sync. Runs in the
   main loop (usb_serial_poll), so updateDisplay is paused meanwhile. */
/* Read rows [y0, y1) of the 320-wide screen. The CDC bulk IN stalls after
   ~148 KB in one go, so the host pulls the frame in bands (each < that) and
   stitches them; each call is a fresh, short transfer. */
void lcd_screenshot(int y0, int y1)
{
	static uint8_t row[320 * 2];
	if(y0 < 0) y0 = 0; if(y1 > 240) y1 = 240; if(y1 <= y0) return;
	lcd_bb_begin();
	HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 0);          /* CS low for the whole read */
	lcd_bb_cmd(0x2A); lcd_bb_dat(0); lcd_bb_dat(0); lcd_bb_dat((319)>>8); lcd_bb_dat((319)&0xFF); /* CASET 0..319 */
	lcd_bb_cmd(0x2B); lcd_bb_dat((uint8_t)(y0>>8)); lcd_bb_dat((uint8_t)(y0&0xFF)); lcd_bb_dat((uint8_t)((y1-1)>>8)); lcd_bb_dat((uint8_t)((y1-1)&0xFF)); /* PASET y0..y1-1 */
	lcd_bb_cmd(0x2E);                                        /* RAMRD */
	(void)lcd_bb_rdbyte();                                   /* 1 dummy byte */
	for(int y = y0; y < y1; y++)
	{
		for(int x = 0; x < 320; x++)
		{
			uint8_t r6 = (uint8_t)(lcd_bb_rdbyte() >> 2);   /* 6-bit R in the high bits */
			uint8_t g6 = (uint8_t)(lcd_bb_rdbyte() >> 2);
			uint8_t b6 = (uint8_t)(lcd_bb_rdbyte() >> 2);
			uint16_t px = (uint16_t)(((r6 >> 1) << 11) | (g6 << 5) | (b6 >> 1));
			row[x*2] = (uint8_t)(px >> 8); row[x*2+1] = (uint8_t)(px & 0xFF);
		}
		int off = 0;
		while(off < 640)
		{
			int chunk = (640 - off > 64) ? 64 : (640 - off);
			/* wait (bounded, so a dead host can't hang us) for this chunk to be
			   accepted before advancing, so the frame isn't truncated. */
			long tries = 0;
			while(usb_cdc_tx(row + off, (uint16_t)chunk) != 0 && ++tries < 20000000L) { }
			off += chunk;
		}
	}
	/* The band length is a multiple of 64, so the host holds the last buffered
	   packets until it sees a SHORT packet. Send a 2-byte flush so the whole
	   band is delivered. (The host reads exactly nrows*640 and drops this.) */
	{ uint8_t fin[2] = {0, 0}; long tr = 0; while(usb_cdc_tx(fin, 2) != 0 && ++tr < 20000000L) { } }
	HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 1);          /* CS high */
	lcd_bb_end();
}

/* --- RTC clock (LSE 32.768kHz crystal Y183 on PC14/PC15) -------------------
   Keeps time on battery across STOP/off (backup domain). The Control Panel
   sets it over the console until GPS can; the time shows along the bottom. */
RTC_HandleTypeDef hrtc;
#define RTC_MAGIC 0x50534401u    /* "PSD\1" - marks the RTC as already set */

void rtc_init(void)
{
	__HAL_RCC_PWR_CLK_ENABLE();
	HAL_PWR_EnableBkUpAccess();

	RCC_OscInitTypeDef osc = {0};
	osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
	osc.LSEState = RCC_LSE_ON;
	osc.PLL.PLLState = RCC_PLL_NONE;        /* leave the system PLL alone */
	HAL_RCC_OscConfig(&osc);

	RCC_PeriphCLKInitTypeDef pclk = {0};
	pclk.PeriphClockSelection = RCC_PERIPHCLK_RTC;
	pclk.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
	HAL_RCCEx_PeriphCLKConfig(&pclk);
	__HAL_RCC_RTC_ENABLE();

	hrtc.Instance = RTC;
	hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
	hrtc.Init.AsynchPrediv = 127;
	hrtc.Init.SynchPrediv = 255;            /* 32768 / 128 / 256 = 1 Hz */
	hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
	hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
	hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
	HAL_RTC_Init(&hrtc);                     /* sets prescalers; preserves a running calendar */

	if(RTC->BKP0R != RTC_MAGIC)              /* first ever boot / VBAT lost -> default, mark set */
	{
		RTC_TimeTypeDef tm = {0}; RTC_DateTypeDef dt = {0};
		tm.Hours = 12; tm.Minutes = 0; tm.Seconds = 0;
		dt.Year = 25; dt.Month = RTC_MONTH_JANUARY; dt.Date = 1; dt.WeekDay = RTC_WEEKDAY_WEDNESDAY;
		HAL_RTC_SetTime(&hrtc, &tm, RTC_FORMAT_BIN);
		HAL_RTC_SetDate(&hrtc, &dt, RTC_FORMAT_BIN);
		RTC->BKP0R = RTC_MAGIC;
	}
}

/* Set the clock from the console: y=full year, mo=1..12, d=1..31, h=0..23. */
void rtc_set(int y, int mo, int d, int h, int mi, int s)
{
	RTC_TimeTypeDef tm = {0}; RTC_DateTypeDef dt = {0};
	tm.Hours = (uint8_t)h; tm.Minutes = (uint8_t)mi; tm.Seconds = (uint8_t)s;
	dt.Year = (uint8_t)(y % 100); dt.Month = (uint8_t)mo; dt.Date = (uint8_t)d; dt.WeekDay = RTC_WEEKDAY_MONDAY;
	HAL_RTC_SetTime(&hrtc, &tm, RTC_FORMAT_BIN);
	HAL_RTC_SetDate(&hrtc, &dt, RTC_FORMAT_BIN);
	RTC->BKP0R = RTC_MAGIC;
}

/* Fill buf with "H:MM:SS A - M/D/YYYY". Must read time then date (unlocks the
   shadow regs). Returns the 20xx year via *yrOut for callers that want it. */
void rtc_format(char *buf, int buflen)
{
	RTC_TimeTypeDef tm; RTC_DateTypeDef dt;
	HAL_RTC_GetTime(&hrtc, &tm, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &dt, RTC_FORMAT_BIN);
	int h = tm.Hours; char ap = (h < 12) ? 'A' : 'P';
	int h12 = h % 12; if(h12 == 0) h12 = 12;
	snprintf(buf, buflen, "%d:%02d:%02d %c - %d/%d/20%02d",
	         h12, tm.Minutes, tm.Seconds, ap, dt.Month, dt.Date, dt.Year);
}

/* --- Battery (LIPO_V on ADC1 injected channel; see adc.c) ------------------ */
void     battery_read_raw2(uint16_t *batt, uint16_t *vref);   /* adc.c */
void     battery_adc_init(void);          /* adc.c */
/* volts per ADC count = VDDA/4095 * 2 (R174/R175 = 100K/100K -> /2 divider,
   from the schematic). VDDA is measured per-unit each sample from the MCU's
   internal VREFINT (so it self-calibrates on any board - no baked constant);
   this nominal is only used before the first sample. */
float    g_battScale = 3.3f * 2.0f / 4095.0f;
uint16_t g_vrefRaw = 0;                       /* VREFINT raw, for VDDA measurement */
#define  VREFINT_CAL_ADDR  ((uint16_t*)0x1FF0F44A)   /* factory VREFINT @ 3.3V (F74x/75x) */
uint16_t g_battRaw = 0;
float    g_battVolts = 0.0f;

/* 1S LiPo resting discharge curve -> percent (piecewise-linear interpolation). */
int battery_percent(float v)
{
	static const float vt[] = {3.30f,3.50f,3.68f,3.73f,3.78f,3.82f,3.86f,3.92f,3.98f,4.08f,4.20f};
	static const int   pt[] = {   0,    5,   10,   20,   30,   40,   50,   60,   70,   80,  100};
	const int n = 11;
	if(v <= vt[0]) return 0;
	if(v >= vt[n-1]) return 100;
	for(int i = 1; i < n; i++)
		if(v < vt[i])
			return pt[i-1] + (int)((v - vt[i-1]) / (vt[i] - vt[i-1]) * (pt[i] - pt[i-1]));
	return 100;
}

/* Sample the battery and update the globals. */
void battery_poll(void)
{
	uint16_t vref = 0;
	battery_read_raw2(&g_battRaw, &vref);
	g_vrefRaw = vref;
	uint16_t cal = *VREFINT_CAL_ADDR;                 /* factory VREFINT reading @ 3.3 V */
	if(vref > 100 && cal > 100 && cal < 4096)
		g_battScale = (3.3f * (float)cal / (float)vref) * 2.0f / 4095.0f;   /* real VDDA / unit */
	g_battVolts = g_battRaw * g_battScale;
}

/* MCP73871 (U161) charge status. Pins from the netlist: PG=PD0, STAT2=PD1,
   STAT1/LBO=PD4 (all open-drain, active-low). PG low = input power present. */
void charge_init(void)
{
	__HAL_RCC_GPIOD_CLK_ENABLE();
	GPIO_InitTypeDef g = {0};
	g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_PULLUP; g.Speed = GPIO_SPEED_LOW;
	g.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4;
	HAL_GPIO_Init(GPIOD, &g);
}

const char *charge_label(void)
{
	int pg    = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_0);   /* 0 = input power present (plugged in) */
	int stat2 = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_1);   /* 0 = charge complete */
	int stat1 = HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_4);   /* 0 = charging */
	if(pg)                 return "";                  /* no input -> running on battery */
	if(!stat1 &&  stat2)   return " Charge";           /* charging */
	if(!stat1 && !stat2)   return " Fault";            /* fault (both low) */
	return " Plug";                                    /* plugged, charge complete / standby */
}

/* --- Sleep/wake state retention -------------------------------------------
   "Off" enters STOP then cold-boots on wake, which re-inits the globals, so we
   stash the user state in the RTC backup registers (backup domain) before
   sleeping and restore it at boot. These survive the STOP cold-boot AND power
   cycles (VBAT keeps them), so state also persists across reboots - that's the
   free bonus Michael was fine with. */
#define STATE_MAGIC 0x50534431u   /* "PSD1" */
void state_save(void)
{
	RTC->BKP1R = (uint32_t)vfoAFrequency;
	RTC->BKP2R = ((uint32_t)(mode & 0x3))
	           | (((uint32_t)(filterLowerLimit & 0xFF)) << 8)
	           | (((uint32_t)(filterUpperLimit & 0xFF)) << 16)
	           | (((uint32_t)(g_hmcGainOn & 1)) << 24);
	RTC->BKP4R = (uint32_t)(afGain * 10.0f + 0.5f);   /* volume, 0..250 */
	RTC->BKP3R = STATE_MAGIC;
}
void state_restore(void)
{
	if(RTC->BKP3R != STATE_MAGIC) return;              /* nothing saved yet -> keep defaults */
	long f = (long)RTC->BKP1R;
	uint32_t p = RTC->BKP2R;
	if(f >= 2500000L && f <= 175000000L) { vfoAFrequency = f; setFreq((unsigned long)f); }
	uint8_t m = (uint8_t)(p & 0x3); if(m <= 2) mode = m;
	int flo = (int)((p >> 8) & 0xFF), fhi = (int)((p >> 16) & 0xFF);
	if(fhi > flo && fhi <= 128) { filterLowerLimit = (int16_t)flo; filterUpperLimit = (int16_t)fhi; }
	populateCoeficients(filterUpperLimit - filterLowerLimit, mode, filterLowerLimit);
	uint32_t v = RTC->BKP4R; if(v <= 250) afGain = (float)v / 10.0f;   /* volume */
	if((p >> 24) & 1) hmcGainSet(1);                   /* re-route through the HMC1023 if it was on */
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

enum signalPaths
{
  recieve_no_filter = 0,
  recieve_with_filter = 1,
  transmit_no_amp_no_filter = 2,
  transmit_with_amp_no_filter = 3,
  tramsmit_with_amp_with_filter = 4,
  vna_reflected = 5,
  vna_through = 6
};

//Set up the signal path for various scenarios.
void signalPath(int signalPath)
{
  //Remember the relays operate in pairs (there is a pin that needs to be off and one that needs to be on.)
  switch(signalPath)
  {
    case recieve_no_filter:
      //Amp power off
      HAL_GPIO_WritePin (AMP_POWER.port, AMP_POWER.pin, 1); //1 is off.

      //Filter power off

      //So 5V power can be off
      HAL_GPIO_WritePin (FIVE_VOLT_REGULATOR_ENABLE.port, FIVE_VOLT_REGULATOR_ENABLE.pin, 0);

      //Amp relay set to bypass
      HAL_GPIO_WritePin (AMP_SWITCH_A.port, AMP_SWITCH_A.pin, 0); // 0 to bypass (I think!)
      HAL_GPIO_WritePin (AMP_SWITCH_B.port, AMP_SWITCH_B.pin, 1); //always reverse of above.

      //Impedance bridge relay set to bypass
      HAL_GPIO_WritePin (IMP_BRIDGE_SWITCH_A.port, IMP_BRIDGE_SWITCH_A.pin, 0); //0 to bypass
      HAL_GPIO_WritePin (IMP_BRIDGE_SWITCH_B.port, IMP_BRIDGE_SWITCH_B.pin, 1); //always reverse of above.

      //VNA port relay (called TX_RF_Switch) set to bypass
      HAL_GPIO_WritePin (TX_RF_SWITCH_A.port, TX_RF_SWITCH_A.pin, 1); //0 to route to VNA Out SMA connector
      HAL_GPIO_WritePin (TX_RF_SWITCH_B.port, TX_RF_SWITCH_B.pin, 0); //always reverse of above.

      //DAC Mux set to Speaker
      HAL_GPIO_WritePin (DAC_MUX.port, DAC_MUX.pin, 0); //0 = speaker/earphone.

      //Both TX/RX Muxes set to RX
      HAL_GPIO_WritePin (TRX_SWITCH.port, TRX_SWITCH.pin, 0);

      //Filter Muxes set to bypass
      HAL_GPIO_WritePin (FILTER_S0.port, FILTER_S0.pin, 1);
      HAL_GPIO_WritePin (FILTER_S1.port, FILTER_S1.pin, 0);

      break;
    case recieve_with_filter:

          break;
    case transmit_no_amp_no_filter:
      //Amp power off
      HAL_GPIO_WritePin (AMP_POWER.port, AMP_POWER.pin, 1); //1 is off.

      //Filter power off

      //So 5V power can be off
      HAL_GPIO_WritePin (FIVE_VOLT_REGULATOR_ENABLE.port, FIVE_VOLT_REGULATOR_ENABLE.pin, 0);

      //Amp relay set to bypass
      HAL_GPIO_WritePin (AMP_SWITCH_A.port, AMP_SWITCH_A.pin, 0); // 0 to bypass (I think!)
      HAL_GPIO_WritePin (AMP_SWITCH_B.port, AMP_SWITCH_B.pin, 1); //always reverse of above.

      //Impedance bridge relay set to bypass
      HAL_GPIO_WritePin (IMP_BRIDGE_SWITCH_A.port, IMP_BRIDGE_SWITCH_A.pin, 0); //0 to bypass
      HAL_GPIO_WritePin (IMP_BRIDGE_SWITCH_B.port, IMP_BRIDGE_SWITCH_B.pin, 1); //always reverse of above.

      //VNA port relay (called TX_RF_Switch) set to bypass
      HAL_GPIO_WritePin (TX_RF_SWITCH_A.port, TX_RF_SWITCH_A.pin, 1); //0 to route to VNA Out SMA connector
      HAL_GPIO_WritePin (TX_RF_SWITCH_B.port, TX_RF_SWITCH_B.pin, 0); //always reverse of above.

      //DAC Mux set to RF
      HAL_GPIO_WritePin (DAC_MUX.port, DAC_MUX.pin, 1); //0 = speaker/earphone.

      //Both TX/RX Muxes set to TX
      HAL_GPIO_WritePin (TRX_SWITCH.port, TRX_SWITCH.pin, 1);

      //Filter Muxes set to bypass
      HAL_GPIO_WritePin (FILTER_S0.port, FILTER_S0.pin, 1);
      HAL_GPIO_WritePin (FILTER_S1.port, FILTER_S1.pin, 0);


      break;
    case transmit_with_amp_no_filter:
      //Amp power on
      HAL_GPIO_WritePin (AMP_POWER.port, AMP_POWER.pin, 0); //1 is off.

      //Filter power off

      //So 5V power needs to be on
      HAL_GPIO_WritePin (FIVE_VOLT_REGULATOR_ENABLE.port, FIVE_VOLT_REGULATOR_ENABLE.pin, 1); //1 is on.

      //Amp relay set to on
      HAL_GPIO_WritePin (AMP_SWITCH_A.port, AMP_SWITCH_A.pin, 0); // 0 to bypass (I think!)
      HAL_GPIO_WritePin (AMP_SWITCH_B.port, AMP_SWITCH_B.pin, 1); //always reverse of above.

      //Impedance bridge relay set to bypass
      HAL_GPIO_WritePin (IMP_BRIDGE_SWITCH_A.port, IMP_BRIDGE_SWITCH_A.pin, 0); //0 to bypass
      HAL_GPIO_WritePin (IMP_BRIDGE_SWITCH_B.port, IMP_BRIDGE_SWITCH_B.pin, 1); //always reverse of above.

      //VNA port relay (called TX_RF_Switch) set to bypass
      HAL_GPIO_WritePin (TX_RF_SWITCH_A.port, TX_RF_SWITCH_A.pin, 1); //0 to route to VNA Out SMA connector
      HAL_GPIO_WritePin (TX_RF_SWITCH_B.port, TX_RF_SWITCH_B.pin, 0); //always reverse of above.

      //DAC Mux set to RF
      HAL_GPIO_WritePin (DAC_MUX.port, DAC_MUX.pin, 1); //0 = speaker/earphone.

      //Both TX/RX Muxes set to TX
      HAL_GPIO_WritePin (TRX_SWITCH.port, TRX_SWITCH.pin, 1);

      //Filter Muxes set to bypass
      HAL_GPIO_WritePin (FILTER_S0.port, FILTER_S0.pin, 1);
      HAL_GPIO_WritePin (FILTER_S1.port, FILTER_S1.pin, 0);

      break;
    case tramsmit_with_amp_with_filter:

      break;
    case vna_reflected:

      break;
    case vna_through:

      break;
//    default:
//      #ifdef debug
//        trace_puts("Bad signalPath Value");
//      #endif
  }
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

	initAdcDMA();            /* DMA-driven oversampling capture; TIM6 paces, DMA ISR drives captureSamples */
	adcConfigured = 1;
	adcStartConversion();
	battery_adc_init();      /* ADC1 injected channel on LIPO_V (PC0) for battery voltage */
	charge_init();           /* MCP73871 charge-status GPIOs (PD0/PD1/PD4) */

	//TIM_setup();
	//TIM_Config();
	TIM_Try();

	//long long timeMeasurement = 0;

	// Bench bring-up: init the HMC1023 bit-bang IO (CS/MISO) so the serial console
	// can talk to it live, and so a restored +20 dB state can re-route the RX path.
	// NOT the full probe -- no reg writes, no mux switch unless state_restore asks.
	hmc1023_io_init();

	// Restore freq/mode/filter/volume/HMC saved before the last sleep BEFORE the
	// first draw, so the display never flashes the power-on defaults on wake.
	// rtc_init() runs first: it enables backup-domain access where the state lives.
	rtc_init();          // LSE + RTC clock (keeps time on battery; set from the Control Panel)
	state_restore();

	updateDisplay(1);    // first draw now shows the restored state, not defaults

	setGainPot(200, 200);

	usb_serial_init();   // bring up the USB CDC serial console

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
		if (g_bench) {   // bench overrides OFF by default -> use the stock receive path
		// Live filter A/B (bench): re-assert the RX mux every loop so signalPath()
		// can't force bypass. Poke g_filter_in over SWD: 1=HMC1023 in (B4), 0=bypass.
		// Shared select lines to BOTH muxes (U211+U298). ch = (S1<<1)|S0:
		// 1=bypass (stock), 0=external filter (DNF), 2 & 3 = through the HMC1023
		// in opposite directions (one is RX, one is TX).
		HAL_GPIO_WritePin(FILTER_S0.port, FILTER_S0.pin, g_filt_ch & 1);
		HAL_GPIO_WritePin(FILTER_S1.port, FILTER_S1.pin, (g_filt_ch >> 1) & 1);
		// 5V boost: enable ONLY when the LPF or amp needs it. Stock RX turns it off
		// on purpose -- the ADP1613 switcher desensitizes the HF front end -- so
		// forcing it on was deafening the receiver. Off in plain bypass RX.
		HAL_GPIO_WritePin(FIVE_VOLT_REGULATOR_ENABLE.port, FIVE_VOLT_REGULATOR_ENABLE.pin,
		                  (g_filter_in || g_amp_in) ? 1 : 0);
		// RF gain block (HMC482). amp OFF -> stock BYPASS (A=0,B=1, exactly as
		// signalPath sets it) so the signal passes straight through. amp ON ->
		// insert it; g_amp_pol flips the unverified insert sense.
		HAL_GPIO_WritePin(AMP_POWER.port, AMP_POWER.pin, g_amp_in ? 0 : 1);  // 0 = supply on
		if (g_amp_in) {
			HAL_GPIO_WritePin(AMP_SWITCH_A.port, AMP_SWITCH_A.pin, g_amp_pol ? 0 : 1);
			HAL_GPIO_WritePin(AMP_SWITCH_B.port, AMP_SWITCH_B.pin, g_amp_pol ? 1 : 0);
		} else {
			HAL_GPIO_WritePin(AMP_SWITCH_A.port, AMP_SWITCH_A.pin, 0);  // stock bypass
			HAL_GPIO_WritePin(AMP_SWITCH_B.port, AMP_SWITCH_B.pin, 1);
		}
		// Force the rest of the RX path deterministically (don't depend on signalPath
		// having run): T/R switch, the TX/antenna RF switch, and the imp-bridge switch.
		HAL_GPIO_WritePin(TRX_SWITCH.port, TRX_SWITCH.pin, g_trx ? 1 : 0);
		HAL_GPIO_WritePin(TX_RF_SWITCH_A.port, TX_RF_SWITCH_A.pin, g_txrf ? 1 : 0);
		HAL_GPIO_WritePin(TX_RF_SWITCH_B.port, TX_RF_SWITCH_B.pin, g_txrf ? 0 : 1);
		HAL_GPIO_WritePin(IMP_BRIDGE_SWITCH_A.port, IMP_BRIDGE_SWITCH_A.pin, g_imp ? 1 : 0);
		HAL_GPIO_WritePin(IMP_BRIDGE_SWITCH_B.port, IMP_BRIDGE_SWITCH_B.pin, g_imp ? 0 : 1);
		}   // end bench overrides (g_bench)
		usb_serial_poll();   // process any serial-console commands

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

		//int* p = UartHandle.pRxBuffPtr;
		//int* q = &aRxBuffer;
		//int difference = p- q;


		updateMenu();
		updateDisplay(0);
		drawWaterfall();
		drawSMeter();

		Adafruit_GFX_fillRect(310, 0, 3, 3, HAL_GPIO_ReadPin(PADDLE_THUMB_NC.port, PADDLE_THUMB_NC.pin) ? ILI9340_RED : ILI9340_BLUE);
		Adafruit_GFX_fillRect(310, 4, 3, 3, HAL_GPIO_ReadPin(PADDLE_INDEX_NC.port, PADDLE_INDEX_NC.pin) ? ILI9340_RED : ILI9340_BLUE);
		Adafruit_GFX_fillRect(310, 8, 3, 3, HAL_GPIO_ReadPin(PADDLE_THUMB_NO.port, PADDLE_THUMB_NO.pin) ? ILI9340_RED : ILI9340_BLUE);
		Adafruit_GFX_fillRect(310, 12, 3, 3, HAL_GPIO_ReadPin(PADDLE_INDEX_NO.port, PADDLE_INDEX_NO.pin) ? ILI9340_RED : ILI9340_BLUE);

		HAL_GPIO_WritePin(RED_LED.port, RED_LED.pin, HAL_GPIO_ReadPin(GPS_PPS.port, GPS_PPS.pin));

		if(!HAL_GPIO_ReadPin(PADDLE_THUMB_NO.port, PADDLE_THUMB_NO.pin) && !HAL_GPIO_ReadPin(PADDLE_INDEX_NO.port, PADDLE_INDEX_NO.pin))
		//if(1) //I am locking it in transmit for some testing.
		{
			transmitting = 1;
			signalPath(transmit_no_amp_no_filter);
			//signalPath(transmit_with_amp_no_filter);
//	        HAL_GPIO_WritePin(DAC_MUX.port, DAC_MUX.pin, 1); //0 = speaker/earphone. 1=TX Drivers
////	        HAL_GPIO_WritePin(RX_MUX.port, RX_MUX.pin, 1); //Active Low
////	        HAL_GPIO_WritePin(TX_MUX.port, TX_MUX.pin, 0); //Active Low
//	        HAL_GPIO_WritePin(AMP_SWITCH_A.port, AMP_SWITCH_A.pin, 1); //Route through amp.
//	        HAL_GPIO_WritePin(AMP_SWITCH_B.port, AMP_SWITCH_B.pin, 0); //always reverse of above.
//	        HAL_GPIO_WritePin(AMP_POWER.port, AMP_POWER.pin, 0); //0 is on.
//	        //tone = 200;
	        tone = 0;
		} else {
			transmitting = 0;
			signalPath(recieve_no_filter);
//	        HAL_GPIO_WritePin(DAC_MUX.port, DAC_MUX.pin, 0); //0 = speaker/earphone. 1=TX Drivers
////	        HAL_GPIO_WritePin(RX_MUX.port, RX_MUX.pin, 0); //Active Low
////	        HAL_GPIO_WritePin(TX_MUX.port, TX_MUX.pin, 1); //Active Low
//	        HAL_GPIO_WritePin(AMP_SWITCH_A.port, AMP_SWITCH_A.pin, 0); //Bypass amp.
//	        HAL_GPIO_WritePin(AMP_SWITCH_B.port, AMP_SWITCH_B.pin, 1); //always reverse of above.
//	        HAL_GPIO_WritePin(AMP_POWER.port, AMP_POWER.pin, 1); //1 is off.
//	        tone = 0;
		}
	}
}

float passBandRms = 0;
int lastSMeterBarWidth = 0;
void drawSMeter(void)
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
  filterHighMenuItem,
  hmcGainMenuItem,
  offMenuItem
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
	case hmcGainMenuItem:
		encoderPos = getPos();
		if(encoderPos != encoderLastPos)
		{
			/* turn one way = HMC RX gain ON (routed through the HMC1023),
			   the other way = OFF (stock bypass). */
			hmcGainSet((encoderLastPos - encoderPos) > 0 ? 1 : 0);
			encoderLastPos = encoderPos;
		}
		break;
	case offMenuItem:
		encoderPos = getPos();
		if(encoderPos != encoderLastPos)
		{
			teardownPeripheralPower();
			shutDown();
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
	/* Snapshot menuPos ONCE for the whole redraw. The encoder ISR (Tick) can
	   move menuPos, and this redraw reads it many times (every item's red/white
	   guard) and latches it to menuLastPos at the end. If menuPos changes
	   partway through, different items compare against different values and the
	   latch can skip the transition -> items drawn red never get redrawn white
	   = the stale-red bug (all items stuck red). Using a single frame-local
	   value makes the whole frame self-consistent regardless of the ISR. The
	   macro just guarantees every menuPos reference below uses the snapshot. */
	uint16_t mp = menuPos;
	#define menuPos mp
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

	sprintf(freqChar, "%9ld", vfoAFrequency);

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
		sprintf(freqChar, "%4d", filterLowerLimit * 40);
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
		sprintf(freqChar, "%-4d", filterUpperLimit * 40);
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
		sprintf(freqChar, "%4d", (int)(afGain * 10));
		int i;
		for(i = 0; i < 4; i++)
		{
			//Adafruit_GFX_write(freqChar[i]);
			drawNumberSmall(freqChar[i], 250 + (i * 9), 10, menuPos == volumeMenuItem ? MASKRED : MASKWHITE);
		}

		afGainLast = afGain;
	}

	/* Static "Vol" label above the volume number (number is at x=250, y=10). */
	if(force)
	{
		Adafruit_GFX_setTextSize(1);
		Adafruit_GFX_setTextColor(ILI9340_WHITE, ILI9340_BLACK);
		Adafruit_GFX_setCursor(259, 1);
		for(const char *p = "Vol"; *p; p++) Adafruit_GFX_write(*p);
	}

	if(force || (menuPos != menuLastPos && (menuPos == offMenuItem || menuLastPos == offMenuItem)))
	{
		Adafruit_GFX_drawColorBitmap(220, 10, bitmapOff, 20, 9, menuPos == offMenuItem ? MASKRED : MASKWHITE);
	}

	/* HMC1023 RX-preamp on/off indicator (text, self-erasing via black bg).
	   Redraws on selection change or when the state toggles. Placement is a
	   first guess between the Mode and Filter labels - adjust once we can read
	   the display back. */
	if(force || g_hmcGainOn != hmcGainLast
	    || (menuPos != menuLastPos && (menuPos == hmcGainMenuItem || menuLastPos == hmcGainMenuItem)))
	{
		Adafruit_GFX_setTextSize(1);
		Adafruit_GFX_setTextColor(menuPos == hmcGainMenuItem ? ILI9340_RED : ILI9340_WHITE, ILI9340_BLACK);
		Adafruit_GFX_setCursor(220, 30);   /* under the "Off" label (x=220, y=10) */
		const char *hs = g_hmcGainOn ? "+20dB ON " : "+20dB OFF";
		for(const char *p = hs; *p; p++) Adafruit_GFX_write(*p);
		hmcGainLast = g_hmcGainOn;
	}

	/* Battery line, centered in the right panel (right of the waterfall, x>=121):
	   "Batt: X.XXv (~Y%)". Samples ADC1's injected LIPO_V channel, smoothed.
	   Integer formatting (newlib-nano printf has no %f). */
	{
		/* Throttle to ~1 Hz: the two injected ADC reads are slow, and the text
		   redraw (esp. a width change) stutters the waterfall. Debounce the
		   charge state so a toggling charger doesn't thrash that redraw. */
		static float sm = 0.0f;
		static int shownPct = -1;
		static char chg[10] = "", chgCand[10] = ""; static int chgCnt = 0;
		static int bpc = 0;
		if(force || ++bpc >= 24)
		{
			bpc = 0;
			battery_poll();
			sm = (sm <= 0.01f) ? g_battVolts : (sm * 0.85f + g_battVolts * 0.15f);  /* ~6 s low-pass */
			int pct = battery_percent(sm);
			if(shownPct < 0 || pct >= shownPct + 2 || pct <= shownPct - 2) shownPct = pct;
			const char *cl = charge_label();
			if(strcmp(cl, chgCand) != 0) { strcpy(chgCand, cl); chgCnt = 0; }  /* candidate changed */
			else if(++chgCnt >= 3) strcpy(chg, cl);                           /* adopt only when stable */
		}
		int mv = (int)(sm * 100.0f + 0.5f);
		static char lastB[48] = ""; static int lastBW = -1;
		char bb[48]; snprintf(bb, sizeof(bb), "Batt: %d.%02dv (~%d%%)%s", mv/100, mv%100, shownPct, chg);
		int w = (int)strlen(bb) * 6, cx = 121 + (199 - w)/2, by = 216;
		Adafruit_GFX_setTextSize(1);
		Adafruit_GFX_setTextColor(ILI9340_WHITE, ILI9340_BLACK);
		if(force || w != lastBW)   /* width/center changed -> clear the row and redraw centered */
		{
			Adafruit_GFX_fillRect(121, by, 199, 8, ILI9340_BLACK);
			Adafruit_GFX_setCursor(cx, by);
			for(char *p = bb; *p; p++) Adafruit_GFX_write((uint8_t)*p);
		}
		else   /* same width -> per-character update only (no blink) */
			for(int i = 0; bb[i]; i++)
				if(bb[i] != lastB[i]) { Adafruit_GFX_setCursor(cx + i*6, by); Adafruit_GFX_write((uint8_t)bb[i]); }
		strcpy(lastB, bb); lastBW = w;
	}

	/* Clock along the bottom: "H:MM:SS A - M/D/YYYY". Reads the RTC each frame,
	   redraws only when the text changes (once a second); black bg self-erases. */
	{
		static char lastClk[40] = ""; static int lastCW = -1;
		char tb[40]; rtc_format(tb, sizeof(tb));
		int w = (int)strlen(tb) * 6, cx = 121 + (199 - w)/2, by = 229;   /* centered in the right panel */
		Adafruit_GFX_setTextSize(1);
		Adafruit_GFX_setTextColor(ILI9340_WHITE, ILI9340_BLACK);
		if(force || w != lastCW)   /* width/center changed (hour or date digits) -> clear + redraw centered */
		{
			Adafruit_GFX_fillRect(121, by, 199, 8, ILI9340_BLACK);
			Adafruit_GFX_setCursor(cx, by);
			for(char *p = tb; *p; p++) Adafruit_GFX_write((uint8_t)*p);
		}
		else   /* same width -> redraw only the characters that changed (no blink) */
			for(int i = 0; tb[i]; i++)
				if(tb[i] != lastClk[i]) { Adafruit_GFX_setCursor(cx + i*6, by); Adafruit_GFX_write((uint8_t)tb[i]); }
		strcpy(lastClk, tb); lastCW = w;
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
				/* AM: one bar CENTERED on the dial = the channel (carrier +/- the
				   audio bandwidth, filterUpperLimit). The HPF/LPF are audio filters
				   on the envelope now, so there's no two-sided FFT passband. */
				Adafruit_GFX_fillRect(121, 0, 3, 240 , ILI9340_BLACK);
				Adafruit_GFX_fillRect(121, 120 - (filterUpperLimit/2), 3, filterUpperLimit, ILI9340_WHITE);
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

	/* Latch the highlight position EVERY frame. This used to be gated by a stray
	   `if(afGain * 0.99)` (no braces), so menuLastPos only updated when the
	   volume was non-zero; with it stale, the "redraw the item you left back to
	   white" guard never fired and items got stuck red. That was the real
	   stale-red bug. */
	menuLastPos = menuPos;
	#undef menuPos
	displayUpdating = 0;
}

int newWaterFallData = 0;
void drawWaterfall(void)
{
  if(newWaterFallData == 1)
    {
      static float magnitudes[FFT_SIZE];
      static float mags;
      static uint8_t waterfallScanLine = 0;

      unsigned short *gradient;

      if(transmitting)
        gradient = bitmapIronGradient;
      else
        gradient = bitmapWebSdrGradient;

      //arm_cmplx_mag_f32(samplesDisplay, magnitudes, FFT_SIZE);
      arm_cmplx_mag_f32(samplesDisplay, magnitudes, FFT_SIZE);

      float fftMax = 0; //AH! These are being reset each time! Static makes them persistant right? Does it also ensure they are
      float fftMin = 100; //only initialized once? Have to try it when I get home. It would certainly be nice if the waterfall
      static float fftMaxMax = 0; //didn't change in brightness so much. Later, I may want to fix these values, or at least, make them
      //static float logMax; //manually controllable, sorta, you know?
      uint8_t i;
      for(i = 1; i < 255; i++) //If bin 0 is the DC offset, should we skip it in this calculation?
      {
        float mags = magnitudes[i];
        if(mags > fftMax) fftMax = mags;
        if(mags < fftMin) fftMin = mags;
      }
      //logMax = log2(fftMax);

      if(fftMax > fftMaxMax) fftMaxMax += fftMax * 0.1;
      //logMax = log2(fftMaxMax);
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
      Adafruit_ILI9340_setVerticalScrollStartAddress((/*119 -*/ waterfallScanLine) /*+ 200*/);

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



			/* AM is envelope-detected in the time domain at the output tap (not
			   here): taking |z| per FFT block then overlap-saving is nonlinear and
			   corrupts the reconstruction. Keep the bank linear (complex). */

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

			/* AM envelope done at the output tap - keep the bank linear (complex). */

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

			/* AM envelope done at the output tap - keep the bank linear (complex). */

			passBandRms = calculateRmsOfSample(samplesC, FFT_BUFFER_SIZE);

			sampleBankCReady = 0;

		}

		//blink_led_off();
		sampleRun = 0;
		newWaterFallData = 1;
	}

	//clearTimUpdateFlag(&TimHandle4);
}

void updateVfo(void)
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
	TimHandle.Init.Period = 449; //48.000 kHz exactly: 21.6MHz/(449+1). TIM3 alone now drives captureSamples (TIM4 only runs processStream), so sampling is uniform. Was 899 (24 kHz) while BOTH timers fired captureSamples -> an effective but jittery ~48 kHz.
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
		TimHandle4.Init.Period = 899; //match TIM3 at 24.000 kHz (was 1050).
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

void TIM3_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&TimHandle);
}

void TIM4_IRQHandler(void)
{
  processStream();
  HAL_TIM_IRQHandler(&TimHandle4);
}

int ledState = 0;
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	/* captureSamples() is now driven by the ADC DMA half/full-complete ISR at a
	   hardware-paced 48 kHz (see adc.c initAdcDMA). Neither TIM3 nor TIM4 calls
	   it here anymore. TIM4 still runs processStream() in TIM4_IRQHandler; TIM3
	   is now idle (left started, harmless) - its period no longer sets Fs. */
	(void)htim;
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
  // DISABLED: SCB_EnableDCache() hard-faults on this rebuild. The Cortex-M7
  // speculatively prefetches cacheable regions; without an MPU marking the
  // non-existent memory non-cacheable, that triggers a bus fault at cache
  // enable. I-cache alone is safe. TODO: add an MPU config, then re-enable.
  // SCB_EnableDCache();
}

void shutDown(void)
{
	state_save();   /* stash freq/mode/filter/HMC so they come back on wake */

	/* Low-power OFF via STOP mode (not STANDBY). STOP keeps the core domain
	   powered, so GPIO output levels are HELD for the whole sleep - including
	   the audio-amp shutdown pin (PE14/nSHDN, driven low in
	   teardownPeripheralPower) - which stops the amp draining the battery once
	   it is fed from LIPO instead of +3V3_CPU. Wake on the encoder push-button
	   (PB9, idle-high / press-low) and cold-boot. STANDBY could not do this: it
	   floats PE14. Sleep current is higher than STANDBY (~100 uA vs ~3 uA) but
	   still far below the amp's ~3 mA, and it needs no extra hardware.
	   NOTE: verify sleep current + button-wake on the bench before relying on it. */

	/* Make the encoder button the ONLY wake source: disable + clear every NVIC
	   IRQ (USB SOF, ADC DMA, timers would otherwise bounce us straight out of
	   STOP). We cold-boot on wake, so throwing away the IRQ config is fine. */
	for (int i = 0; i < 8; i++) { NVIC->ICER[i] = 0xFFFFFFFFu; NVIC->ICPR[i] = 0xFFFFFFFFu; }
	__DSB(); __ISB();

	/* Arm PB9 (encoder button) as a falling-edge EXTI wake. */
	__HAL_RCC_SYSCFG_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	GPIO_InitTypeDef w = {0};
	w.Pin  = encoderP.pin;             /* PB9 */
	w.Mode = GPIO_MODE_IT_FALLING;     /* press pulls the line low */
	w.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(encoderP.port, &w);
	__HAL_GPIO_EXTI_CLEAR_IT(encoderP.pin);
	HAL_NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

	HAL_SuspendTick();
	HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

	/* Woken by the button. PLL is off (HSI running); a clean cold boot
	   re-inits clocks + peripherals and re-enables the amp. */
	HAL_NVIC_SystemReset();
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
