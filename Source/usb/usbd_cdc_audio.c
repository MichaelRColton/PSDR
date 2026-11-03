/* usbd_cdc_audio.c - Composite CDC ACM console + UAC1 microphone (see header). */
#include "usbd_cdc_audio.h"
#include "usbd_core.h"
#include "usbd_ctlreq.h"

/* ---- endpoints ---------------------------------------------------------- */
#define CDC_IN_EP        0x81U   /* bulk  IN  : console -> host              */
#define CDC_OUT_EP       0x01U   /* bulk  OUT : host -> console              */
#define CDC_CMD_EP       0x82U   /* intr  IN  : CDC notifications (unused)   */
#define AUDIO_IN_EP      0x83U   /* iso   IN  : mono demod audio -> host      */
#define AUDIO_IQ_IN_EP   0x84U   /* iso   IN  : stereo I/Q (L=I,R=Q) -> host  */

#define CDC_MPS          64U
#define CDC_CMD_MPS      8U
#define AUDIO_MAX_PKT    (AUDIO_MAX_SAMPLES_PER_FRAME * 2U)        /* 64 B: mono iso wMaxPacketSize   */
#define AUDIO_IQ_MAX_PKT (AUDIO_MAX_SAMPLES_PER_FRAME * 2U * 2U)   /* 128 B: stereo iso wMaxPacketSize */

#define USB_CDC_AUDIO_CONFIG_DESC_SIZ   248U

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
    uint8_t  as_alt;                 /* iface 3 (mono demod) alt setting (0/1)  */
    uint8_t  as_alt_iq;              /* iface 4 (stereo I/Q) alt setting (0/1)  */
    uint8_t  ep0_buf[8];
    int16_t  audio_buf[AUDIO_MAX_SAMPLES_PER_FRAME];      /* mono demod frame    */
    int16_t  iq_buf[AUDIO_MAX_SAMPLES_PER_FRAME * 2];     /* stereo I/Q frame    */
} CDC_AUDIO_Handle;

static CDC_AUDIO_Handle hcls;        /* single static instance (one device)    */
static volatile uint8_t iso_armed = 0;  /* demod iso IN transfer queued/in-flight    */
static volatile uint8_t iso_stuck = 0;  /* demod SOF watchdog                         */
static volatile uint8_t iq_armed  = 0;  /* I/Q iso IN transfer queued/in-flight       */
static volatile uint8_t iq_stuck  = 0;  /* I/Q SOF watchdog                           */

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
    uint32_t iso_arm;          /* demod iso IN transfers armed                          */
    uint32_t iso_datain;       /* demod iso IN transfers completed                      */
    uint32_t iso_incomplete;   /* demod IISOIXFR                                         */
    uint32_t iq_arm;           /* I/Q iso IN transfers armed                            */
    uint32_t iq_datain;        /* I/Q iso IN transfers completed                        */
    uint32_t iq_incomplete;    /* I/Q IISOIXFR                                           */
} usb_dbg_t;
volatile usb_dbg_t g_usbdbg = { .magic = 0x0DB00001U };

