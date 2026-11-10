/*
 * GPS receiver (U185, Linx RXM-GPS-RM) on USART6.
 *
 * Wiring, from the 2.6.1 netlist (Hardware/PSDR.asc):
 *   RX_TO_GPS   = U121.63 = PC6 (USART6_TX)  -> GPS serial in
 *   TX_FROM_GPS = U121.64 = PC7 (USART6_RX)  <- GPS serial out (NMEA, 9600 8N1)
 *   GPS_POWER   = U121.67 = PA8              -> power switch (0 = on, 1 = off)
 *   GPS_RESET   = U121.62 = PD15             -> module reset   (hal.c had PE15, which is an LED)
 *   GPS_PPS     = U121.37 = PE7              <- 1 pulse per second
 *
 * This file only receives: bytes come in on the USART6 interrupt into a ring
 * buffer, gps_poll() (main loop) splits them into NMEA sentences, checks the
 * checksum and keeps the latest time, date, fix and satellite count from RMC
 * and GGA. Console 'N' reports it. Nothing here sets the RTC or the display
 * yet; that is the next step once the module has been seen to get a fix.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "stm32f7xx_hal.h"

void usb_printf(const char *fmt, ...);

#define GPS_RING 512
static volatile uint8_t  gpsRing[GPS_RING];
static volatile uint16_t gpsHead = 0;
static uint16_t gpsTail = 0;

volatile uint32_t g_gpsBytes = 0;      /* bytes received since boot */
volatile uint32_t g_gpsOverruns = 0;
uint32_t g_gpsSentences = 0;           /* sentences with a good checksum */
uint32_t g_gpsBadChecksum = 0;
static char gpsLine[100];
static int  gpsLineLen = 0;
static char gpsLastRmc[100] = "";
static char gpsLastGga[100] = "";
static char gpsLastOther[100] = "";

/* parsed */
int  g_gpsFixValid = 0;                /* RMC status 'A' */
int  g_gpsFixQuality = 0;              /* GGA: 0 none, 1 GPS, 2 DGPS */
int  g_gpsSats = 0;
int  g_gpsHour = -1, g_gpsMin = 0, g_gpsSec = 0;       /* UTC */
int  g_gpsDay = 0, g_gpsMonth = 0, g_gpsYear = 0;
char g_gpsLat[16] = "", g_gpsLon[16] = "", g_gpsNS = ' ', g_gpsEW = ' ';
static int gpsPowerLevel = -1;

void gps_power(int level)              /* raw level on GPS_POWER (PA8). Measured: 0 = module ON, 1 = OFF. hal.c leaves it at 0. */
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
	gpsPowerLevel = level ? 1 : 0;
}

void gps_reset_pulse(void)             /* active-low reset on PD15 */
{
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
	HAL_Delay(20);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET);
}

void gps_set_baud(uint32_t baud)
{
	USART6->CR1 &= ~USART_CR1_UE;
	USART6->BRR = (HAL_RCC_GetPCLK2Freq() + baud / 2U) / baud;   /* oversampling by 16 */
	USART6->CR1 |= USART_CR1_UE;
}

void gps_init(void)
{
	GPIO_InitTypeDef g = {0};
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_USART6_CLK_ENABLE();

	g.Pin = GPIO_PIN_6 | GPIO_PIN_7;           /* PC6 = TX, PC7 = RX */
	g.Mode = GPIO_MODE_AF_PP;
	g.Pull = GPIO_PULLUP;
	g.Speed = GPIO_SPEED_LOW;
	g.Alternate = GPIO_AF8_USART6;
	HAL_GPIO_Init(GPIOC, &g);

	g.Pin = GPIO_PIN_15;                       /* reset line, held high (not in reset) */
	g.Mode = GPIO_MODE_OUTPUT_PP;
	g.Pull = GPIO_NOPULL;
	g.Alternate = 0;
	HAL_GPIO_Init(GPIOD, &g);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET);

	/* 9600 8N1, receiver + transmitter, RX interrupt. Registers directly: the
	   HAL UART module is not enabled in this build and this is all we need. */
	USART6->CR1 = 0;
	USART6->CR2 = 0;
	USART6->CR3 = 0;
	USART6->BRR = (HAL_RCC_GetPCLK2Freq() + 4800U) / 9600U;
	USART6->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE;
	USART6->CR1 |= USART_CR1_UE;
	HAL_NVIC_SetPriority(USART6_IRQn, 8, 0);   /* below audio capture, USB and the display DMA */
	HAL_NVIC_EnableIRQ(USART6_IRQn);
}

void USART6_IRQHandler(void)
{
	uint32_t isr = USART6->ISR;
	if(isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE))
	{
		if(isr & USART_ISR_ORE) g_gpsOverruns++;
		USART6->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_PECF;
	}
	if(isr & USART_ISR_RXNE)
	{
		uint8_t c = (uint8_t)USART6->RDR;
		uint16_t next = (uint16_t)((gpsHead + 1U) % GPS_RING);
		if(next != gpsTail) { gpsRing[gpsHead] = c; gpsHead = next; }
		g_gpsBytes++;
	}
}

