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
//#include <stm32f415xx.h>
#include <stm32f4xx_hal.h>


//static int spi2Semaphore;

//SPI_HandleTypeDef SpiHandle;

void spi_init(void)
{
    // set up the used SPI (SPI2) and pins




    SPI_InitTypeDef  spiInitStructure;
    GPIO_InitTypeDef gpioInitStructure;

    HAL_SPI_MspInit(&SpiHandle);


    __SPI1_CLK_ENABLE();





//    // SPI2 SCK and MOSI
//    gpioInitStructure.GPIO_Pin   = SPI1_SCK.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
//    GPIO_Init(SPI1_SCK.port, &gpioInitStructure);
//
//    gpioInitStructure.GPIO_Pin   = SPI1_MOSI.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
//    GPIO_Init(SPI1_MOSI.port, &gpioInitStructure);
//
//    // SPI2 MISO
//    gpioInitStructure.GPIO_Pin   = SPI1_MISO.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_IPU;
//    GPIO_Init(SPI1_MISO.port, &gpioInitStructure);
//
//    // RFID NSS
//    gpioInitStructure.GPIO_Pin   = LCD_NSS.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
//    GPIO_Init(LCD_NSS.port, &gpioInitStructure);
//    GPIO_WriteBit(LCD_NSS.port, LCD_NSS.pin, (BitAction)1);       // TBD - should this be before init?

    // SPI2 SCK and MOSI
    gpioInitStructure.Pin   = SPI1_SCK.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_AF_PP;
    gpioInitStructure.Alternate = GPIO_AF5_SPI1;
    //gpioInitStructure.Mode  = GPIO_MODE_OUTPUT_PP;
    gpioInitStructure.Pull  = GPIO_NOPULL;
    //gpioInitStructure.Alternate = 1;
    HAL_GPIO_Init(SPI1_SCK.port, &gpioInitStructure);

    gpioInitStructure.Pin   = SPI1_MOSI.pin;
    gpioInitStructure.Speed = GPIO_SPEED_FAST;
    gpioInitStructure.Mode  = GPIO_MODE_AF_PP;
    //gpioInitStructure.Mode  = GPIO_MODE_OUTPUT_PP;
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


    // Accelerometer NSS
//    gpioInitStructure.GPIO_Pin   = ACCEL_NSS.pin;
//    gpioInitStructure.GPIO_Speed = GPIO_Speed_2MHz;
//    gpioInitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
//    GPIO_Init(ACCEL_NSS.port, &gpioInitStructure);
//    GPIO_WriteBit(ACCEL_NSS.port, ACCEL_NSS.pin, (BitAction)1);     // TBD - should this be before init?

    // init semaphore
//    spi2Semaphore = 1;

    //GPIO_PinRemapConfig(GPIO_Remap_SPI1, ENABLE); //So I can use the other SPI1 pins? (To keep the DACs availble)
    //GPIO_Remap_SPI1

    // SPI 1
    // SPI_StructInit(&spiInitStructure);
    //spiInitStructure.
    //HAL_SPI_

    SpiHandle.Instance = SPI1;
    SpiHandle.Init.Direction = SPI_DIRECTION_2LINES;//   SPI_Direction_2Lines_FullDuplex;
    SpiHandle.Init.Mode               = SPI_MODE_MASTER; //  SPI_Mode_Master;
    SpiHandle.Init.DataSize           = SPI_DATASIZE_8BIT; //  SPI_DataSize_8b;
    SpiHandle.Init.CLKPolarity /*CPOL*/               =  SPI_POLARITY_HIGH; // SPI_C SPI_CPOL_High;
    SpiHandle.Init.CLKPhase /*CPHA*/               = SPI_PHASE_2EDGE; // SPI_CPHA_2Edge;
    SpiHandle.Init.NSS                = SPI_NSS_SOFT; // SPI_NSS_Soft;
    SpiHandle.Init.BaudRatePrescaler  = SPI_BAUDRATEPRESCALER_2; //SPI_BaudRatePrescaler_2;
    SpiHandle.Init.FirstBit           = SPI_FIRSTBIT_MSB; // SPI_FirstBit_MSB;
    SpiHandle.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLED;
    SpiHandle.Init.TIMode = SPI_TIMODE_DISABLED;
    SpiHandle.Init.CRCPolynomial      = 7;

    if(HAL_SPI_Init(&SpiHandle) != HAL_OK)
    {
      /* Initialization Error */
      //Error_Handler();
    }
    //HAL_SPI_TransmitReceive();

    //SPI_Cmd(SPI2, ENABLE);
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

void spi_readWrite(SPI_HandleTypeDef SpiH, uint8_t* rxBuf, uint8_t* txBuf, int cnt)
{
	//HAL_SPI_TransmitReceive(&SpiHandle, txBuf, rxBuf, cnt, 1000);
	HAL_SPI_Transmit(&SpiHandle, txBuf, cnt, 1);

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