/* ---- combined configuration descriptor ---------------------------------- */
__ALIGN_BEGIN static uint8_t CfgDesc[] __ALIGN_END = {
    /* ---- Configuration ---- (wTotalLength / bNumInterfaces fixed below, asserted == sizeof) */
    0x09, USB_DESC_TYPE_CONFIGURATION,
    LOBYTE(USB_CDC_AUDIO_CONFIG_DESC_SIZ), HIBYTE(USB_CDC_AUDIO_CONFIG_DESC_SIZ),
    0x05,                 /* bNumInterfaces: CDC(0,1) + Audio(2,3,4)          */
    0x01,                 /* bConfigurationValue                              */
    0x00,                 /* iConfiguration                                   */
    0xC0,                 /* bmAttributes: self-powered                       */
    0x32,                 /* bMaxPower: 100 mA                                */

    /* ===== IAD: CDC (interfaces 0-1) ===== */
    0x08, 0x0B, 0x00, 0x02, 0x02, 0x02, 0x01, 0x00,

    /* ---- CDC communication interface (0) ---- iInterface=8 "PSDR Console" */
    0x09, USB_DESC_TYPE_INTERFACE, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x08,
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

    /* ===== IAD: Audio (interfaces 2-4) ===== bFunctionClass=AUDIO, subclass=AUDIOCONTROL */
    0x08, 0x0B, 0x02, 0x03, 0x01, 0x01, 0x00, 0x00,

    /* ---- Audio Control interface (2) ---- */
    0x09, USB_DESC_TYPE_INTERFACE, 0x02, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
    /* AC header: bcdADC 1.00, wTotalLen=52, 2 AS ifaces (3=demod, 4=I/Q) */
    0x0A, 0x24, 0x01, 0x00, 0x01, 0x34, 0x00, 0x02, 0x03, 0x04,
    /* demod chain: Input Terminal ID1 (Microphone, 1ch) -> Output Terminal ID2 (USB streaming) */
    0x0C, 0x24, 0x02, 0x01, 0x01, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06,  /* IT1 iTerminal=6 */
    0x09, 0x24, 0x03, 0x02, 0x01, 0x01, 0x00, 0x01, 0x06,                    /* OT2 ID2 iTerminal=6 "PSDR Demod" */
    /* I/Q chain: Input Terminal ID3 (Microphone, 2ch L+R) -> Output Terminal ID4 (USB streaming) */
    0x0C, 0x24, 0x02, 0x03, 0x01, 0x02, 0x00, 0x02, 0x03, 0x00, 0x00, 0x07,  /* IT3 iTerminal=7 */
    0x09, 0x24, 0x03, 0x04, 0x01, 0x01, 0x00, 0x03, 0x07,                    /* OT4 iTerminal=7 "PSDR I/Q" */

    /* ---- Audio Streaming interface (3) = mono demod ---- */
    0x09, USB_DESC_TYPE_INTERFACE, 0x03, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,  /* alt 0 idle */
    0x09, USB_DESC_TYPE_INTERFACE, 0x03, 0x01, 0x01, 0x01, 0x02, 0x00, 0x06,  /* alt 1 active, iInterface=6 */
    0x07, 0x24, 0x01, 0x02, 0x01, 0x01, 0x00,              /* AS general: terminal link=2, PCM */
    0x0B, 0x24, 0x02, 0x01, 0x01, 0x02, 0x10, 0x01,        /* Format Type I: 1ch, 16-bit, 1 freq */
          LOBYTE(AUDIO_SAMPLE_RATE), (uint8_t)((AUDIO_SAMPLE_RATE >> 8) & 0xFF),
          (uint8_t)((AUDIO_SAMPLE_RATE >> 16) & 0xFF),
    0x09, USB_DESC_TYPE_ENDPOINT, AUDIO_IN_EP, 0x05,       /* iso, async */
          LOBYTE(AUDIO_MAX_PKT), HIBYTE(AUDIO_MAX_PKT), 0x01, 0x00, 0x00,
    0x07, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00,              /* CS audio endpoint */

    /* ---- Audio Streaming interface (4) = stereo I/Q (L=I, R=Q) ---- */
    0x09, USB_DESC_TYPE_INTERFACE, 0x04, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,  /* alt 0 idle */
    0x09, USB_DESC_TYPE_INTERFACE, 0x04, 0x01, 0x01, 0x01, 0x02, 0x00, 0x07,  /* alt 1 active, iInterface=7 */
    0x07, 0x24, 0x01, 0x04, 0x01, 0x01, 0x00,              /* AS general: terminal link=4, PCM */
    0x0B, 0x24, 0x02, 0x01, 0x02, 0x02, 0x10, 0x01,        /* Format Type I: 2ch, 16-bit, 1 freq */
          LOBYTE(AUDIO_SAMPLE_RATE), (uint8_t)((AUDIO_SAMPLE_RATE >> 8) & 0xFF),
          (uint8_t)((AUDIO_SAMPLE_RATE >> 16) & 0xFF),
    0x09, USB_DESC_TYPE_ENDPOINT, AUDIO_IQ_IN_EP, 0x05,    /* iso, async */
          LOBYTE(AUDIO_IQ_MAX_PKT), HIBYTE(AUDIO_IQ_MAX_PKT), 0x01, 0x00, 0x00,
    0x07, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00,              /* CS audio endpoint */
};
_Static_assert(sizeof(CfgDesc) == USB_CDC_AUDIO_CONFIG_DESC_SIZ,
               "combined config descriptor byte count != 248");

