/* usbd_cdc_audio.c - Composite CDC ACM console + UAC1 microphone (see header). */
#include "usbd_cdc_audio.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"

/* ---- endpoints ---------------------------------------------------------- */
#define CDC_IN_EP        0x81U   /* bulk  IN  : console -> host              */
#define CDC_OUT_EP       0x01U   /* bulk  OUT : host -> console              */
#define CDC_CMD_EP       0x82U   /* intr  IN  : CDC notifications (unused)   */
#define AUDIO_IN_EP      0x83U   /* iso   IN  : mic samples -> host          */

#define CDC_MPS          64U
#define CDC_CMD_MPS      8U
#define AUDIO_PKT        (AUDIO_SAMPLES_PER_FRAME * 2U)   /* 96 bytes mono 16-bit */

#define USB_CDC_AUDIO_CONFIG_DESC_SIZ   174U

/* CDC class requests we care about */
#define CDC_SET_LINE_CODING        0x20U
#define CDC_GET_LINE_CODING        0x21U
#define CDC_SET_CONTROL_LINE_STATE 0x22U
/* Audio class requests */
#define AUDIO_REQ_GET_CUR          0x81U
#define AUDIO_REQ_SET_CUR          0x01U

extern USBD_HandleTypeDef hUsbDeviceFS;
void usb_serial_on_rx(uint8_t *buf, uint32_t len);   /* console RX sink (usb_serial.c) */

typedef struct {
    uint8_t  cdc_rx[CDC_MPS];
    uint8_t  cdc_tx_state;           /* 1 = a bulk-IN transfer is in flight   */
    uint8_t  line_coding[7];
    uint8_t  cmd;                    /* pending EP0 class request opcode       */
    uint8_t  as_alt;                 /* audio-streaming alt setting (0/1)      */
    uint8_t  ep0_buf[8];
    int16_t  audio_buf[AUDIO_SAMPLES_PER_FRAME];
} CDC_AUDIO_Handle;

static CDC_AUDIO_Handle hcls;        /* single static instance (one device)    */

/* ---- SWD-readable enumeration trace (temporary diagnostics) ------------- *
 * USB is the thing under test, so the console can't report. Read this struct
 * over SWD after plugging in:  STM32_Programmer_CLI -c port=SWD mode=HOTPLUG
 *                              -r32 <&g_usbdbg from the .map> 0x40            */
typedef struct {
    uint32_t magic;            /* 0x0DB00001 marker so we know we read the right addr */
    uint32_t cfg_reads;        /* GetFSCfgDesc calls (host read the config descriptor) */
    uint32_t init_calls;       /* Init() = SET_CONFIGURATION accepted                  */
    uint32_t deinit_calls;
    uint32_t setup_std_if;     /* standard interface requests seen in Setup()          */
    uint32_t setup_class_if;   /* class interface requests seen in Setup()             */
    uint32_t getif;            /* GET_INTERFACE count                                  */
    uint32_t last_getif_iface;
    uint32_t setif;            /* SET_INTERFACE count                                  */
    uint32_t last_setif_iface;
    uint32_t last_setif_alt;
    uint32_t audio_alt1_opens; /* audio_set_alt(1) -> iso EP opened                    */
    uint32_t sof_count;
    uint32_t setup_other;      /* requests that hit the default/stall path             */
} usb_dbg_t;
volatile usb_dbg_t g_usbdbg = { .magic = 0x0DB00001U };

