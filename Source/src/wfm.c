/*
 * Wideband FM (broadcast) receive.
 *
 * The normal signal path averages every 15 ADC samples down to 48 kHz, which is
 * far too narrow for a broadcast station (about +/-75 kHz of deviation). The
 * ADCs themselves run at 720 kHz, so this mode works on those raw samples
 * before the averaging:
 *
 *   720 kHz I/Q  -> average 3 at a time -> 240 kHz I/Q        (5 per audio sample)
 *   each new I/Q sample is multiplied by the conjugate of the previous one; the
 *   angle of that product is how far the signal's phase moved in 1/240000 s,
 *   i.e. its instantaneous frequency. That is the demodulated audio.
 *   -> average the 5 values -> one 48 kHz audio sample
 *   -> DC block (removes the offset from being slightly off-tune)
 *   -> 75 microsecond de-emphasis (the North American broadcast standard)
 *
 * Mono only. The result is handed to the same output path AM uses (speaker and
 * the USB "PSDR Demod" sound card).
 *
 * Measured with a signal generator (2026-11-10, modified 2.6.1 unit, 100 MHz):
 * the detector reads a carrier's offset to within 0.3 kHz. The analog baseband
 * on this board has a low-pass near 8 kHz (signal 10 dB down at 25 kHz, 19 dB
 * at 50 kHz, 25 dB at 75 kHz), far narrower than a broadcast channel. An FM
 * detector ignores amplitude, so that only costs signal-to-noise at the edges
 * of the deviation: at -40 dBm the carrier is tracked correctly out to +/-88 kHz
 * (full broadcast deviation); at -60 dBm only to about +/-55 kHz, so weak
 * stations will distort on loud passages. Widening that analog filter is a
 * hardware change. Also, the only digital filtering ahead of the detector is
 * the 3-sample average, so a strong station 200 kHz away is only partly
 * rejected. Not yet listened to on a real antenna.
 */
#include <stdint.h>
#include <math.h>

#define WFM_FS        240000.0f      /* I/Q rate at the detector */
#define WFM_DEV_HZ    75000.0f       /* full-scale deviation */
#define WFM_AUDIO_FS  0.08f          /* audio level at full deviation, same scale as the AM path */

extern int dcOffset2, dcOffset3;           /* static ADC offsets (I = ADC2, Q = ADC3), in counts */
extern float rxDcI, rxDcQ;                 /* slow DC trackers from the main receive path, fraction of full scale */

volatile float g_wfmAudio = 0.0f;          /* latest audio sample */
volatile float g_wfmDiscHz = 0.0f;         /* smoothed discriminator output: where the carrier sits, Hz from the dial */
volatile float g_wfmPower = 0.0f;          /* smoothed signal power, (fraction of ADC full scale)^2 */

static float wfmPrevI = 0.0f, wfmPrevQ = 1.0f;
static float wfmDc = 0.0f, wfmDeemph = 0.0f;

/* atan2 good to about 0.005 rad: plenty for FM audio, and far cheaper than atan2f */
static inline float wfm_atan2(float y, float x)
{
	float ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
	float a;
	if(ax >= ay)
	{
		if(ax == 0.0f) return 0.0f;
		float t = ay / ax;
		a = t * (0.78539816f + 0.273f * (1.0f - t));
	}
	else
	{
		float t = ax / ay;
		a = 1.57079633f - t * (0.78539816f + 0.273f * (1.0f - t));
	}
	if(x < 0) a = 3.14159265f - a;
	return y < 0 ? -a : a;
}

/* Called from the ADC DMA interrupt, once per 48 kHz audio sample, with the n
   (= 15) raw I and Q samples that just arrived. */
void wfm_block(const volatile uint16_t *ibuf, const volatile uint16_t *qbuf, int n)
{
	float offI = (float)dcOffset2 + rxDcI * 4096.0f;
	float offQ = (float)dcOffset3 + rxDcQ * 4096.0f;
	float sum = 0.0f, pw = 0.0f;
	int groups = n / 3;
	for(int g = 0; g < groups; g++)
	{
		const volatile uint16_t *pi = ibuf + g * 3, *pq = qbuf + g * 3;
		float i = ((float)(pi[0] + pi[1] + pi[2])) * (1.0f / 3.0f) - offI;
		float q = ((float)(pq[0] + pq[1] + pq[2])) * (1.0f / 3.0f) - offQ;
		/* z[n] * conj(z[n-1]) */
		float re = i * wfmPrevI + q * wfmPrevQ;
		float im = i * wfmPrevQ - q * wfmPrevI;   /* sign chosen so a carrier above the dial reads positive (measured) */
		sum += wfm_atan2(im, re);
		pw += i * i + q * q;
		wfmPrevI = i; wfmPrevQ = q;
	}
	float hz = (sum / (float)groups) * (WFM_FS / 6.2831853f);     /* radians per sample -> Hz */
	g_wfmDiscHz += 0.002f * (hz - g_wfmDiscHz);                   /* ~10 ms smoothing, for the console and tuning aids */
	pw = pw / (float)groups / (4096.0f * 4096.0f);
	g_wfmPower += 0.0005f * (pw - g_wfmPower);

	float x = hz / WFM_DEV_HZ;
	wfmDc += 0.002f * (x - wfmDc);                                 /* ~15 Hz high-pass: removes the off-tune offset */
	x -= wfmDc;
	wfmDeemph += 0.2425f * (x - wfmDeemph);                        /* 75 us de-emphasis at 48 kHz */
	float a = wfmDeemph * WFM_AUDIO_FS;
	if(a > 0.10f) a = 0.10f; else if(a < -0.10f) a = -0.10f;
	g_wfmAudio = a;
}
