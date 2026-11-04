#include "adc.h"

/* ====================================================================== *
 * DMA-driven oversampling capture (2026-11-03)
 *
 * TIM6 TRGO fires ADC1(mic)/ADC2(I)/ADC3(Q) SIMULTANEOUSLY at 48 kHz*ADC_OS.
 * Each ADC circular-DMAs into a 2*ADC_OS buffer; the half/full-complete ISR
 * (at exactly 48 kHz) averages the ADC_OS samples that just landed, publishes
 * them to uhADCxConvertedValueN, and runs ONE captureSamples(). The average is
 * a sinc anti-alias low-pass + ~sqrt(ADC_OS) noise reduction, and the shared
 * trigger samples I and Q at the same instant (removes the inter-ADC skew that
 * free-running independent ADCs added to the quadrature error). Replaces the
 * old free-running ADCs + polled adcGetConversion(). Old initAdc()/callback
 * kept below as dead code for easy revert.
 *
 * ADC_OS = 15: TIM6 = 108 MHz/150 = 720 kHz = 48000*15 (exact). ADC conv at
 * 27 MHz ADCCLK, 15-cyc sampling + 12-bit = ~1.0 us < 1.39 us trigger period.
 * ====================================================================== */
#define ADC_OS        15U
#define ADC_DMA_LEN   (ADC_OS * 2U)             /* circular, double-buffered (HT + TC) */

static volatile uint16_t adcMicBuf[ADC_DMA_LEN];
static volatile uint16_t adcIBuf[ADC_DMA_LEN];
static volatile uint16_t adcQBuf[ADC_DMA_LEN];
static DMA_HandleTypeDef hdmaMic, hdmaI, hdmaQ;
static TIM_HandleTypeDef hAdcTrig;              /* TIM6 */