/* ---- combined configuration descriptor ---------------------------------- */
__ALIGN_BEGIN static uint8_t CfgDesc[] __ALIGN_END = {
    /* ---- Configuration ---- (wTotalLength / bNumInterfaces fixed below, asserted == sizeof) */
    0x09, USB_DESC_TYPE_CONFIGURATION,
    LOBYTE(USB_CDC_AUDIO_CONFIG_DESC_SIZ), HIBYTE(USB_CDC_AUDIO_CONFIG_DESC_SIZ),
    0x04,                 /* bNumInterfaces: CDC(0,1) + Audio(2,3)            */
    0x01,                 /* bConfigurationValue                              */
    0x00,                 /* iConfiguration                                   */
    0xC0,                 /* bmAttributes: self-powered                       */
    0x32,                 /* bMaxPower: 100 mA                                */

    /* ===== IAD: CDC (interfaces 0-1) ===== */
    0x08, 0x0B, 0x00, 0x02, 0x02, 0x02, 0x01, 0x00,

    /* ---- CDC communication interface (0) ---- */
    0x09, USB_DESC_TYPE_INTERFACE, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
    0x05, 0x24, 0x00, 0x10, 0x01,              /* CDC header, bcdCDC 1.10     */
    0x05, 0x24, 0x01, 0x00, 0x01,              /* call mgmt, data iface = 1   */
    0x04, 0x24, 0x02, 0x02,                    /* ACM, supports line coding   */
    0x05, 0x24, 0x06, 0x00, 0x01,              /* union: master 0, slave 1    */
    0x07, USB_DESC_TYPE_ENDPOINT, CDC_CMD_EP, 0x03,
          LOBYTE(CDC_CMD_MPS), HIBYTE(CDC_CMD_MPS), 0x10,   /* intr IN, 16ms  */

    /* ---- CDC data interface (1) ---- */
    0x09, USB_DESC_TYPE_INTERFACE, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
    0x07, USB_DESC_TYPE_ENDPOINT, CDC_OUT_EP, 0x02, LOBYTE(CDC_MPS), HIBYTE(CDC_MPS), 0x00,
    0x07, USB_DESC_TYPE_ENDPOINT, CDC_IN_EP,  0x02, LOBYTE(CDC_MPS), HIBYTE(CDC_MPS), 0x00,

    /* ===== IAD: Audio (interfaces 2-3) ===== bFunctionClass=AUDIO, subclass=AUDIOCONTROL */
    0x08, 0x0B, 0x02, 0x02, 0x01, 0x01, 0x00, 0x00,

    /* ---- Audio Control interface (2) ---- */
    0x09, USB_DESC_TYPE_INTERFACE, 0x02, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
    0x09, 0x24, 0x01, 0x00, 0x01, 0x1E, 0x00, 0x01, 0x03,  /* AC header, wTotalLen=30, 1 AS iface(=3) */
    0x0C, 0x24, 0x02, 0x01, 0x01, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, /* Input Terminal: ID1, Microphone, 1ch */
    0x09, 0x24, 0x03, 0x02, 0x01, 0x01, 0x00, 0x01, 0x00,  /* Output Terminal: ID2, USB streaming, src=1 */

    /* ---- Audio Streaming interface (3), alt 0 (idle) ---- */
    0x09, USB_DESC_TYPE_INTERFACE, 0x03, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,
    /* ---- Audio Streaming interface (3), alt 1 (active) ---- */
    0x09, USB_DESC_TYPE_INTERFACE, 0x03, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00,
    0x07, 0x24, 0x01, 0x02, 0x01, 0x01, 0x00,              /* AS general: terminal link=2, PCM */
    0x0B, 0x24, 0x02, 0x01, 0x01, 0x02, 0x10, 0x01,        /* Format Type I: 1ch, 2 bytes, 16-bit, 1 freq */
          LOBYTE(AUDIO_SAMPLE_RATE), (uint8_t)((AUDIO_SAMPLE_RATE >> 8) & 0xFF),
          (uint8_t)((AUDIO_SAMPLE_RATE >> 16) & 0xFF),     /* 48000 Hz                      */
    0x09, USB_DESC_TYPE_ENDPOINT, AUDIO_IN_EP, 0x05,       /* iso, async                    */
          LOBYTE(AUDIO_PKT), HIBYTE(AUDIO_PKT), 0x01, 0x00, 0x00,
    0x07, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00,              /* CS audio endpoint             */
};
_Static_assert(sizeof(CfgDesc) == USB_CDC_AUDIO_CONFIG_DESC_SIZ,
               "combined config descriptor byte count != 174");

__ALIGN_BEGIN static uint8_t DevQualDesc[10] __ALIGN_END =
    { 10, 0x06, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00 };

/* ---- audio sample source (weak default = quiet 1 kHz test tone) --------- */
__attribute__((weak)) void usb_audio_get_samples(int16_t *dst, uint32_t n)
{
    static uint32_t ph;
    /* ~1 kHz at 48 kHz: 48 samples/cycle. Low amplitude. */
    for (uint32_t i = 0; i < n; i++) {
        /* crude triangle to avoid a sin table: cheap and recognizable */
        uint32_t p = ph & 0x2FU;                 /* 0..47 */
        int32_t v = (p < 24) ? (int32_t)p - 12 : 36 - (int32_t)p;  /* -12..+12..-11 */
        dst[i] = (int16_t)(v * 200);             /* ~+/-2400 */
        ph++;
    }
}

