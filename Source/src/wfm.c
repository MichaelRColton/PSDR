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
 * at 50 kHz, 25 dB at 75 kHz), far narrower than a broadcast channel. That
 * distorts the audio badly unless it is undone first (see the equaliser below),
 * and it costs signal-to-noise at the edges of the deviation: at -40 dBm the carrier is tracked correctly out to +/-88 kHz
 * (full broadcast deviation); at -60 dBm only to about +/-55 kHz, so weak
 * stations will distort on loud passages. Widening that analog filter is a
 * hardware change. Also, the only digital filtering ahead of the detector is
 * the 3-sample average, so a strong station 200 kHz away is only partly
 * rejected.
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
volatile float g_wfmNoise = 0.0f;          /* smoothed detector noise above the programme (see below), kHz rms-ish */

volatile float g_wfmRipple = 0.0f;         /* how much the signal's power wobbles, 0 = perfectly constant (see the equaliser) */

/* Equaliser for the analog baseband low-pass.
   The board's baseband amplifiers roll off from about 8 kHz, and a broadcast
   signal swings +/-75 kHz. An FM detector does not care about the amplitude
   change that causes, but it does care about the phase shift that comes with
   it: the filter delays the signal by a different amount depending on where it
   is in its swing, and that is heard as heavy distortion (in simulation about
   7 dB signal-to-distortion, however strong the station). Running the samples
   through the inverse of that filter first puts it right (37 dB or better in
   the same simulation). The inverse of a one-pole low-pass is
       y[n] = (x[n] - (1-a) x[n-1]) / a,   a = 1 - exp(-2 pi fc / fs)
   Two of them in a row, so a second, higher pole can be undone too.
   The price is that noise far from the carrier is boosted along with the
   signal, so very weak stations get no better.
   A clean FM signal has constant amplitude, which gives a way to set the
   corner without test equipment: g_wfmRipple is the relative wobble in power
   after the equaliser, and the right corner is the one that makes it smallest
   on a strong station (console: j<corner Hz> [second corner Hz], P shows it). */
volatile float g_wfmEqHz[2] = { 8000.0f, 0.0f };   /* 0 = that stage off */
static float wfmEqA[2] = { 0.0f, 0.0f }, wfmEqFor[2] = { -1.0f, -1.0f };
static float wfmEqI[2], wfmEqQ[2];
static float wfmEp = 0.0f, wfmEp2 = 0.0f;

static float wfmPrevI = 0.0f, wfmPrevQ = 1.0f;
static float wfmDc = 0.0f, wfmDeemph = 0.0f;
static float wfmD1 = 0.0f, wfmD2 = 0.0f;   /* previous detector outputs, for the noise measure */

/* Audio low-pass at the detector rate, before dropping to 48 kHz.
   A broadcast FM signal is more than the audio: above 15 kHz the detector
   output also carries the 19 kHz stereo pilot, the stereo difference signal
   around 38 kHz and RDS at 57 kHz. Averaging five samples to get to 48 kHz
   (the first version) let the 23-53 kHz stereo band fold straight back into
   the audio. Two biquads (4th-order Butterworth, 15 kHz corner) at 240 kHz
   remove it first. */
typedef struct { float b0, b1, b2, a1, a2, s1, s2; } wfm_bq_t;
static wfm_bq_t wfmLp[2];
static int wfmLpReady = 0;

static void wfm_lp_design(void)
{
	static const float q[2] = { 0.54119610f, 1.30656296f };
	float w0 = 6.2831853f * 15000.0f / WFM_FS;
	float cs = cosf(w0), sn = sinf(w0);
	for(int k = 0; k < 2; k++)
	{
		float alpha = sn / (2.0f * q[k]), a0 = 1.0f + alpha;
		wfmLp[k].b0 = (1.0f - cs) * 0.5f / a0;
		wfmLp[k].b1 = (1.0f - cs) / a0;
		wfmLp[k].b2 = wfmLp[k].b0;
		wfmLp[k].a1 = -2.0f * cs / a0;
		wfmLp[k].a2 = (1.0f - alpha) / a0;
		wfmLp[k].s1 = wfmLp[k].s2 = 0.0f;
	}
	wfmLpReady = 1;
}