__ALIGN_BEGIN static uint8_t DevQualDesc[10] __ALIGN_END =
    { 10, 0x06, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00 };

/* ---- audio sample source (weak default = quiet 1 kHz test tone) --------- */
__attribute__((weak)) uint32_t usb_audio_get_samples(int16_t *dst, uint32_t max)
{
    static uint32_t ph;
    /* Quiet ~1 kHz triangle test tone (no DSP source linked). One frame's worth. */
    uint32_t n = (AUDIO_SAMPLES_PER_FRAME < max) ? AUDIO_SAMPLES_PER_FRAME : max;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t p = ph % 24U;                   /* 24 samples/cycle @ 24 kHz = 1 kHz */
        int32_t v = (p < 12) ? (int32_t)p - 6 : 18 - (int32_t)p;
        dst[i] = (int16_t)(v * 200);
        ph++;
    }
    return n;
}

/* Weak no-op so a build without a DSP source still links. main.c overrides it. */
__attribute__((weak)) void usb_audio_push(int16_t sample) { (void)sample; }

/* Stereo I/Q source: fill up to `frames` interleaved L,R (=I,Q) frames, return
 * frames written. Weak default = silence; the DSP overrides it in main.c. */
__attribute__((weak)) uint32_t usb_iq_get_samples(int16_t *dst, uint32_t frames)
{
    for (uint32_t i = 0; i < frames * 2U; i++) dst[i] = 0;
    return frames;
}
__attribute__((weak)) void usb_iq_push(int16_t i_samp, int16_t q_samp)
{ (void)i_samp; (void)q_samp; }

int usb_audio_streaming(void) { return hcls.as_alt == 1; }
int usb_iq_streaming(void)    { return hcls.as_alt_iq == 1; }

/* ---- class callbacks ---------------------------------------------------- */
static uint8_t Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
    (void)cfgidx;
    g_usbdbg.init_calls++;
    static const uint8_t lc[7] = { 0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08 };
    for (int i = 0; i < 7; i++) hcls.line_coding[i] = lc[i];
    hcls.cdc_tx_state = 0;
    hcls.as_alt = 0;
    hcls.as_alt_iq = 0;
    iso_armed = 0; iso_stuck = 0; iq_armed = 0; iq_stuck = 0;

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
    if (hcls.as_alt)    { USBD_LL_CloseEP(pdev, AUDIO_IN_EP);    hcls.as_alt = 0; }
    if (hcls.as_alt_iq) { USBD_LL_CloseEP(pdev, AUDIO_IQ_IN_EP); hcls.as_alt_iq = 0; }
    pdev->pClassData = NULL;
    return (uint8_t)USBD_OK;
}

/* Queue one audio frame on the iso IN endpoint. Called from the USB ISR only
   (SOF kick, DataIn chain, alt-set prime), so no locking is needed here; the
   sample source is the one producer/consumer boundary with the main loop. */