int usb_audio_streaming(void) { return hcls.as_alt == 1; }

/* ---- class callbacks ---------------------------------------------------- */
static uint8_t Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    g_usbdbg.init_calls++;
    static const uint8_t lc[7] = { 0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08 };
    for (int i = 0; i < 7; i++) hcls.line_coding[i] = lc[i];
    hcls.cdc_tx_state = 0;
    hcls.as_alt = 0;

    USBD_LL_OpenEP(pdev, CDC_IN_EP,  USBD_EP_TYPE_BULK, CDC_MPS);
    USBD_LL_OpenEP(pdev, CDC_OUT_EP, USBD_EP_TYPE_BULK, CDC_MPS);
    USBD_LL_OpenEP(pdev, CDC_CMD_EP, USBD_EP_TYPE_INTR, CDC_CMD_MPS);
    pdev->ep_in[CDC_IN_EP & 0x0FU].is_used = 1U;
    pdev->ep_out[CDC_OUT_EP & 0x0FU].is_used = 1U;
    pdev->ep_in[CDC_CMD_EP & 0x0FU].is_used = 1U;

    pdev->pClassData = &hcls;
    USBD_LL_PrepareReceive(pdev, CDC_OUT_EP, hcls.cdc_rx, CDC_MPS);
    return (uint8_t)USBD_OK;
}

static uint8_t DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    g_usbdbg.deinit_calls++;
    USBD_LL_CloseEP(pdev, CDC_IN_EP);
    USBD_LL_CloseEP(pdev, CDC_OUT_EP);
    USBD_LL_CloseEP(pdev, CDC_CMD_EP);
    if (hcls.as_alt) { USBD_LL_CloseEP(pdev, AUDIO_IN_EP); hcls.as_alt = 0; }
    pdev->pClassData = NULL;
    return (uint8_t)USBD_OK;
}

static void audio_set_alt(USBD_HandleTypeDef *pdev, uint8_t alt)
{
    if (alt == hcls.as_alt) return;
    if (alt == 1U) {
        g_usbdbg.audio_alt1_opens++;
        USBD_LL_OpenEP(pdev, AUDIO_IN_EP, USBD_EP_TYPE_ISOC, AUDIO_PKT);
        pdev->ep_in[AUDIO_IN_EP & 0x0FU].is_used = 1U;
        hcls.as_alt = 1;
        /* prime the first frame */
        usb_audio_get_samples(hcls.audio_buf, AUDIO_SAMPLES_PER_FRAME);
        USBD_LL_Transmit(pdev, AUDIO_IN_EP, (uint8_t *)hcls.audio_buf, AUDIO_PKT);
    } else {
        USBD_LL_CloseEP(pdev, AUDIO_IN_EP);
        pdev->ep_in[AUDIO_IN_EP & 0x0FU].is_used = 0U;
        hcls.as_alt = 0;
    }
}

static uint8_t Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    uint8_t iface = (uint8_t)(req->wIndex & 0xFFU);

    switch (req->bmRequest & USB_REQ_TYPE_MASK) {
    case USB_REQ_TYPE_CLASS:
        g_usbdbg.setup_class_if++;
        if (req->wLength) {
            if (req->bmRequest & 0x80U) {                 /* device -> host */
                if (iface <= 1U) {                        /* CDC */
                    if (req->bRequest == CDC_GET_LINE_CODING)
                        USBD_CtlSendData(pdev, hcls.line_coding, 7U);
                } else {                                  /* Audio GET_CUR */
                    hcls.ep0_buf[0] = LOBYTE(AUDIO_SAMPLE_RATE);
                    hcls.ep0_buf[1] = (uint8_t)((AUDIO_SAMPLE_RATE >> 8) & 0xFF);
                    hcls.ep0_buf[2] = (uint8_t)((AUDIO_SAMPLE_RATE >> 16) & 0xFF);
                    USBD_CtlSendData(pdev, hcls.ep0_buf,
                                     (req->wLength < 3U) ? req->wLength : 3U);
                }
            } else {                                      /* host -> device */
                hcls.cmd = req->bRequest;
                USBD_CtlPrepareRx(pdev, hcls.ep0_buf,
                                  (req->wLength < sizeof(hcls.ep0_buf)) ? req->wLength
                                                                       : sizeof(hcls.ep0_buf));
            }
        } else {
            /* zero-length class request (e.g. SET_CONTROL_LINE_STATE): just ACK */
        }
        break;

    case USB_REQ_TYPE_STANDARD:
        g_usbdbg.setup_std_if++;
        switch (req->bRequest) {
        case USB_REQ_GET_INTERFACE:
            g_usbdbg.getif++; g_usbdbg.last_getif_iface = iface;
            hcls.ep0_buf[0] = (iface == 3U) ? hcls.as_alt : 0U;
            USBD_CtlSendData(pdev, hcls.ep0_buf, 1U);
            break;
        case USB_REQ_SET_INTERFACE:
            g_usbdbg.setif++; g_usbdbg.last_setif_iface = iface;
            g_usbdbg.last_setif_alt = (uint8_t)(req->wValue & 0xFFU);
            if (iface == 3U) audio_set_alt(pdev, (uint8_t)(req->wValue & 0xFFU));
            break;
        default:
            break;
        }
        break;

    default:
        g_usbdbg.setup_other++;
        USBD_CtlError(pdev, req);
        return (uint8_t)USBD_FAIL;
    }
    return (uint8_t)USBD_OK;
}

