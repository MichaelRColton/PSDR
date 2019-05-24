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
//#include <stm32f415xx.h>
#ifdef PSDR2
	#include "stm32f429xx.h"
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include <stm32f4xx_hal_spi.h>
#endif
//#include <stm32f4xx_hal.h>



//#ifdef PSDR257
#include "stm32f7xx_hal.h"
  //#include "stm32f746xx.h"
//#include "stm32f7xx.h"
#include "stm32f7xx_hal.h"
//#include <stm32f7xx_hal_spi.h>
//#endif

//SPI_HandleTypeDef SpiHandle;

    void spi_init(void);
        // set up the used SPI (SPI2) and pins

//    int spi_getSpi2Semaphore(void);
//        // tries to take the spi2 semaphore
//        // returns 0 if semaphore successfully taken, -1 if not
//
//    void spi_postSpi2Semaphore(void);
//        // post (make available) spi2 semaphore

    void spi_readWrite(SPI_HandleTypeDef SpiHandle, uint16_t *rxBuf, uint16_t *txBuf, uint16_t cnt); //was uint8_t
    //int spi_readWrite(SPI_TypeDef* SPIx, uint8_t *rxBuf, const uint8_t *txBuf, int cnt, uint16_t speed);
        // send data to the spi bus, read data in from the spi bus
        // returns the number of bytes written/read

#endif /* SPI_H_ */
