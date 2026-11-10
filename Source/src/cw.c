/*
 * CW (Morse) keying.
 *
 * Two sources of keying:
 *   - text: cw_send_text() queues a string and sends it at a set speed
 *     (console 'q<wpm> <text>'), handy as a beacon or for bench tests;
 *   - paddles: an iambic keyer (mode A) on the two paddle contacts, enabled
 *     with console 'qk1'. Thumb = dit, index = dah. While the keyer is enabled
 *     the "squeeze both paddles to talk" microphone PTT is disabled.
 *
 * How the signal is made: while keyed, the transmit input is a 700 Hz tone
 * (instead of the microphone) passed through the normal sideband filter, so the
 * radio emits a single carrier 700 Hz from the dial (above it in USB, below in
 * LSB). The key does not switch that tone on and off abruptly: cw_envelope()
 * is called for every audio sample and ramps the level over about 5 ms, which
 * keeps the key clicks narrow.
 *
 * Timing runs from the 1 ms system tick (cw_tick_1ms), so element lengths do not
 * depend on how long the main loop takes to redraw the screen. The main loop
 * only switches the radio between receive and transmit: it transmits while
 * cw_tx_request() is true, and the engine waits for cw_tx_ready() before the
 * first element so nothing is clipped. After the last element the transmitter
 * stays on for CW_HANG_MS (semi break-in).
 *
 * Not done yet: sidetone (the DACs are busy driving the transmitter, so it needs
 * another path), a CW receive mode with a narrow filter, and decoding.
 */
#include <string.h>
#include <stdint.h>
#include "stm32f7xx_hal.h"

#define CW_HANG_MS   600
#define CW_TEXT_MAX  96
#define CW_TONE_HZ   700

extern int transmitting;

volatile int g_cwKeyerOn = 0;          /* paddles act as an iambic keyer */
volatile int g_cwWpm = 18;
volatile int g_cwKeyDown = 0;          /* 1 while an element is being sent */
static volatile int cwTxRequest = 0;   /* ask the main loop to transmit */
static volatile uint32_t cwHang = 0;

/* --- text queue --- */
static char cwText[CW_TEXT_MAX];
static volatile int cwTextLen = 0, cwTextPos = 0;
static const char *cwPattern = 0;      /* remaining dots/dashes of the current character */

/* --- element state machine --- */
enum { CW_IDLE, CW_WAIT_TX, CW_MARK, CW_SPACE };
static volatile int cwState = CW_IDLE;
static volatile uint32_t cwTimer = 0;  /* ms left in the current mark or space */
static int cwPendingMark = 0;          /* length in dit units of the mark waiting for the transmitter */
static int cwLastWasDit = 0, cwDitMem = 0, cwDahMem = 0;

static const char *cw_code(char c)
{
	static const char *letters[26] = {
		".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--",
		"-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--.." };
	static const char *digits[10] = {
		"-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----." };
	if(c >= 'a' && c <= 'z') c = (char)(c - 32);
	if(c >= 'A' && c <= 'Z') return letters[c - 'A'];
	if(c >= '0' && c <= '9') return digits[c - '0'];
	switch(c)
	{
	case '/': return "-..-.";
	case '?': return "..--..";
	case '.': return ".-.-.-";
	case ',': return "--..--";
	case '=': return "-...-";
	case '+': return ".-.-.";
	case '-': return "-....-";
	}
	return 0;
}

static uint32_t cw_dit_ms(void)
{
	int w = g_cwWpm;
	if(w < 5) w = 5;
	if(w > 40) w = 40;
	return (uint32_t)(1200 / w);       /* PARIS standard: dit = 1200 / wpm milliseconds */
}

void cw_send_text(int wpm, const char *text)
{
	if(wpm >= 5 && wpm <= 40) g_cwWpm = wpm;
	int n = (int)strlen(text);
	if(n > CW_TEXT_MAX - 1) n = CW_TEXT_MAX - 1;
	__disable_irq();
	memcpy(cwText, text, (size_t)n);
	cwText[n] = 0;
	cwTextLen = n;
	cwTextPos = 0;
	cwPattern = 0;
	__enable_irq();
}