static void iso_send(USBD_HandleTypeDef *pdev)
{
    g_usbdbg.iso_arm++;
    iso_armed = 1U;
    iso_stuck = 0U;
    /* Send EXACTLY one nominal frame (24 samples) every time, so the device's
       output rate is exactly AUDIO_SAMPLE_RATE and matches the host's playback
       clock - no drift, no growing latency. The ring (filled at the same nominal
       rate by the DSP) absorbs jitter; a rare underrun pads a little silence and
       a rare overrun drops in usb_audio_push(), each a sub-ppm glitch, never lag.
       Always non-zero, so the DataIn completion chain can't stall on a ZLP. */
    uint32_t n = usb_audio_get_samples(hcls.audio_buf, AUDIO_SAMPLES_PER_FRAME);
    while (n < AUDIO_SAMPLES_PER_FRAME) hcls.audio_buf[n++] = 0;
    (void)USBD_LL_Transmit(pdev, AUDIO_IN_EP, (uint8_t *)hcls.audio_buf,
                           (uint16_t)(AUDIO_SAMPLES_PER_FRAME * 2U));
}

/* Same as iso_send but stereo I/Q on AUDIO_IQ_IN_EP: exactly one nominal frame
   (24 L,R pairs = 96 bytes) per call, so the rate matches the host playback clock. */
static void iq_send(USBD_HandleTypeDef *pdev)
{
    g_usbdbg.iq_arm++;
    iq_armed = 1U;
    iq_stuck = 0U;
    uint32_t n = usb_iq_get_samples(hcls.iq_buf, AUDIO_SAMPLES_PER_FRAME);   /* frames */
    while (n < AUDIO_SAMPLES_PER_FRAME) { hcls.iq_buf[n * 2U] = 0; hcls.iq_buf[n * 2U + 1U] = 0; n++; }
    (void)USBD_LL_Transmit(pdev, AUDIO_IQ_IN_EP, (uint8_t *)hcls.iq_buf,
                           (uint16_t)(AUDIO_SAMPLES_PER_FRAME * 2U * 2U));   /* *2ch *2bytes */
}

static void audio_set_alt(USBD_HandleTypeDef *pdev, uint8_t alt)
{
    if (alt == hcls.as_alt) return;
    if (alt == 1U) {
        g_usbdbg.audio_alt1_opens++;
        USBD_LL_OpenEP(pdev, AUDIO_IN_EP, USBD_EP_TYPE_ISOC, AUDIO_MAX_PKT);
        pdev->ep_in[AUDIO_IN_EP & 0x0FU].is_used = 1U;
        hcls.as_alt = 1;
        iso_send(pdev);               /* prime the first frame */
    } else {
        USBD_LL_CloseEP(pdev, AUDIO_IN_EP);
        pdev->ep_in[AUDIO_IN_EP & 0x0FU].is_used = 0U;
        hcls.as_alt = 0;
        iso_armed = 0U;
    }
}

