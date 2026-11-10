/* usbd_cdc_if.h - CDC ACM interface for the PSDR serial console. */
#ifndef __USBD_CDC_IF_H
#define __USBD_CDC_IF_H

#include "usbd_cdc.h"

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);

/* Implemented by usb_serial.c; called with freshly received bytes. */
void usb_serial_on_rx(uint8_t *buf, uint32_t len);

#endif /* __USBD_CDC_IF_H */