void cw_stop(void)
{
	__disable_irq();
	cwTextLen = 0; cwTextPos = 0; cwPattern = 0;
	cwState = CW_IDLE; g_cwKeyDown = 0; cwHang = 0; cwTxRequest = 0;
	__enable_irq();
}

int cw_tx_request(void) { return cwTxRequest; }
int cw_busy(void)       { return cwState != CW_IDLE || cwTextPos < cwTextLen || cwPattern; }

/* start a mark of `units` dits; if the transmitter is not up yet, wait for it */
static void cw_start_mark(int units)
{
	cwTxRequest = 1;
	cwHang = CW_HANG_MS;
	if(!transmitting) { cwPendingMark = units; cwState = CW_WAIT_TX; cwTimer = 0; return; }
	cwTimer = (uint32_t)units * cw_dit_ms();
	cwState = CW_MARK;
	g_cwKeyDown = 1;
}

/* pick what comes next when the engine is idle or a space has just ended */
static void cw_next(void)
{
	/* 1) paddles */
	if(g_cwKeyerOn)
	{
		int dit = cwDitMem || !HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_3);   /* PADDLE_THUMB_NO = PD3, low = pressed */
		int dah = cwDahMem || !HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_7);   /* PADDLE_INDEX_NO = PD7 */
		cwDitMem = cwDahMem = 0;
		if(dit && dah) { cwLastWasDit = !cwLastWasDit; cw_start_mark(cwLastWasDit ? 1 : 3); return; }   /* squeeze: alternate */
		if(dit) { cwLastWasDit = 1; cw_start_mark(1); return; }
		if(dah) { cwLastWasDit = 0; cw_start_mark(3); return; }
	}
	/* 2) queued text */
	if(cwPattern && *cwPattern)
	{
		cw_start_mark(*cwPattern == '-' ? 3 : 1);
		cwPattern++;
		return;
	}
	if(cwPattern)                       /* character finished: 3 dits between letters (1 already sent) */
	{
		cwPattern = 0;
		cwTimer = 2 * cw_dit_ms();
		cwState = CW_SPACE;
		return;
	}
	if(cwTextPos < cwTextLen)
	{
		char c = cwText[cwTextPos++];
		if(c == ' ') { cwTimer = 4 * cw_dit_ms(); cwState = CW_SPACE; cwHang = CW_HANG_MS + cwTimer; return; }   /* 7 dits between words in total */
		cwPattern = cw_code(c);
		if(cwPattern) { cw_start_mark(*cwPattern == '-' ? 3 : 1); cwPattern++; }
		return;
	}
	cwState = CW_IDLE;
}

/* called every millisecond from the system tick */
void cw_tick_1ms(void)
{
	/* remember a paddle tapped during the opposite element (iambic memory) */
	if(g_cwKeyerOn && cwState == CW_MARK)
	{
		if(!cwLastWasDit && !HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_3)) cwDitMem = 1;
		if( cwLastWasDit && !HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_7)) cwDahMem = 1;
	}

	switch(cwState)
	{
	case CW_WAIT_TX:
		if(transmitting && ++cwTimer >= 15)        /* transmitter up and settled for 15 ms */
		{
			cwTimer = (uint32_t)cwPendingMark * cw_dit_ms();
			cwState = CW_MARK;
			g_cwKeyDown = 1;
		}
		break;
	case CW_MARK:
		cwHang = CW_HANG_MS;
		if(cwTimer) cwTimer--;
		if(!cwTimer) { g_cwKeyDown = 0; cwTimer = cw_dit_ms(); cwState = CW_SPACE; }   /* 1 dit between elements */
		break;
	case CW_SPACE:
		if(cwTimer) cwTimer--;
		if(!cwTimer) cw_next();
		break;
	default:
		cw_next();
		break;
	}

	if(cwState == CW_IDLE || cwState == CW_SPACE)
	{
		if(cwHang) cwHang--;
		if(!cwHang && cwState == CW_IDLE) cwTxRequest = 0;
	}
}

/* called for every audio sample (48 kHz) while transmitting CW: smoothed key level 0..1 */
float cw_envelope(void)
{
	static float env = 0.0f;
	env += 0.006f * ((g_cwKeyDown ? 1.0f : 0.0f) - env);   /* ~3.5 ms time constant, about 5 ms rise */
	return env;
}

int cw_tone_hz(void) { return CW_TONE_HZ; }
