/* spi.c
 *
 * high level spi implementation
 *
 * Copyright 2013 RPH Engineering, VPI Engineering
 */

#include "spi.h"
#include <hal.h>
#include <stm32f4xx_hal_spi.h>
#include <stm32f4xx_hal_gpio.h>
#include <stm32f4xx_hal.h>
#include <stm32f4xx_hal_dma.h>
#include <stm32f429xx.h>

void spi_dma_init(void)
{
	DMA_InitTypeDef dma_is;

	dma_is.Channel = DMA_CHANNEL_3;
	dma_is.Direction = DMA_MEMORY_TO_PERIPH;
	dma_is.FIFOMode = DMA_FIFOMODE_DISABLE;
	dma_is.MemBurst = DMA_MBURST_SINGLE;
	dma_is.MemInc = DMA_MINC_DISABLE;
	dma_is.Mode = DMA_

//	dma_is.DMA_Channel = DMA_Channel_0;
//	dma_is.DMA_Memory0BaseAddr = (uint32_t)screenBuffer;
//	dma_is.DMA_PeripheralBaseAddr = (uint32_t)(0x4000380C); //SPI2 DR
//	dma_is.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
//	dma_is.DMA_PeripheralDataSize = DMA_MemoryDataSize_Byte;
//	dma_is.DMA_DIR = DMA_DIR_MemoryToPeripheral;
//	dma_is.DMA_Mode = DMA_Mode_Normal;
//	dma_is.DMA_MemoryInc = DMA_MemoryInc_Enable;
//	dma_is.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
//	dma_is.DMA_BufferSize = 6*84;
//	dma_is.DMA_Priority = DMA_Priority_High;
//	dma_is.DMA_MemoryBurst = DMA_MemoryBurst_Single;
//	dma_is.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
//	dma_is.DMA_FIFOMode = DMA_FIFOMode_Disable;

	DMA_Init(DMA1_Stream4, &dma_is);
	DMA_Cmd(DMA1_Stream4, ENABLE);

	// ...
	SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);
}

void spi_init(void)
{
    SPI_InitTypeDef  spiInitStructure;
    GPIO_InitTypeDef gpioInitStructure;

    HAL_SPI_MspInit(&SpiHandle);

    __SPI1_CLK_ENABLE();

    // SPI2 SCK and MOSI
    gpioInitStructure.Pin   = SPI1_SCK.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_AF_PP;
    gpioInitStructure.Alternate = GPIO_AF5_SPI1;
    gpioInitStructure.Pull  = GPIO_NOPULL;
    //gpioInitStructure.Alternate = 1;
    HAL_GPIO_Init(SPI1_SCK.port, &gpioInitStructure);

    gpioInitStructure.Pin   = SPI1_MOSI.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_AF_PP;
    gpioInitStructure.Pull  = GPIO_NOPULL;
    gpioInitStructure.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(SPI1_MOSI.port, &gpioInitStructure);

    // SPI2 MISO
    gpioInitStructure.Pin   = SPI1_MISO.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_INPUT;
    gpioInitStructure.Pull = GPIO_PULLUP;
    gpioInitStructure.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(SPI1_MISO.port, &gpioInitStructure);

    // RFID NSS
    gpioInitStructure.Pin   = LCD_NSS.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_OUTPUT_PP;
    gpioInitStructure.Pull  = GPIO_NOPULL;
    gpioInitStructure.Alternate = 0;
    HAL_GPIO_Init(LCD_NSS.port, &gpioInitStructure);
    HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 1);       // TBD - should this be before init?

//    gpioInitStructure.Pin   = LCD_NSS.pin;
//    gpioInitStructure.Speed = GPIO_SPEED_FAST;
//    gpioInitStructure.Mode  = GPIO_MODE_AF_PP;
//    gpioInitStructure.Pull  = GPIO_NOPULL;
//    gpioInitStructure.Alternate = GPIO_AF5_SPI1;
//    HAL_GPIO_Init(LCD_NSS.port, &gpioInitStructure);
//    //HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 1);       // TBD - should this be before init?


    SpiHandle.Instance 				  = SPI1;
    SpiHandle.Init.Direction 		  = SPI_DIRECTION_2LINES;
    SpiHandle.Init.Mode               = SPI_MODE_MASTER;
    SpiHandle.Init.DataSize           = SPI_DATASIZE_8BIT; //Was 8BIT
    SpiHandle.Init.CLKPolarity 		  = SPI_POLARITY_HIGH;
    SpiHandle.Init.CLKPhase			  = SPI_PHASE_2EDGE;
    SpiHandle.Init.NSS                = SPI_NSS_SOFT; //SPI_NSS_SOFT;
    SpiHandle.Init.BaudRatePrescaler  = SPI_BAUDRATEPRESCALER_2
    SpiHandle.Init.FirstBit           = SPI_FIRSTBIT_MSB;
    SpiHandle.Init.CRCCalculation 	  = SPI_CRCCALCULATION_DISABLED;
    SpiHandle.Init.TIMode 			  = SPI_TIMODE_DISABLED;
    SpiHandle.Init.CRCPolynomial      = 7;

    if(HAL_SPI_Init(&SpiHandle) != HAL_OK)
    {
      /* Initialization Error */
      //Error_Handler();
    }
}