static void iq_set_alt(USBD_HandleTypeDef *pdev, uint8_t alt)
{
    if (alt == hcls.as_alt_iq) return;
    if (alt == 1U) {
        USBD_LL_OpenEP(pdev, AUDIO_IQ_IN_EP, USBD_EP_TYPE_ISOC, AUDIO_IQ_MAX_PKT);
        pdev->ep_in[AUDIO_IQ_IN_EP & 0x0FU].is_used = 1U;
        hcls.as_alt_iq = 1;
        iq_send(pdev);                /* prime the first frame */
    } else {
        USBD_LL_CloseEP(pdev, AUDIO_IQ_IN_EP);
        pdev->ep_in[AUDIO_IQ_IN_EP & 0x0FU].is_used = 0U;
        hcls.as_alt_iq = 0;
        iq_armed = 0U;
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
            hcls.ep0_buf[0] = (iface == 3U) ? hcls.as_alt
                            : (iface == 4U) ? hcls.as_alt_iq : 0U;
            USBD_CtlSendData(pdev, hcls.ep0_buf, 1U);
            break;
        case USB_REQ_SET_INTERFACE:
            g_usbdbg.setif++; g_usbdbg.last_setif_iface = iface;
            g_usbdbg.last_setif_alt = (uint8_t)(req->wValue & 0xFFU);
            if (iface == 3U)      audio_set_alt(pdev, (uint8_t)(req->wValue & 0xFFU));
            else if (iface == 4U) iq_set_alt(pdev, (uint8_t)(req->wValue & 0xFFU));
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
    if (epnum == (CDC_IN_EP & 0x0FU)) {
        hcls.cdc_tx_state = 0;
    } else if (epnum == (AUDIO_IN_EP & 0x0FU)) {
        g_usbdbg.iso_datain++;
        /* chain the next frame straight off completion -> one clean packet/frame */
        if (hcls.as_alt == 1U) iso_send(pdev);
    } else if (epnum == (AUDIO_IQ_IN_EP & 0x0FU)) {
        g_usbdbg.iq_datain++;
        if (hcls.as_alt_iq == 1U) iq_send(pdev);
    }
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
    /* Steady-state pacing is the DataIn completion chain. The SOF starts it and
       acts as a watchdog: if a completion is missed and iso_armed stays stuck for
       a few frames, flush and restart so the stream can't wedge. The >=3 threshold
       tolerates the normal 1-frame arm->send->complete pipeline. */
    if (hcls.as_alt == 1U) {
        if (iso_armed == 0U) {
            iso_send(pdev);
        } else if (++iso_stuck >= 3U) {
            (void)USBD_LL_FlushEP(pdev, AUDIO_IN_EP);
            iso_armed = 0U;
            iso_send(pdev);
        }
    }
    if (hcls.as_alt_iq == 1U) {
        if (iq_armed == 0U) {
            iq_send(pdev);
        } else if (++iq_stuck >= 3U) {
            (void)USBD_LL_FlushEP(pdev, AUDIO_IQ_IN_EP);
            iq_armed = 0U;
            iq_send(pdev);
        }
    }
    return (uint8_t)USBD_OK;
}

/* iso IN transfer did not go out in its scheduled frame (host missed the poll).
   Flush the stale packet so it doesn't back up the FIFO, and let SOF re-kick. */
static uint8_t IsoINIncomplete(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
    (void)pdev;
    /* IISOIXFR is a device-global interrupt and the HAL's epnum does NOT reliably
       say which iso IN endpoint missed its frame. Do NOT flush here: flushing the
       wrong endpoint stalls the other stream (an unread I/Q stream was killing the
       demod stream this way). Just record it; each stream's SOF watchdog detects
       its own stuck transfer (armed for >=3 frames) and flushes/restarts only its
       own endpoint. Leaving armed set is what lets that watchdog fire. */
    if (epnum == (AUDIO_IQ_IN_EP & 0x0FU)) g_usbdbg.iq_incomplete++;
    else                                   g_usbdbg.iso_incomplete++;
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

#if (USBD_SUPPORT_USER_STRING_DESC == 1U)
/* Distinct names for the two capture devices. The terminals/interfaces of the
   demod chain carry iString 6 and the I/Q chain iString 7 (see CfgDesc). */
static uint8_t UsrStrBuf[48];
static uint8_t *GetUsrStrDescriptor(USBD_HandleTypeDef *pdev, uint8_t index, uint16_t *length)
{
    (void)pdev;
    const char *s;
    switch (index) {
        case 6U:  s = "PSDR Demod";   break;
        case 7U:  s = "PSDR I/Q";     break;
        case 8U:  s = "PSDR Console"; break;
        default:  *length = 0U; return NULL;
    }
    USBD_GetString((uint8_t *)s, UsrStrBuf, length);
    return UsrStrBuf;
}
#endif

USBD_ClassTypeDef USBD_CDC_AUDIO = {
    Init, DeInit, Setup,
    NULL,                 /* EP0_TxSent */
    EP0_RxReady,
    DataIn, DataOut,
    SOF,
    IsoINIncomplete, NULL,  /* IsoINIncomplete / IsoOUTIncomplete */
    GetHSCfgDesc, GetFSCfgDesc, GetOtherCfgDesc, GetDevQualDesc,
#if (USBD_SUPPORT_USER_STRING_DESC == 1U)
    GetUsrStrDescriptor,
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
