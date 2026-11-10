/* usb_serial.c - USB CDC serial console: init + printf + command parser. */
#include "usb_serial.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "usbd_cdc_audio.h"
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
extern volatile int      g_filt_ch;
extern float             passBandRms;   /* the real S-meter: what drives the red bar (drawSMeter) */
extern long              vfoAFrequency;
extern void              setFreq(unsigned long frequency);   /* reprogram the Si5338 LO (i2c.c) */
extern float             usbAudioGain;     /* gain on the USB demod-mic feed      */
extern float             usbIqGain;        /* gain on the USB stereo-I/Q feed     */
extern uint8_t           mode;             /* demod: 0=LSB 1=USB 2=AM             */
extern int               g_lpfMode;         /* LPF menu setting: 0 = bypass, 1 = 0 dB, 2 = +20 dB */
extern void              lpfModeSet(int m);
extern uint16_t          g_battRaw;          /* battery ADC raw (LIPO_V) */
extern uint16_t          g_vrefRaw;          /* VREFINT raw (for VDDA calc) */
extern float             g_battVolts;        /* battery volts (g_battRaw * g_battScale) */
extern float             g_battScale;        /* volts per ADC count (calibratable) */
extern void              lcd_read_regs(uint8_t cmd, uint8_t *out, int n);   /* bit-bang LCD readback */
extern void              lcd_screenshot(int y0, int y1);                    /* stream framebuffer rows [y0,y1) */
extern void              rtc_set(int y, int mo, int d, int h, int mi, int s);
extern void              rtc_format(char *buf, int buflen);
extern volatile float    g_testTone;        /* audio self-test: sine freq Hz (0=off) */
extern volatile float    g_testAmp;         /* audio self-test: amplitude pre-afGain  */
extern int16_t           filterUpperLimit; /* DSP filter LPF edge, FFT-bin index  */
extern int16_t           filterLowerLimit; /* DSP filter HPF edge, FFT-bin index  */
extern volatile int      iqCorrEnable;     /* adaptive I/Q imbalance correction on/off */
extern float             iq_c1;            /* I/Q correction: correlation (phase) term  */
extern float             iq_c2;            /* I/Q correction: amplitude normalization   */
void     populateCoeficients(int bandwidth, int sideband, int offset);
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
	USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC_AUDIO);   /* composite: CDC console + UAC mic */
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
		if (usb_cdc_tx((uint8_t *)buf, (uint16_t)n) == USBD_OK) return;
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
	usb_printf("rms=%lu bar=%d S=%u vfo=%ld filt=%d mux=%d amp=%d/pol%d trx=%d txrf=%d imp=%d reg2=%03lX mode=%d flo=%d fhi=%d iqc=%d c1=%d c2=%d lpf=%d\r\n",
	           rms_u, bar, (unsigned)maxAmplitude, vfoAFrequency, g_filter_in, g_filt_ch, g_amp_in, g_amp_pol,
	           g_trx, g_txrf, g_imp, (unsigned long)g_hmc1023_reg2, mode, filterLowerLimit, filterUpperLimit,
	           iqCorrEnable, (int)(iq_c1 * 1000.0f), (int)(iq_c2 * 1000.0f), g_lpfMode);
	usb_printf("battRaw=%u battV=%d/100 scale=%d/1e6 vref=%u cal=%u\r\n",
	           (unsigned)g_battRaw, (int)(g_battVolts * 100.0f), (int)(g_battScale * 1000000.0f),
	           (unsigned)g_vrefRaw, (unsigned)(*((uint16_t*)0x1FF0F44A)));
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
	case 'C': {   /* set the clock: C <year> <month> <day> <hour> <min> <sec> */
	            char *p = s + 1; long y = strtol(p,&p,10), mo = strtol(p,&p,10), d = strtol(p,&p,10);
	            long h = strtol(p,&p,10), mi = strtol(p,&p,10), se = strtol(p,&p,10);
	            rtc_set((int)y,(int)mo,(int)d,(int)h,(int)mi,(int)se);
	            char tb[40]; rtc_format(tb, sizeof(tb)); usb_printf("rtc set: %s\r\n", tb); } break;
	case 'c': { char tb[40]; rtc_format(tb, sizeof(tb)); usb_printf("rtc: %s\r\n", tb); } break;
	case 'V': {   /* calibrate battery: V<volts x100> sets scale from the current raw reading */
	            int cv = atoi(s + 1);
	            if(cv > 0 && g_battRaw > 0) { g_battScale = (cv / 100.0f) / (float)g_battRaw;
	                usb_printf("batt cal: %d/100 V at raw %u -> scale=%d/1e6\r\n",
	                           cv, (unsigned)g_battRaw, (int)(g_battScale * 1000000.0f)); }
	            else usb_printf("batt cal needs V>0 and a nonzero raw\r\n"); } break;
	case 'p': {   /* screenshot band: p<0-3> = a 60-row band (CDC bulk stalls past
	                 ~148KB, so the host pulls 4 bands and stitches); 'p' = full */
	            int band = (s[1] >= '0' && s[1] <= '3') ? (s[1] - '0') : -1;
	            int y0 = (band < 0) ? 0 : band * 60;
	            int y1 = (band < 0) ? 240 : y0 + 60;
	            usb_printf("IMG 320 %d 565 %d\r\n", y1 - y0, y0);
	            lcd_screenshot(y0, y1); } break;
	case 'i': {   /* probe the LCD read path (MISO/PB4) across a few read registers */
	            uint8_t b[6];
	            lcd_read_regs(0xD3, b, 4); usb_printf("D3(id): %02X %02X %02X %02X\r\n", b[0],b[1],b[2],b[3]);
	            lcd_read_regs(0x04, b, 4); usb_printf("04(did): %02X %02X %02X %02X\r\n", b[0],b[1],b[2],b[3]);
	            lcd_read_regs(0x09, b, 5); usb_printf("09(stat): %02X %02X %02X %02X %02X\r\n", b[0],b[1],b[2],b[3],b[4]);
	            lcd_read_regs(0x0A, b, 2); usb_printf("0A(pwr): %02X %02X\r\n", b[0],b[1]); } break;
	case 'T': {   /* audio self-test tone: T<hz> (0 = off), pure sine to the speaker DACs; volume via v */
	            float hz = (float)atof(s + 1); if (hz < 0.0f) hz = 0.0f; if (hz > 20000.0f) hz = 20000.0f;
	            g_testTone = hz;
	            usb_printf("testtone=%dHz amp=%d/1000 (0=off; set volume with v)\r\n",
	                       (int)g_testTone, (int)(g_testAmp * 1000.0f)); } break;
	case 'W': {   /* audio self-test amplitude W<x1000> (pre-afGain) */
	            float a = (float)atof(s + 1) / 1000.0f; if (a < 0.0f) a = 0.0f; if (a > 0.5f) a = 0.5f;
	            g_testAmp = a; usb_printf("testamp=%d/1000\r\n", (int)(g_testAmp * 1000.0f)); } break;
	case 'X': {   /* timed transmit for bench tests: X<ms> [tone Hz] [amp 0/1]; X0 = unkey now.
	                 A tone frequency sends a test tone instead of the microphone. Max 10 s. */
	            extern volatile uint32_t g_txKeyUntil; extern volatile int g_txKeyTone, g_txKeyAmp;
	            int ms = 0, tb = 0, amp = 0;
	            sscanf(s + 1, "%d %d %d", &ms, &tb, &amp);
	            if (ms < 0) ms = 0; if (ms > 10000) ms = 10000;
	            if (tb < 0) tb = 0; if (tb > 20000) tb = 20000;
	            g_txKeyTone = tb; g_txKeyAmp = amp ? 1 : 0;
	            g_txKeyUntil = HAL_GetTick() + (uint32_t)ms;
	            usb_printf("tx key %d ms tone=%d Hz amp=%d\r\n", ms, tb, g_txKeyAmp); } break;
	case 'w': {   /* transmit power step, same as the on-device "TX" menu item: w<0-4>; w alone reports */
	            extern int g_txPower; extern void txPowerSet(int idx); extern volatile float txDrive; extern int txPowerStepAmp(int idx);
	            if (s[1] >= '0' && s[1] <= '9') txPowerSet(s[1] - '0');
	            usb_printf("txpower=%d drive=%d/100 amp=%d\r\n", g_txPower, (int)(txDrive * 100.0f), txPowerStepAmp(g_txPower)); } break;
	case 'y': {   /* TX power calibration: y<step> <dBm x10> stores this unit's measured output for a step;
	                 y alone lists them (tenths of a dBm) */
	            extern int txPowerCalGet(int idx); extern void txPowerCalSet(int idx, int dbmX10);
	            int idx = -1, v = 0;
	            if (sscanf(s + 1, "%d %d", &idx, &v) == 2) txPowerCalSet(idx, v);
	            usb_printf("txcal(dBm x10): %d %d %d %d %d\r\n", txPowerCalGet(0), txPowerCalGet(1), txPowerCalGet(2),
	                       txPowerCalGet(3), txPowerCalGet(4)); } break;
	case 'O': {   /* transmit drive (DAC scale while transmitting); O alone reports it */
	            extern volatile float txDrive;
	            if (s[1]) { float d = (float)atof(s + 1); if (d < 0.0f) d = 0.0f; if (d > 500.0f) d = 500.0f; txDrive = d; }
	            usb_printf("txdrive=%d/100\r\n", (int)(txDrive * 100.0f)); } break;
	case 'J': {   /* frequency calibration: J<ppb> sets + saves the reference error, J alone reports it */
	            extern long g_freqCalPpb; extern void freqCalSet(long ppb); extern void freqcal_save(void);
	            if (s[1] == '-' || (s[1] >= '0' && s[1] <= '9')) {
	                freqCalSet(strtol(s + 1, 0, 10));
	                freqcal_save();
	                setFreq((unsigned long)vfoAFrequency);
	            }
	            usb_printf("freqcal=%ld ppb\r\n", g_freqCalPpb); } break;
	case 'F': { long hz = strtol(s + 1, 0, 10);
	            if (hz >= 2500000L && hz <= 175000000L) {
	                vfoAFrequency = hz;
	                setFreq((unsigned long)vfoAFrequency);   /* actually retune the LO, not just the display */
	            }
	            usb_printf("vfo=%ld\r\n", vfoAFrequency); } break;
	case 'G': { int v = atoi(s + 1); if (v < 0) v = 0; if (v > 255) v = 255; g_gain_pot = (uint16_t)v;
	            setGainPot((uint8_t)v, (uint8_t)v); usb_printf("gainpot=%u\r\n", g_gain_pot); } break;
	case 'E': {   /* display speed test: 4 full-screen fills + one forced full redraw, in ms */
	            extern void Adafruit_ILI9340_fillScreen(uint16_t color);
	            extern void updateDisplay(uint8_t force);
	            uint32_t t0 = HAL_GetTick();
	            for (int i = 0; i < 4; i++) Adafruit_ILI9340_fillScreen((i & 1) ? 0x001F : 0x0000);
	            uint32_t t1 = HAL_GetTick();
	            Adafruit_ILI9340_fillScreen(0x0000);
	            uint32_t t2 = HAL_GetTick();
	            updateDisplay(1);
	            uint32_t t3 = HAL_GetTick();
	            extern volatile uint32_t g_lcdDmaErr;
	            usb_printf("disp: fill4=%lu ms (%lu ms/frame) redraw=%lu ms dmaerr=%lu\r\n",
	                       (unsigned long)(t1 - t0), (unsigned long)((t1 - t0) / 4), (unsigned long)(t3 - t2),
	                       (unsigned long)g_lcdDmaErr); } break;
	case 'N': {   /* GPS: N = report; Np0 / Np1 = power pin level; Nr = reset pulse; Nb<baud> = serial speed */
	            extern void gps_report(void); extern void gps_power(int level); extern void gps_reset_pulse(void);
	            extern void gps_set_baud(uint32_t baud);
	            if (s[1] == 'p' && (s[2] == '0' || s[2] == '1')) gps_power(s[2] - '0');
	            else if (s[1] == 'r') gps_reset_pulse();
	            else if (s[1] == 'b') { long b = strtol(s + 2, 0, 10); if (b >= 1200 && b <= 230400) gps_set_baud((uint32_t)b); }
	            gps_report(); } break;
	case 'k': {   /* LPF corner: ka = auto, k0..k8 = 5/7/10/14/20/28/40/50/72 MHz, k9 = filter off; k alone reports */
	            extern int g_lpfCorner, g_lpfCoarseNow, g_lpfOnTx; extern void lpfCornerSet(int c);
	            if (s[1] == 'a') lpfCornerSet(-1);
	            else if (s[1] >= '0' && s[1] <= '9') lpfCornerSet(s[1] - '0');
	            usb_printf("lpfcorner=%d coarse=%d ontx=%d\r\n", g_lpfCorner, g_lpfCoarseNow, g_lpfOnTx); } break;
	case 'n': {   /* route transmit through the HMC1023 too: n1 / n0 */
	            extern int g_lpfOnTx; extern void lpfcorner_save(void);
	            extern int txPowerCalDirty, g_txPower; extern void txPowerSet(int idx);
	            if (s[1] == '0' || s[1] == '1') { g_lpfOnTx = s[1] - '0'; lpfcorner_save(); txPowerSet(g_txPower); txPowerCalDirty = 1; }
	            usb_printf("lpfontx=%d\r\n", g_lpfOnTx); } break;
	case 'l': lpfModeSet(atoi(s + 1));   /* LPF mode: l0 = bypass, l1 = 0 dB, l2 = +20 dB (same as the menu) */
	          usb_printf("lpf=%d\r\n", g_lpfMode); break;
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
	case 'Z': { float gz = (float)atof(s + 1); if (gz < 0.0f) gz = 0.0f; if (gz > 256.0f) gz = 256.0f;
	            usbAudioGain = gz; usb_printf("usbAudioGain=%d/100\r\n", (int)(usbAudioGain * 100.0f)); } break;
	case 'Q': { float gq = (float)atof(s + 1); if (gq < 0.0f) gq = 0.0f; if (gq > 256.0f) gq = 256.0f;
	            usbIqGain = gq; usb_printf("usbIqGain=%d/100\r\n", (int)(usbIqGain * 100.0f)); } break;
	case 'B': { int b = atoi(s + 1); if (b < 0) b = 0; if (b > 2) b = 2; iqCorrEnable = b;
	            usb_printf("iqCorr=%d (%s) c1=%d/1000 c2=%d/1000\r\n", iqCorrEnable,
	                       iqCorrEnable==0?"off":iqCorrEnable==1?"adaptive":"manual",
	                       (int)(iq_c1 * 1000.0f), (int)(iq_c2 * 1000.0f)); } break;
	case 'Y': { iq_c1 = (float)atof(s + 1) / 1000.0f;   /* manual phase/correlation term x1000 */
	            usb_printf("iq_c1=%d/1000\r\n", (int)(iq_c1 * 1000.0f)); } break;
	case 'U': { iq_c2 = (float)atof(s + 1) / 1000.0f;   /* manual amplitude term x1000 */
	            usb_printf("iq_c2=%d/1000\r\n", (int)(iq_c2 * 1000.0f)); } break;
	case 'D': { int m = atoi(s + 1); if (m < 0) m = 0; if (m > 2) m = 2; mode = (uint8_t)m;
	            populateCoeficients(filterUpperLimit - filterLowerLimit, mode, filterLowerLimit);
	            usb_printf("mode=%d (%s)\r\n", mode, mode==0?"LSB":mode==1?"USB":"AM"); } break;
	case 'K': {   /* K <lo> <hi> : DSP filter edges in FFT bins (HPF, LPF); ~93.75 Hz/bin @ 48k */
	            char *p = s + 1; long lo = strtol(p, &p, 10); long hi = strtol(p, &p, 10);
	            if (lo < 0) lo = 0; if (hi > 128) hi = 128; if (hi <= lo) hi = lo + 1;
	            filterLowerLimit = (int16_t)lo; filterUpperLimit = (int16_t)hi;
	            populateCoeficients(filterUpperLimit - filterLowerLimit, mode, filterLowerLimit);
	            usb_printf("filt lo=%d hi=%d (~%ld..%ld Hz)\r\n", filterLowerLimit, filterUpperLimit,
	                       lo * 46L, hi * 46L); } break;
	default:  usb_printf("cmds: s | F<hz> | f | a | A | t | x | b | r<reg><hex> | R<reg> | m | v<g> | D<0-2 mode> | K<lo> <hi> filt\r\n"); break;
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
