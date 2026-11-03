/* usbd_desc.c - USB device + string descriptors for a CDC ACM (virtual COM). */
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_conf.h"

#define USBD_VID            0x0483
#define USBD_PID            0x5740
#define USBD_LANGID         0x0409
#define USBD_MANUF_STR      "PortableSDR"
#define USBD_PRODUCT_STR    "PSDR Serial Console"
#define USBD_SERIAL_STR     "PSDR-0001"
#define USBD_CONFIG_STR     "PSDR CDC Config"
#define USBD_IFACE_STR      "PSDR CDC Interface"

static void Get_SerialNum(void);

/* 18-byte device descriptor */
__ALIGN_BEGIN static uint8_t USBD_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END = {
	0x12,                       /* bLength */
	USB_DESC_TYPE_DEVICE,       /* bDescriptorType */
	0x00, 0x02,                 /* bcdUSB 2.00 */
	0xEF,                       /* bDeviceClass: Misc (IAD composite) */
	0x02,                       /* bDeviceSubClass: Common Class */
	0x01,                       /* bDeviceProtocol: Interface Association Descriptor */
	USB_MAX_EP0_SIZE,           /* bMaxPacketSize0 */
	LOBYTE(USBD_VID), HIBYTE(USBD_VID),
	LOBYTE(USBD_PID), HIBYTE(USBD_PID),
	0x03, 0x02,                 /* bcdDevice 2.03 (bumped so Windows re-reads fresh strings) */
	USBD_IDX_MFC_STR,           /* iManufacturer */
	USBD_IDX_PRODUCT_STR,       /* iProduct */
	USBD_IDX_SERIAL_STR,        /* iSerialNumber */
	USBD_MAX_NUM_CONFIGURATION  /* bNumConfigurations */
};

/* LangID string */
__ALIGN_BEGIN static uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END = {
	USB_LEN_LANGID_STR_DESC, USB_DESC_TYPE_STRING, LOBYTE(USBD_LANGID), HIBYTE(USBD_LANGID)
};

__ALIGN_BEGIN static uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;

static uint8_t *Get_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; *length = sizeof(USBD_DeviceDesc); return USBD_DeviceDesc; }

static uint8_t *Get_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; *length = sizeof(USBD_LangIDDesc); return USBD_LangIDDesc; }

static uint8_t *Get_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; USBD_GetString((uint8_t *)USBD_PRODUCT_STR, USBD_StrDesc, length); return USBD_StrDesc; }

static uint8_t *Get_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; USBD_GetString((uint8_t *)USBD_MANUF_STR, USBD_StrDesc, length); return USBD_StrDesc; }

static uint8_t *Get_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; USBD_GetString((uint8_t *)USBD_SERIAL_STR, USBD_StrDesc, length); return USBD_StrDesc; }

static uint8_t *Get_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; USBD_GetString((uint8_t *)USBD_CONFIG_STR, USBD_StrDesc, length); return USBD_StrDesc; }

static uint8_t *Get_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{ (void)speed; USBD_GetString((uint8_t *)USBD_IFACE_STR, USBD_StrDesc, length); return USBD_StrDesc; }

static void Get_SerialNum(void) { }

USBD_DescriptorsTypeDef CDC_Desc = {
	Get_DeviceDescriptor,
	Get_LangIDStrDescriptor,
	Get_ManufacturerStrDescriptor,
	Get_ProductStrDescriptor,
	Get_SerialStrDescriptor,
	Get_ConfigStrDescriptor,
	Get_InterfaceStrDescriptor,
};
