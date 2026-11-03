/* usb_serial.c - USB CDC serial console: init + printf + command parser. */
#include "usb_serial.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

USBD_HandleTypeDef hUsbDeviceFS;

/* Firmware state exposed over the console. */
extern volatile int      g_filter_in;
extern float             afGain;
extern uint16_t          maxAmplitude;
extern volatile uint32_t g_hmc1023_readback;
extern volatile uint32_t g_hmc1023_reg1;
extern volatile uint32_t g_hmc1023_reg2;
extern volatile int      g_amp_in;
extern volatile int      g_amp_pol;
extern volatile int      g_trx;
extern volatile int      g_txrf;
extern volatile int      g_imp;
extern volatile int      g_bench;
extern volatile int      g_filt_ch;
extern float             passBandRms;   /* the real S-meter: what drives the red bar (drawSMeter) */
extern long              vfoAFrequency;
void     hmc1023_write(uint32_t reg, uint32_t val);
uint32_t hmc1023_read(uint32_t reg);
void     hmc1023_config_rx(void);
void     lpf_power(int on);
void     setGainPot(uint8_t a, uint8_t b);
static uint16_t g_gain_pot = 200;   /* last MAX5388 value set over the console */

#define CMD_MAX 64
static volatile char     cmdBuf[CMD_MAX];
static volatile uint16_t cmdLen = 0;
static volatile uint8_t  cmdReady = 0;
static volatile uint8_t  g_stream = 0;   /* periodic S-meter streaming */

void usb_serial_init(void)
{
	USBD_Init(&hUsbDeviceFS, &CDC_Desc, DEVICE_FS);
	USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC);
	USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS);
	USBD_Start(&hUsbDeviceFS);
}

void usb_printf(const char *fmt, ...)
{
	char buf[128];
	va_list ap; va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n <= 0) return;
	if (n > (int)sizeof(buf)) n = (int)sizeof(buf);
	for (int tries = 0; tries < 4000; tries++)
		if (CDC_Transmit_FS((uint8_t *)buf, (uint16_t)n) == USBD_OK) return;
}

/* USB IRQ context: buffer bytes, flag a complete line on CR/LF. */
void usb_serial_on_rx(uint8_t *buf, uint32_t len)
{
	for (uint32_t i = 0; i < len; i++)
	{
		char c = (char)buf[i];
		if (c == '\r' || c == '\n') { if (cmdLen) cmdReady = 1; }
		else if (!cmdReady && cmdLen < CMD_MAX - 1) cmdBuf[cmdLen++] = c;
	}
}

static void print_status(void)
{
	unsigned long rms_u = (unsigned long)(passBandRms * 1000000.0f);
	int bar = (int)(10.0 * log((double)passBandRms * 1000000.0 + 1.0));
	usb_printf("rms=%lu bar=%d S=%u vfo=%ld filt=%d mux=%d amp=%d/pol%d trx=%d txrf=%d imp=%d reg2=%03lX\r\n",
	           rms_u, bar, (unsigned)maxAmplitude, vfoAFrequency, g_filter_in, g_filt_ch, g_amp_in, g_amp_pol,
	           g_trx, g_txrf, g_imp, (unsigned long)g_hmc1023_reg2);
	maxAmplitude = 0;   /* peak since last read */
}

