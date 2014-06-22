/* spi.h
 *
 * high level spi interface
 *
 * Copyright 2013 RPH Engineering, VPI Engineering
 */

#ifndef SPI_H_
    #define SPI_H_

    #include <stdint.h>
    //#include <stm32f10x.h>
#include <stm32f415xx.h>
//#include <stm32f4xx_hal.h>
#include <stm32f4xx_hal_spi.h>

SPI_HandleTypeDef SpiHandle;

    void spi_init(void);
        // set up the used SPI (SPI2) and pins

//    int spi_getSpi2Semaphore(void);
//        // tries to take the spi2 semaphore
//        // returns 0 if semaphore successfully taken, -1 if not
//
//    void spi_postSpi2Semaphore(void);
//        // post (make available) spi2 semaphore
    void spi_readWrite(SPI_HandleTypeDef SpiHandle, uint8_t *rxBuf, uint8_t *txBuf, int cnt);
    //int spi_readWrite(SPI_TypeDef* SPIx, uint8_t *rxBuf, const uint8_t *txBuf, int cnt, uint16_t speed);
        // send data to the spi bus, read data in from the spi bus
        // returns the number of bytes written/read

#endif /* SPI_H_ */