/* copy field n (0 = the sentence name) of a comma-separated sentence into out */
static void gps_field(const char *s, int n, char *out, int outLen)
{
	int i = 0;
	while(n > 0 && *s) { if(*s == ',') n--; s++; }
	while(*s && *s != ',' && *s != '*' && i < outLen - 1) out[i++] = *s++;
	out[i] = 0;
}

static void gps_sentence(char *s)
{
	/* "$GPRMC,hhmmss.sss,A,llll.ll,N,yyyyy.yy,W,spd,crs,ddmmyy,...*CS" */
	char *star = strchr(s, '*');
	if(s[0] != '$' || !star) return;
	uint8_t sum = 0;
	for(char *p = s + 1; p < star; p++) sum ^= (uint8_t)*p;
	if((uint8_t)strtol(star + 1, 0, 16) != sum) { g_gpsBadChecksum++; return; }
	g_gpsSentences++;

	char f[20];
	if(strlen(s) > 6 && !strncmp(s + 3, "RMC", 3))
	{
		strncpy(gpsLastRmc, s, sizeof(gpsLastRmc) - 1);
		gps_field(s, 1, f, sizeof(f));
		if(strlen(f) >= 6)
		{
			g_gpsHour = (f[0] - '0') * 10 + (f[1] - '0');
			g_gpsMin  = (f[2] - '0') * 10 + (f[3] - '0');
			g_gpsSec  = (f[4] - '0') * 10 + (f[5] - '0');
		}
		gps_field(s, 2, f, sizeof(f)); g_gpsFixValid = (f[0] == 'A');
		gps_field(s, 3, g_gpsLat, sizeof(g_gpsLat));
		gps_field(s, 4, f, sizeof(f)); g_gpsNS = f[0] ? f[0] : ' ';
		gps_field(s, 5, g_gpsLon, sizeof(g_gpsLon));
		gps_field(s, 6, f, sizeof(f)); g_gpsEW = f[0] ? f[0] : ' ';
		gps_field(s, 9, f, sizeof(f));
		if(strlen(f) >= 6)
		{
			g_gpsDay   = (f[0] - '0') * 10 + (f[1] - '0');
			g_gpsMonth = (f[2] - '0') * 10 + (f[3] - '0');
			int yy = (f[4] - '0') * 10 + (f[5] - '0');
			g_gpsYear = (yy >= 80) ? 1900 + yy : 2000 + yy;   /* with no fix the module reports its 1980 epoch */
		}
	}
	else if(strlen(s) > 6 && !strncmp(s + 3, "GGA", 3))
	{
		strncpy(gpsLastGga, s, sizeof(gpsLastGga) - 1);
		gps_field(s, 6, f, sizeof(f)); g_gpsFixQuality = atoi(f);
		gps_field(s, 7, f, sizeof(f)); g_gpsSats = atoi(f);
	}
	else strncpy(gpsLastOther, s, sizeof(gpsLastOther) - 1);
}

void gps_poll(void)                    /* call from the main loop */
{
	while(gpsTail != gpsHead)
	{
		char c = (char)gpsRing[gpsTail];
		gpsTail = (uint16_t)((gpsTail + 1U) % GPS_RING);
		if(c == '$') gpsLineLen = 0;
		if(c == '\r' || c == '\n')
		{
			if(gpsLineLen > 6) { gpsLine[gpsLineLen] = 0; gps_sentence(gpsLine); }
			gpsLineLen = 0;
		}
		else if(gpsLineLen < (int)sizeof(gpsLine) - 1) gpsLine[gpsLineLen++] = c;
	}
}

void gps_report(void)                  /* console 'N' */
{
	usb_printf("gps: power pin=%d bytes=%lu sentences=%lu badsum=%lu overruns=%lu pps=%d\r\n", gpsPowerLevel,
	           (unsigned long)g_gpsBytes, (unsigned long)g_gpsSentences, (unsigned long)g_gpsBadChecksum,
	           (unsigned long)g_gpsOverruns, (int)HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_7));
	usb_printf("gps: fix=%d quality=%d sats=%d utc=%02d:%02d:%02d date=%04d-%02d-%02d lat=%s%c lon=%s%c\r\n",
	           g_gpsFixValid, g_gpsFixQuality, g_gpsSats, g_gpsHour < 0 ? 0 : g_gpsHour, g_gpsMin, g_gpsSec,
	           g_gpsYear, g_gpsMonth, g_gpsDay, g_gpsLat, g_gpsNS, g_gpsLon, g_gpsEW);
	if(gpsLastRmc[0])   usb_printf("gps: %s\r\n", gpsLastRmc);
	if(gpsLastGga[0])   usb_printf("gps: %s\r\n", gpsLastGga);
	if(gpsLastOther[0]) usb_printf("gps: %s\r\n", gpsLastOther);
}