static void handle_cmd(char *s)
{
	switch (s[0])
	{
	case 's': print_status(); break;
	case 'P': {   /* quick S-meter read: scaled passband RMS + red-bar width */
		unsigned long rms_u = (unsigned long)(passBandRms * 1000000.0f);
		int bar = (int)(10.0 * log((double)passBandRms * 1000000.0 + 1.0));
		usb_printf("rms=%lu bar=%d\r\n", rms_u, bar);
	} break;
	case 'f': g_filter_in = (s[1] == '1'); usb_printf("filter %s\r\n", g_filter_in ? "IN (B4)" : "BYPASS (B2)"); break;
	case 'a': g_amp_in = (s[1] == '1'); usb_printf("amp %s\r\n", g_amp_in ? "ON + inserted" : "off"); break;
	case 'A': g_amp_pol = (s[1] == '1'); usb_printf("amp switch pol=%d\r\n", g_amp_pol); break;
	case 'm': g_stream = (s[1] == '1'); usb_printf("stream %s\r\n", g_stream ? "on" : "off"); break;
	case 'v': afGain = (float)atof(s + 1); usb_printf("afGain=%d/1000\r\n", (int)(afGain * 1000.0f)); break;
	case 'F': { long hz = strtol(s + 1, 0, 10); if (hz >= 2500000L && hz <= 175000000L) vfoAFrequency = hz;
	            usb_printf("vfo=%ld\r\n", vfoAFrequency); } break;
	case 'G': { int v = atoi(s + 1); if (v < 0) v = 0; if (v > 255) v = 255; g_gain_pot = (uint16_t)v;
	            setGainPot((uint8_t)v, (uint8_t)v); usb_printf("gainpot=%u\r\n", g_gain_pot); } break;
	case 'g': g_bench = (s[1] == '1'); usb_printf("bench=%d\r\n", g_bench); break;
	case 'M': g_filt_ch = (s[1] - '0') & 3; usb_printf("mux ch=%d (S1=%d S0=%d) %s\r\n", g_filt_ch,
	          (g_filt_ch >> 1) & 1, g_filt_ch & 1,
	          g_filt_ch==1?"bypass":g_filt_ch==0?"ext(DNF)":"HMC1023"); break;
	case 'L': lpf_power(s[1] == '1'); usb_printf("lpf_power %s\r\n", s[1] == '1' ? "ON" : "OFF"); break;
	case 'H': hmc1023_config_rx();   /* power + enable I/Q channels + 10 dB gain (reg1=1C reg2=115) */
	          usb_printf("hmc1023 RX cfg: reg1=%06lX reg2=%06lX (then g1+M2 to route through it)\r\n",
	                     (unsigned long)g_hmc1023_reg1, (unsigned long)g_hmc1023_reg2); break;
	case 't': g_trx  = (s[1] == '1'); usb_printf("trx=%d\r\n", g_trx); break;
	case 'x': g_txrf = (s[1] == '1'); usb_printf("txrf=%d\r\n", g_txrf); break;
	case 'b': g_imp  = (s[1] == '1'); usb_printf("imp=%d\r\n", g_imp); break;
	case 'r': { uint32_t reg = (uint32_t)(s[1] - '0'); uint32_t val = strtoul(s + 2, 0, 16);
	            hmc1023_write(reg, val); usb_printf("wrote reg%lu=%lX\r\n", (unsigned long)reg, (unsigned long)val); } break;
	case 'R': { uint32_t reg = (uint32_t)(s[1] - '0'); uint32_t val = hmc1023_read(reg);
	            usb_printf("reg%lu=%06lX\r\n", (unsigned long)reg, (unsigned long)val); } break;
	default:  usb_printf("cmds: s | F<hz> | f filter | a amp | A ampPol | t trx | x txrf | b impbridge | r<reg> <hex> | R<reg> | m stream | v<gain>\r\n"); break;
	}
}

void usb_serial_poll(void)
{
	if (g_stream)
	{
		static uint32_t sc = 0;
		if (++sc >= 10) { sc = 0; usb_printf("S=%u f=%d a=%d\r\n", (unsigned)maxAmplitude, g_filter_in, g_amp_in); maxAmplitude = 0; }
	}
	if (!cmdReady) return;
	char local[CMD_MAX];
	uint16_t n = cmdLen;
	if (n >= CMD_MAX) n = CMD_MAX - 1;
	for (uint16_t i = 0; i < n; i++) local[i] = cmdBuf[i];
	local[n] = 0;
	cmdLen = 0; cmdReady = 0;
	handle_cmd(local);
}
