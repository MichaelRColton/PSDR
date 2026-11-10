/* usb_serial.h - USB CDC serial console for PSDR bench bring-up. */
#ifndef __USB_SERIAL_H
#define __USB_SERIAL_H

void usb_serial_init(void);   /* bring up the CDC device (call once at startup) */
void usb_serial_poll(void);   /* process queued commands (call from the main loop) */
void usb_printf(const char *fmt, ...);

#endif /* __USB_SERIAL_H */
