/* usbd_conf.h - USB Device library config for the PSDR CDC serial console. */
#ifndef __USBD_CONF_H
#define __USBD_CONF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stm32f7xx_hal.h"

/* This 2016-era CMSIS doesn't define these; the USB-Device library needs them. */
#ifndef __PACKED
#define __PACKED        __attribute__((packed))
#endif
#ifndef __ALIGN_BEGIN
#define __ALIGN_BEGIN
#endif
#ifndef __ALIGN_END
#define __ALIGN_END     __attribute__((aligned(4)))
#endif

/* Composite CDC + UAC: interfaces 0-3. This gates USBD_Std{Itf,EP}Req: the core
   STALLs any standard/class interface request whose wIndex exceeds it (see
   usbd_ctlreq.c). At 1, SET_INTERFACE/GET_INTERFACE to the audio interfaces (2,3)
   stalled, so Windows' usbccgp failed to start the device (code 10) the moment the
   audio streaming interface had a second alt setting. Must cover the highest iface #. */
#define USBD_MAX_NUM_INTERFACES        4U
#define USBD_MAX_NUM_CONFIGURATION     1U
#define USBD_MAX_STR_DESC_SIZ          0x100U
#define USBD_SELF_POWERED              1U
#define USBD_DEBUG_LEVEL               0U
#define USBD_CDC_INTERVAL              2000U
#define DEVICE_FS                      0

/* Static (no heap) memory management. */
#define USBD_malloc                    (void *)USBD_static_malloc
#define USBD_free                      USBD_static_free
#define USBD_memset                    memset
#define USBD_memcpy                    memcpy
#define USBD_Delay                     HAL_Delay

/* Logging off (no printf dependency). */
#define USBD_UsrLog(...)               do {} while (0)
#define USBD_ErrLog(...)               do {} while (0)
#define USBD_DbgLog(...)               do {} while (0)

void *USBD_static_malloc(uint32_t size);
void  USBD_static_free(void *p);

#endif /* __USBD_CONF_H */