static inline float wfm_lp_run(float x)
{
	for(int k = 0; k < 2; k++)
	{
		wfm_bq_t *f = &wfmLp[k];
		float y = f->b0 * x + f->s1;
		f->s1 = f->b1 * x - f->a1 * y + f->s2;
		f->s2 = f->b2 * x - f->a2 * y;
		x = y;
	}
	return x;
}

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
	if(!wfmLpReady) wfm_lp_design();
	float offI = (float)dcOffset2 + rxDcI * 4096.0f;
	float offQ = (float)dcOffset3 + rxDcQ * 4096.0f;
	float sum = 0.0f, pw = 0.0f, nz = 0.0f, lp = 0.0f, ep = 0.0f, ep2 = 0.0f;
	for(int e = 0; e < 2; e++)
		if(wfmEqFor[e] != g_wfmEqHz[e])
		{
			wfmEqFor[e] = g_wfmEqHz[e];
			wfmEqA[e] = wfmEqFor[e] > 0.0f ? 1.0f - expf(-6.2831853f * wfmEqFor[e] / WFM_FS) : 0.0f;
		}
	int groups = n / 3;
	for(int g = 0; g < groups; g++)
	{
		const volatile uint16_t *pi = ibuf + g * 3, *pq = qbuf + g * 3;
		float i = ((float)(pi[0] + pi[1] + pi[2])) * (1.0f / 3.0f) - offI;
		float q = ((float)(pq[0] + pq[1] + pq[2])) * (1.0f / 3.0f) - offQ;
		pw += i * i + q * q;                       /* level at the ADC, before the equaliser (for the gain control) */
		for(int e = 0; e < 2; e++)
			if(wfmEqA[e] > 0.0f)
			{
				float b = 1.0f - wfmEqA[e], g = 1.0f / wfmEqA[e];
				float yi = (i - b * wfmEqI[e]) * g, yq = (q - b * wfmEqQ[e]) * g;
				wfmEqI[e] = i; wfmEqQ[e] = q;
				i = yi; q = yq;
			}
		{ float pe = i * i + q * q; ep += pe; ep2 += pe * pe; }
		/* z[n] * conj(z[n-1]): its angle is the phase step, i.e. the instantaneous frequency */
		float re = i * wfmPrevI + q * wfmPrevQ;
		float im = i * wfmPrevQ - q * wfmPrevI;   /* sign chosen so a carrier above the dial reads positive (measured) */
		float d = wfm_atan2(im, re);
		sum += d;
		lp = wfm_lp_run(d);                        /* audio band only; the last of the 5 values is the 48 kHz sample */
		/* noise measure: second difference of the detector output, which weights the
		   top of the 0-120 kHz range (above everything a station transmits) by more
		   than 10 dB over the stereo band. Low = clean signal, high = noise. */
		float dd = d - 2.0f * wfmD1 + wfmD2;
		nz += dd * dd;
		wfmD2 = wfmD1; wfmD1 = d;
		wfmPrevI = i; wfmPrevQ = q;
	}
	float k = WFM_FS / 6.2831853f;                                 /* radians per sample -> Hz */
	g_wfmDiscHz += 0.002f * ((sum / (float)groups) * k - g_wfmDiscHz);
	g_wfmNoise += 0.0005f * (sqrtf(nz / (float)groups) * k * 0.001f - g_wfmNoise);
	pw = pw / (float)groups / (4096.0f * 4096.0f);
	g_wfmPower += 0.0005f * (pw - g_wfmPower);
	wfmEp += 0.0005f * (ep / (float)groups - wfmEp);
	wfmEp2 += 0.0005f * (ep2 / (float)groups - wfmEp2);
	{ float var = wfmEp2 - wfmEp * wfmEp; g_wfmRipple = (var > 0.0f && wfmEp > 0.0f) ? sqrtf(var) / wfmEp : 0.0f; }

	float x = lp * k / WFM_DEV_HZ;
	wfmDc += 0.002f * (x - wfmDc);                                 /* ~15 Hz high-pass: removes the off-tune offset */
	x -= wfmDc;
	wfmDeemph += 0.2425f * (x - wfmDeemph);                        /* 75 us de-emphasis at 48 kHz */
	float a = wfmDeemph * WFM_AUDIO_FS;
	if(a > 0.10f) a = 0.10f; else if(a < -0.10f) a = -0.10f;
	g_wfmAudio = a;
}