static uint8_t EP0_RxReady(USBD_HandleTypeDef *pdev)
{
    (void)pdev;
    if (hcls.cmd == CDC_SET_LINE_CODING)
        for (int i = 0; i < 7; i++) hcls.line_coding[i] = hcls.ep0_buf[i];
    hcls.cmd = 0xFFU;
    return (uint8_t)USBD_OK;
}

static uint8_t DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    (void)pdev;
    if (epnum == (CDC_IN_EP & 0x0FU)) hcls.cdc_tx_state = 0;
    return (uint8_t)USBD_OK;
}

static uint8_t DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    if (epnum == (CDC_OUT_EP & 0x0FU)) {
        uint32_t len = USBD_LL_GetRxDataSize(pdev, CDC_OUT_EP);
        usb_serial_on_rx(hcls.cdc_rx, len);
        USBD_LL_PrepareReceive(pdev, CDC_OUT_EP, hcls.cdc_rx, CDC_MPS);
    }
    return (uint8_t)USBD_OK;
}

static uint8_t SOF(USBD_HandleTypeDef *pdev)
{
    g_usbdbg.sof_count++;
    if (hcls.as_alt == 1U) {
        usb_audio_get_samples(hcls.audio_buf, AUDIO_SAMPLES_PER_FRAME);
        USBD_LL_Transmit(pdev, AUDIO_IN_EP, (uint8_t *)hcls.audio_buf, AUDIO_PKT);
    }
    return (uint8_t)USBD_OK;
}

static uint8_t *cfg_desc(uint16_t *length)
{
    g_usbdbg.cfg_reads++;
    *length = (uint16_t)sizeof(CfgDesc);
    return CfgDesc;
}
static uint8_t *GetFSCfgDesc(uint16_t *length) { return cfg_desc(length); }
static uint8_t *GetHSCfgDesc(uint16_t *length) { return cfg_desc(length); }
static uint8_t *GetOtherCfgDesc(uint16_t *length) { return cfg_desc(length); }
static uint8_t *GetDevQualDesc(uint16_t *length) { *length = sizeof(DevQualDesc); return DevQualDesc; }

USBD_ClassTypeDef USBD_CDC_AUDIO = {
    Init, DeInit, Setup,
    NULL,                 /* EP0_TxSent */
    EP0_RxReady,
    DataIn, DataOut,
    SOF,
    NULL, NULL,           /* IsoINIncomplete / IsoOUTIncomplete */
    GetHSCfgDesc, GetFSCfgDesc, GetOtherCfgDesc, GetDevQualDesc,
#if (USBD_SUPPORT_USER_STRING_DESC == 1U)
    NULL,
#endif
};

/* ---- CDC transmit used by usb_printf ------------------------------------ */
uint8_t usb_cdc_tx(uint8_t *buf, uint16_t len)
{
    if (hUsbDeviceFS.pClassData == NULL) return (uint8_t)USBD_FAIL;
    if (hcls.cdc_tx_state != 0) return (uint8_t)USBD_BUSY;
    hcls.cdc_tx_state = 1;
    USBD_LL_Transmit(&hUsbDeviceFS, CDC_IN_EP, buf, len);
    return (uint8_t)USBD_OK;
}
