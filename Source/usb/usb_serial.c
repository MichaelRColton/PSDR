/* usb_serial.c - USB CDC serial console: init + printf + command parser. */
#include "usb_serial.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

USBD_HandleTypeDef hUsbDeviceFS;

/* Firmware state exposed over the console. */
extern volatile int      g_filter_in;
extern float             afGain;
extern uint16_t          maxAmplitude;
extern volatile uint32_t g_hmc1023_readback;
extern volatile uint32_t g_hmc1023_reg1;
extern volatile uint32_t g_hmc1023_reg2;

#define CMD_MAX 64
static volatile char     cmdBuf[CMD_MAX];
static volatile uint16_t cmdLen = 0;
static volatile uint8_t  cmdReady = 0;

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

static void handle_cmd(char *s)
{
	if      (s[0] == 'f' && s[1] == '1') { g_filter_in = 1; usb_printf("filter IN (B4)\r\n"); }
	else if (s[0] == 'f' && s[1] == '0') { g_filter_in = 0; usb_printf("filter BYPASS (B2)\r\n"); }
	else if (s[0] == 's')
		usb_printf("S=%u filt=%d reg0=%06lX reg1=%02lX reg2=%03lX\r\n",
		           (unsigned)maxAmplitude, g_filter_in,
		           (unsigned long)g_hmc1023_readback,
		           (unsigned long)g_hmc1023_reg1,
		           (unsigned long)g_hmc1023_reg2);
	else if (s[0] == 'v') { afGain = (float)atof(s + 1); usb_printf("afGain=%d/1000\r\n", (int)(afGain * 1000.0f)); }
	else usb_printf("cmds: f1/f0 filter, s status, v<gain>\r\n");
}

void usb_serial_poll(void)
{
	if (!cmdReady) return;
	char local[CMD_MAX];
	uint16_t n = cmdLen;
	if (n >= CMD_MAX) n = CMD_MAX - 1;
	for (uint16_t i = 0; i < n; i++) local[i] = cmdBuf[i];
	local[n] = 0;
	cmdLen = 0; cmdReady = 0;
	handle_cmd(local);
}
