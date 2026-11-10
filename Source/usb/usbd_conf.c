/* usbd_conf.c - USB-Device Library <-> STM32F7 PCD HAL glue (OTG_FS, CDC). */
#include "stm32f7xx_hal.h"
#include "usbd_core.h"
#include "usbd_cdc.h"

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* ===== HAL PCD MSP (pins/clock/NVIC) ====================================== */
void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd)
{
	GPIO_InitTypeDef g;
	if (hpcd->Instance == USB_OTG_FS)
	{
		__HAL_RCC_GPIOA_CLK_ENABLE();
		/* PA11 = DM, PA12 = DP, AF10 OTG_FS */
		g.Pin = GPIO_PIN_11 | GPIO_PIN_12;
		g.Mode = GPIO_MODE_AF_PP;
		g.Pull = GPIO_NOPULL;
		g.Speed = GPIO_SPEED_HIGH;
		g.Alternate = GPIO_AF10_OTG_FS;
		HAL_GPIO_Init(GPIOA, &g);

		__HAL_RCC_USB_OTG_FS_CLK_ENABLE();
		HAL_NVIC_SetPriority(OTG_FS_IRQn, 6, 0);
		HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
	}
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd)
{
	if (hpcd->Instance == USB_OTG_FS)
	{
		__HAL_RCC_USB_OTG_FS_CLK_DISABLE();
		HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11 | GPIO_PIN_12);
		HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
	}
}

/* ===== HAL PCD callbacks -> USB Device Library ============================ */
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
	USBD_LL_SetupStage((USBD_HandleTypeDef *)hpcd->pData, (uint8_t *)hpcd->Setup);
}
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
	USBD_LL_DataOutStage((USBD_HandleTypeDef *)hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
	USBD_LL_DataInStage((USBD_HandleTypeDef *)hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
	USBD_LL_SOF((USBD_HandleTypeDef *)hpcd->pData);
}
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
	USBD_LL_SetSpeed((USBD_HandleTypeDef *)hpcd->pData, USBD_SPEED_FULL);
	USBD_LL_Reset((USBD_HandleTypeDef *)hpcd->pData);
}
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
	USBD_LL_Suspend((USBD_HandleTypeDef *)hpcd->pData);
}
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
	USBD_LL_Resume((USBD_HandleTypeDef *)hpcd->pData);
}
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
	USBD_LL_IsoOUTIncomplete((USBD_HandleTypeDef *)hpcd->pData, epnum);
}
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
	USBD_LL_IsoINIncomplete((USBD_HandleTypeDef *)hpcd->pData, epnum);
}
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
	USBD_LL_DevConnected((USBD_HandleTypeDef *)hpcd->pData);
}
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
	USBD_LL_DevDisconnected((USBD_HandleTypeDef *)hpcd->pData);
}

/* ===== USB Device Library low-level driver (-> HAL PCD) =================== */
USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
	hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
	hpcd_USB_OTG_FS.Init.dev_endpoints = 6;
	hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
	hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
	hpcd_USB_OTG_FS.Init.ep0_mps = DEP0CTL_MPS_64;
	hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
	hpcd_USB_OTG_FS.Init.Sof_enable = ENABLE;   /* SOF paces the audio iso IN */
	hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
	hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
	hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;
	hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;

	/* Link the two handles both ways. */
	pdev->pData = &hpcd_USB_OTG_FS;
	hpcd_USB_OTG_FS.pData = pdev;

	if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK) { for(;;) {} }

	/* 320 x 32-bit words of FIFO RAM across RX + EP0-4 TX. At 48 kHz the iso
	   packets are demod 96 B (24 w) and I/Q 192 B (48 w).
	   RX 0x60, EP0 0x20, EP1 (CDC bulk) 0x40, EP2 (CDC cmd) 0x10,
	   EP3 (demod iso, 96 B) 0x20, EP4 (I/Q iso, 192 B = 48 w) 0x30 = 288. */
	HAL_PCDEx_SetRxFiFo(&hpcd_USB_OTG_FS, 0x60);
	HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 0, 0x20);
	HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 1, 0x40);
	HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 2, 0x10);
	HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 3, 0x20);
	HAL_PCDEx_SetTxFiFo(&hpcd_USB_OTG_FS, 4, 0x30);
	return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{ HAL_PCD_DeInit(pdev->pData); return USBD_OK; }

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{ HAL_PCD_Start(pdev->pData); return USBD_OK; }

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{ HAL_PCD_Stop(pdev->pData); return USBD_OK; }

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps)
{ HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type); return USBD_OK; }

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ HAL_PCD_EP_Close(pdev->pData, ep_addr); return USBD_OK; }

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ HAL_PCD_EP_Flush(pdev->pData, ep_addr); return USBD_OK; }

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ HAL_PCD_EP_SetStall(pdev->pData, ep_addr); return USBD_OK; }

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ HAL_PCD_EP_ClrStall(pdev->pData, ep_addr); return USBD_OK; }

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
	PCD_HandleTypeDef *hpcd = pdev->pData;
	return (ep_addr & 0x80U) ? hpcd->IN_ep[ep_addr & 0x7FU].is_stall
	                         : hpcd->OUT_ep[ep_addr & 0x7FU].is_stall;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{ HAL_PCD_SetAddress(pdev->pData, dev_addr); return USBD_OK; }

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size)
{ HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, (uint16_t)size); return USBD_OK; }

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size)
{ HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, (uint16_t)size); return USBD_OK; }

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{ return HAL_PCD_EP_GetRxCount(pdev->pData, ep_addr); }

void USBD_LL_Delay(uint32_t Delay) { HAL_Delay(Delay); }

/* Static single-block allocator (CDC handle fits here). */
void *USBD_static_malloc(uint32_t size)
{
	(void)size;
	static uint32_t mem[(sizeof(USBD_CDC_HandleTypeDef) / 4) + 1];
	return mem;
}
void USBD_static_free(void *p) { (void)p; }

/* Interrupt entry point (vector weak symbol is OTG_FS_IRQHandler). */
void OTG_FS_IRQHandler(void) { HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS); }
