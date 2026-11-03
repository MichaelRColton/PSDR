/* usbd_cdc_audio.h - Composite USB device: CDC ACM console + UAC1 microphone.
 *
 * Hand-rolled single USBD class that presents two IAD-grouped functions:
 *   - CDC ACM  (interfaces 0/1, EP 0x81 bulk IN, 0x01 bulk OUT, 0x82 intr IN) -> the serial console
 *   - UAC1 mic (interfaces 2/3, EP 0x83 iso IN)                               -> demod audio to the PC
 *
 * Replaces the plain USBD_CDC registration. The old usbd_cdc*.c stay compiled but
 * unused, so reverting is just swapping the class back in usb_serial_init().
 *
 * STATUS 2026-11-03: ENUMERATES on Windows (CDC COM port + UAC mic both OK).
 *   Root cause of the earlier code-10 failure, found via an elevated USB ETW
 *   trace: once interface 3 had a second alt setting, usbccgp issued
 *   SET_INTERFACE(iface 3, alt 0), and the USB-device CORE STALLed it because
 *   USBD_MAX_NUM_INTERFACES was 1 (usbd_ctlreq.c gates interface requests with
 *   wIndex <= USBD_MAX_NUM_INTERFACES, so requests to interfaces 2/3 never even
 *   reached this class). The stall triggered a re-enumerate loop -> code 10.
 *   Fix: USBD_MAX_NUM_INTERFACES = 4 (usbd_conf.h). The device-qualifier stall
 *   seen in the trace is normal for a full-speed-only device and is harmless.
 *   Streaming now runs at real-time 48 kHz: the next frame is chained off the
 *   DataIn completion (one clean packet/frame), SOF only re-kicks if the chain
 *   stalls, and IsoINIncomplete flushes the EP so a missed frame doesn't back up
 *   the FIFO. Verified by host capture (~real-time) and the g_usbdbg counters
 *   (armed ~= completed). Those counters are a left-in SWD side channel.
 *   OPEN: wire a real sample source (demod audio, or stereo I/Q) in place of the
 *   weak test-tone usb_audio_get_samples(); give the AS interface its own name.
 */
#ifndef __USBD_CDC_AUDIO_H
#define __USBD_CDC_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_ioreq.h"

extern USBD_ClassTypeDef USBD_CDC_AUDIO;

/* CDC transmit used by the console (usb_printf). Returns USBD_OK/BUSY/FAIL. */
uint8_t usb_cdc_tx(uint8_t *buf, uint16_t len);

/* Audio sample source. The composite calls this once per USB frame (1 ms) to get
 * AUDIO_SAMPLES_PER_FRAME mono int16 samples for the mic stream. A weak default
 * emits a quiet test tone; the receiver DSP can override it with a strong symbol. */
#define AUDIO_SAMPLE_RATE            24000U                       /* matches the DSP Fs (TIM3) */
#define AUDIO_SAMPLES_PER_FRAME      (AUDIO_SAMPLE_RATE / 1000U)  /* 24 nominal per 1 ms frame  */
#define AUDIO_MAX_SAMPLES_PER_FRAME  32U                          /* headroom for async jitter  */

/* Fill up to `max` mono int16 samples from the audio source and return the
 * count actually written (0..max). The composite calls this once per frame to
 * pace the mic. A weak default emits a quiet test tone; the DSP overrides it. */
uint32_t usb_audio_get_samples(int16_t *dst, uint32_t max);

/* Producer: push one mono demod sample into the USB audio FIFO. Called from the
 * DSP sample ISR; lock-free single-producer / single-consumer with the USB ISR. */
void usb_audio_push(int16_t sample);

/* True once the host has opened the mic (AS alt-setting 1). */
int usb_audio_streaming(void);

#ifdef __cplusplus
}
#endif
#endif /* __USBD_CDC_AUDIO_H */
