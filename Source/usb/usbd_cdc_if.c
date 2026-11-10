/* usbd_cdc_if.c - CDC ACM interface callbacks for the PSDR serial console. */
#include "usbd_cdc_if.h"
#include "usbd_core.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

#define APP_RX_DATA_SIZE  64
#define APP_TX_DATA_SIZE  256

static uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
static uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

/* 115200 8N1, in the CDC line-coding wire format. */
static uint8_t LineCoding[7] = { 0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08 };

static int8_t CDC_Init_FS(void)
{
	USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
	USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
	return (int8_t)USBD_OK;
}

static int8_t CDC_DeInit_FS(void) { return (int8_t)USBD_OK; }

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
	(void)length;
	switch (cmd)
	{
		case CDC_SET_LINE_CODING: for (int i = 0; i < 7; i++) LineCoding[i] = pbuf[i]; break;
		case CDC_GET_LINE_CODING: for (int i = 0; i < 7; i++) pbuf[i] = LineCoding[i]; break;
		default: break;
	}
	return (int8_t)USBD_OK;
}

static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
	usb_serial_on_rx(Buf, *Len);
	USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
	USBD_CDC_ReceivePacket(&hUsbDeviceFS);
	return (int8_t)USBD_OK;
}

static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{ (void)Buf; (void)Len; (void)epnum; return (int8_t)USBD_OK; }

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
	CDC_Init_FS, CDC_DeInit_FS, CDC_Control_FS, CDC_Receive_FS, CDC_TransmitCplt_FS
};

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
	USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
	if (hcdc == NULL) return (uint8_t)USBD_FAIL;
	if (hcdc->TxState != 0) return (uint8_t)USBD_BUSY;
	USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
	return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}