extern void captureSamples(void);               /* main.c, driven here off the 48 kHz DMA ISR */
void initAdcDMA(void);

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
		/* No-op now: the DMA half/full-complete ISR (adc_block) averages the
		   oversampled DMA buffers into uhADCxConvertedValue1/2/3 immediately
		   before it calls captureSamples(), so the values are already fresh.
		   (Old polled body kept in git history.) */
	}


	void adcStartConversion(void)
	{
		/* Arm all three ADCs on their circular DMA, then release TIM6 so its
		   TRGO starts simultaneous conversions. Idempotent: the RX path calls
		   this again on mode changes, but the DMA runs continuously. */
		static uint8_t started = 0;
		if(started) return;
		started = 1;

		if(HAL_ADC_Start_DMA(&AdcHandle3, (uint32_t *)adcQBuf,   ADC_DMA_LEN) != HAL_OK) wrongThings++;
		if(HAL_ADC_Start_DMA(&AdcHandle1, (uint32_t *)adcMicBuf, ADC_DMA_LEN) != HAL_OK) wrongThings++;
		if(HAL_ADC_Start_DMA(&AdcHandle2, (uint32_t *)adcIBuf,   ADC_DMA_LEN) != HAL_OK) wrongThings++;
		if(HAL_TIM_Base_Start(&hAdcTrig) != HAL_OK) wrongThings++;
	}


	//__IO uint16_t uhADCxConvertedValue1 = 0;


	/* ---- DMA oversampling: average + drive one DSP sample ---------------- */
	static uint16_t adc_avg(volatile uint16_t *b, uint32_t s)
	{
		uint32_t a = 0;
		for(uint32_t i = 0; i < ADC_OS; i++) a += b[s + i];
		return (uint16_t)(a / ADC_OS);
	}

	/* One 48 kHz output sample from the ADC_OS-sample half starting at `s`.
	   All three buffers filled the same half (shared TIM6 trigger), so average
	   each, publish, and run exactly one captureSamples(). */
	static void adc_block(uint32_t s)
	{
		uhADCxConvertedValue1 = adc_avg(adcMicBuf, s);
		uhADCxConvertedValue2 = adc_avg(adcIBuf,   s);
		uhADCxConvertedValue3 = adc_avg(adcQBuf,   s);
		captureSamples();
	}

	/* Only ADC2 (I) carries the NVIC IRQ, so only its callbacks fire. */
	void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *h)
	{
		if(h->Instance == ADC2) adc_block(0);          /* first half just filled  */
	}
	void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h)
	{
		if(h->Instance == ADC2) adc_block(ADC_OS);     /* second half just filled */
	}

	/* ADC2's DMA stream is the one with an enabled NVIC IRQ. */
	void DMA2_Stream2_IRQHandler(void)
	{
		HAL_DMA_IRQHandler(&hdmaI);
	}

	static void adc_dma_cfg(DMA_HandleTypeDef *h, DMA_Stream_TypeDef *st, uint32_t ch, ADC_HandleTypeDef *adc)
	{
		h->Instance = st;
		h->Init.Channel = ch;
		h->Init.Direction = DMA_PERIPH_TO_MEMORY;
		h->Init.PeriphInc = DMA_PINC_DISABLE;
		h->Init.MemInc = DMA_MINC_ENABLE;
		h->Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
		h->Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
		h->Init.Mode = DMA_CIRCULAR;
		h->Init.Priority = DMA_PRIORITY_HIGH;
		h->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
		if(HAL_DMA_Init(h) != HAL_OK) wrongThings++;
		__HAL_LINKDMA(adc, DMA_Handle, *h);
	}

	static void adc_one_init(ADC_HandleTypeDef *a, ADC_TypeDef *inst, ADC_ChannelConfTypeDef *sc, uint32_t chan)
	{
		a->Instance = inst;
		a->Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV4;
		a->Init.Resolution = ADC_RESOLUTION12b;
		a->Init.ScanConvMode = DISABLE;
		a->Init.ContinuousConvMode = DISABLE;             /* triggered, not free-running */
		a->Init.DiscontinuousConvMode = DISABLE;
		a->Init.NbrOfDiscConversion = 0;
		a->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
		a->Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T6_TRGO;
		a->Init.DataAlign = ADC_DATAALIGN_RIGHT;
		a->Init.NbrOfConversion = 1;
		a->Init.DMAContinuousRequests = ENABLE;
		a->Init.EOCSelection = DISABLE;
		if(HAL_ADC_Init(a) != HAL_OK) wrongThings++;
		sc->Channel = chan;
		sc->Rank = 1;
		sc->SamplingTime = ADC_SAMPLETIME_15CYCLES;
		sc->Offset = 0;
		if(HAL_ADC_ConfigChannel(a, sc) != HAL_OK) wrongThings++;
	}

	void initAdcDMA(void)
	{
		uhADCxConvertedValue1 = 0; uhADCxConvertedValue2 = 0; uhADCxConvertedValue3 = 0;
		adcConfigured = 0; sampleIndex = 100; sampleRun = 0; wrongThings = 0;

		__ADC1_CLK_ENABLE(); __ADC2_CLK_ENABLE(); __ADC3_CLK_ENABLE();
		__GPIOA_CLK_ENABLE(); __GPIOB_CLK_ENABLE(); __GPIOC_CLK_ENABLE();
		__HAL_RCC_DMA2_CLK_ENABLE();
		__HAL_RCC_TIM6_CLK_ENABLE();
		__HAL_RCC_ADC_FORCE_RESET(); __HAL_RCC_ADC_RELEASE_RESET();

		adc_one_init(&AdcHandle1, ADC1, &sConfig1, ADC_MIC_CHANNEL);
		adc_one_init(&AdcHandle2, ADC2, &sConfig2, ADC_RX_I_CHANNEL);
		adc_one_init(&AdcHandle3, ADC3, &sConfig3, ADC_RX_Q_CHANNEL);

		/* DMA2 request map (RM0385 Table 28): ADC1=Str0/Ch0, ADC2=Str2/Ch1, ADC3=Str1/Ch2 */
		adc_dma_cfg(&hdmaMic, DMA2_Stream0, DMA_CHANNEL_0, &AdcHandle1);
		adc_dma_cfg(&hdmaI,   DMA2_Stream2, DMA_CHANNEL_1, &AdcHandle2);
		adc_dma_cfg(&hdmaQ,   DMA2_Stream1, DMA_CHANNEL_2, &AdcHandle3);

		/* Only ADC2 (I) drives captureSamples -> only its DMA stream gets a NVIC IRQ.
		   ADC1/ADC3 DMA run circular without ISRs; their buffers just stay fresh. */
		HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 1, 0);
		HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);

		/* TIM6: 108 MHz / 150 = 720 kHz = 48 kHz * 15, TRGO on update */
		hAdcTrig.Instance = TIM6;
		hAdcTrig.Init.Prescaler = 0;
		hAdcTrig.Init.CounterMode = TIM_COUNTERMODE_UP;
		hAdcTrig.Init.Period = 149;
		hAdcTrig.Init.ClockDivision = 0;
		if(HAL_TIM_Base_Init(&hAdcTrig) != HAL_OK) wrongThings++;
		TIM_MasterConfigTypeDef mcfg = {0};
		mcfg.MasterOutputTrigger = TIM_TRGO_UPDATE;
		mcfg.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
		HAL_TIMEx_MasterConfigSynchronization(&hAdcTrig, &mcfg);
		/* TIM6 is released in adcStartConversion(), after the ADCs are armed. */
	}

	/* Battery voltage on LIPO_V = PC0 = ADC1_IN10. The mic (ADC1 regular/DMA
	   group) is idle during RX, so we read the battery on ADC1's INJECTED group
	   - it inserts a single conversion between the regular ones without
	   disturbing the mic/demod and works in TX too. */
	void battery_adc_init(void)
	{
		GPIO_InitTypeDef g = {0};
		g.Pin = GPIO_PIN_0; g.Mode = GPIO_MODE_ANALOG; g.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOC, &g);                      /* PC0 analog */

		ADC_InjectionConfTypeDef inj = {0};
		inj.InjectedChannel = ADC_CHANNEL_10;
		inj.InjectedRank = ADC_INJECTED_RANK_1;
		inj.InjectedNbrOfConversion = 1;
		inj.InjectedSamplingTime = ADC_SAMPLETIME_480CYCLES;   /* high-Z divider */
		inj.ExternalTrigInjecConv = ADC_INJECTED_SOFTWARE_START;
		inj.ExternalTrigInjecConvEdge = ADC_EXTERNALTRIGINJECCONVEDGE_NONE;
		inj.AutoInjectedConv = DISABLE;
		inj.InjectedDiscontinuousConvMode = DISABLE;
		inj.InjectedOffset = 0;
		HAL_ADCEx_InjectedConfigChannel(&AdcHandle1, &inj);
	}

	uint16_t battery_read_raw(void)
	{
		if(HAL_ADCEx_InjectedStart(&AdcHandle1) != HAL_OK) return 0;
		HAL_ADCEx_InjectedPollForConversion(&AdcHandle1, 5);
		uint16_t v = (uint16_t)HAL_ADCEx_InjectedGetValue(&AdcHandle1, ADC_INJECTED_RANK_1);
		HAL_ADCEx_InjectedStop(&AdcHandle1);
		return v;
	}