//int spi_getSpi2Semaphore(void)
//{
//    // tries to take the spi2 semaphore
//    // returns 0 if semaphore successfully taken, -1 if not
//
//    int status  = 0;
//
//    if( spi2Semaphore > 0 )
//        spi2Semaphore = 0;
//    else
//        status = -1;
//
//    return status;
//}



//void spi_postSpi2Semaphore(void)
//{
//    // post (make available) spi2 semaphore
//
//    spi2Semaphore = 1;
//}

uint8_t txTmp[] = {0,0};
void spi_readWrite(SPI_HandleTypeDef SpiH, uint16_t* rxBuf, uint16_t* txBuf, uint16_t cnt)
{
	txTmp[0] = txBuf[0] >> 8;
	txTmp[1] = txBuf[0] & 0xFF;

	//HAL_SPI_TransmitReceive(&SpiHandle, txBuf, rxBuf, cnt, 1000);
	HAL_SPI_Transmit(&SpiHandle, txTmp, 2 /*cnt * 2*/, 1);
	//while(HAL_SPI_GetState(&SpiH) != HAL_SPI_STATE_READY);
	//HAL_SPI_Transmit_DMA(&SpiH, txBuf, cnt);
	//while(HAL_SPI_GetState(&SpiH) != HAL_SPI_STATE_READY);

//	//High, second edge
//	//We're going to bitbang it for now, I guess
//	HAL_GPIO_WritePin(SPI1_SCK.port, SPI1_SCK.pin, 0);
//	HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 0);
//	int i, j;
//	for(i = 0; i < cnt; i++)
//	{
//		for(j = 0; j < 8; j++)
//		{
//			HAL_GPIO_WritePin(SPI1_MOSI.port, SPI1_MOSI.pin, (txBuf[i] >> (7 - j)) & 1);
//			HAL_GPIO_WritePin(SPI1_SCK.port, SPI1_SCK.pin, 0);
//			HAL_GPIO_WritePin(SPI1_SCK.port, SPI1_SCK.pin, 1);
//		}
//	}
//	HAL_GPIO_WritePin(LCD_NSS.port, LCD_NSS.pin, 1);


}

//int spi_readWrite(SPI_TypeDef* SPIx, uint8_t *rxBuf, const uint8_t *txBuf, int cnt, uint16_t speed)
//{
//    // send data to the spi bus, read data in from the spi bus
//    // returns the number of bytes written/read
//
//    enum { maxTries = 5000 };
//
//    int      i;
//    uint32_t tries;
//
//    // since everything currently runs at the same speed, we will not set the speed each time
//    // we will leave the set speed line here in case that changes
//    //SPIx->CR1 = (SPIx->CR1 & -SPI_BaudRatePrescaler_256) | speed;
//
//    if( cnt >= 0 )
//    {
//        for( i = 0; i < cnt; ++i )
//        {
//            if( txBuf )
//            {
//                SPI_I2S_SendData(SPIx, *txBuf++);
//                HAL_spi
//            }
//            else
//            {
//                SPI_I2S_SendData(SPIx, 0xff);
//            }
//
//            tries = 0;
//            while( SPI_I2S_GetFlagStatus(SPIx, SPI_FLAG_RXNE /*SPI_I2S_FLAG_RXNE*/) == RESET  &&  tries < maxTries )
//                ++tries;
//
//            if( tries >= maxTries )
//            {
//                // did not get rx data in indication, re-init spi
//                spi_init();
//            }
//
//            if( rxBuf )
//            {
//                *rxBuf++ = SPI_I2S_ReceiveData(SPIx);
//            }
//            else
//            {
//                SPI_I2S_ReceiveData(SPIx);
//            }
//        }
//    }
//    else
//    {
//        i = -1;
//    }
//
//    return i;
//}
