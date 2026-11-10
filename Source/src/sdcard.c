/*
 * microSD card, first step: talk to the card.
 *
 * The socket is wired to the MCU's native SD interface (SDMMC1), 4-bit bus,
 * per the 2.6.1 netlist:
 *   SDIO_DAT0..3 = PC8, PC9, PC10, PC11     SDIO_CLK = PC12     SDIO_CMD = PD2
 *   SD_CARD_DETECT = PD13
 *
 * sd_probe() (console 'S') powers up the interface, identifies the card, and
 * reads sector 0 with plain polling (no DMA, no interrupts), then reports the
 * card's size and what sector 0 looks like (partition table or a FAT boot
 * sector). Nothing is ever written to the card here.
 *
 * Next step, not done: add a FAT file system (FatFs) on top of sd_read_sector().
 */
#include <string.h>
#include "stm32f7xx_hal.h"

void usb_printf(const char *fmt, ...);

static SD_HandleTypeDef hsd;
static HAL_SD_CardInfoTypedef sdInfo;
static int sdReady = 0;

void HAL_SD_MspInit(SD_HandleTypeDef *h)
{
	(void)h;
	GPIO_InitTypeDef g = {0};
	__HAL_RCC_SDMMC1_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	g.Mode = GPIO_MODE_AF_PP;
	g.Pull = GPIO_PULLUP;
	g.Speed = GPIO_SPEED_HIGH;
	g.Alternate = GPIO_AF12_SDMMC1;
	g.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
	HAL_GPIO_Init(GPIOC, &g);
	g.Pin = GPIO_PIN_2;
	HAL_GPIO_Init(GPIOD, &g);
}

/* returns 0 on success, otherwise the HAL's SD error code */
int sd_init(void)
{
	GPIO_InitTypeDef g = {0};
	__HAL_RCC_GPIOD_CLK_ENABLE();
	g.Pin = GPIO_PIN_13;                       /* card detect */
	g.Mode = GPIO_MODE_INPUT;
	g.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOD, &g);

	sdReady = 0;
	memset(&hsd, 0, sizeof(hsd));
	hsd.Instance = SDMMC1;
	hsd.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
	hsd.Init.ClockBypass = SDMMC_CLOCK_BYPASS_DISABLE;
	hsd.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
	hsd.Init.BusWide = SDMMC_BUS_WIDE_1B;
	hsd.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
	hsd.Init.ClockDiv = 6;                     /* 48 MHz / (6 + 2) = 6 MHz: slow and safe for polled reads */
	HAL_SD_ErrorTypedef e = HAL_SD_Init(&hsd, &sdInfo);
	if(e != SD_OK) return (int)e;
	sdReady = 1;
	return 0;
}

/* read one 512-byte sector; returns 0 on success. buf must be 4-byte aligned. */
int sd_read_sector(uint32_t sector, uint8_t *buf)
{
	if(!sdReady) return -1;
	return (int)HAL_SD_ReadBlocks(&hsd, (uint32_t *)buf, (uint64_t)sector * 512ULL, 512, 1);
}

void sd_probe(void)
{
	static uint8_t sec[512] __attribute__((aligned(4)));
	usb_printf("sd: card-detect pin=%d\r\n", (int)HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_13));
	int e = sd_init();
	if(e) { usb_printf("sd: init failed, error %d\r\n", e); return; }
	uint32_t mb = (uint32_t)(sdInfo.CardCapacity >> 20);
	usb_printf("sd: card type=%d capacity=%lu MB block=%lu\r\n", (int)sdInfo.CardType, (unsigned long)mb,
	           (unsigned long)sdInfo.CardBlockSize);
	e = sd_read_sector(0, sec);
	if(e) { usb_printf("sd: read sector 0 failed, error %d\r\n", e); return; }
	usb_printf("sd: sector 0 signature %02X%02X (55AA = valid)\r\n", sec[510], sec[511]);
	if(sec[510] != 0x55 || sec[511] != 0xAA) return;
	if(!memcmp(sec + 82, "FAT32", 5) || !memcmp(sec + 54, "FAT", 3) || !memcmp(sec + 3, "EXFAT", 5))
	{
		char name[9]; memcpy(name, sec + 3, 8); name[8] = 0;
		usb_printf("sd: sector 0 is a file-system boot sector (no partition table), OEM name '%s'\r\n", name);
		return;
	}
	for(int i = 0; i < 4; i++)
	{
		uint8_t *p = sec + 446 + i * 16;
		uint32_t lba = p[8] | (p[9] << 8) | (p[10] << 16) | ((uint32_t)p[11] << 24);
		uint32_t cnt = p[12] | (p[13] << 8) | (p[14] << 16) | ((uint32_t)p[15] << 24);
		if(p[4]) usb_printf("sd: partition %d type 0x%02X start sector %lu size %lu MB\r\n", i + 1, p[4],
		                    (unsigned long)lba, (unsigned long)(cnt >> 11));
	}
	/* look at the first partition's boot sector to name the file system */
	uint8_t *p = sec + 446;
	uint32_t lba = p[8] | (p[9] << 8) | (p[10] << 16) | ((uint32_t)p[11] << 24);
	if(p[4] && !sd_read_sector(lba, sec))
	{
		const char *fs = !memcmp(sec + 82, "FAT32", 5) ? "FAT32" : !memcmp(sec + 54, "FAT16", 5) ? "FAT16" :
		                 !memcmp(sec + 3, "EXFAT", 5) ? "exFAT" : !memcmp(sec + 3, "NTFS", 4) ? "NTFS" : "unknown";
		usb_printf("sd: first partition file system: %s\r\n", fs);
	}
}
