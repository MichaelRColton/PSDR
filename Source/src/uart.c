/* uart.c
 *
 */

#include "uart.h"
#include <queue.h>
#include <hal.h>
#include <stddef.h>
#include <stm32f4xx_hal_usart.h>


enum
{
    uart1TxBufferSize   = 64,
    uart1RxBufferSize   = 64,
    uart2TxBufferSize   = 64,
    uart2RxBufferSize   = 128,
    uart3TxBufferSize   = 64,
    uart3RxBufferSize   = 64
};



static uint8_t uart1TxBuffer[uart1TxBufferSize];
static uint8_t uart1RxBuffer[uart1RxBufferSize];
static uint8_t uart2TxBuffer[uart2TxBufferSize];
static uint8_t uart2RxBuffer[uart2RxBufferSize];
static uint8_t uart3TxBuffer[uart3TxBufferSize];
static uint8_t uart3RxBuffer[uart3RxBufferSize];

static Queue8  uart1TxQ;
static Queue8  uart1RxQ;
static Queue8  uart2TxQ;
static Queue8  uart2RxQ;
static Queue8  uart3TxQ;
static Queue8  uart3RxQ;

static bool    uart1TxPrimed;                           // is uart tx reg empty interrupt enabled
static bool    uart2TxPrimed;
static bool    uart3TxPrimed;
static bool    uart1RxOverflow;                         // overlow in uart rx Q (currently set but not used)
static bool    uart2RxOverflow;
static bool    uart3RxOverflow;



int uart_putChar(uint8_t uart, char c)
{
    // enqueue a char to a uart
    // returns 0 on success

    int status;

    switch( uart )
    {
    case uart_uart1:
        status = queue8_enqueue(&uart1TxQ, (uint8_t)c);

        if( !uart1TxPrimed )
        {
            uart1TxPrimed = true;
            USART_ITConfig(USART1, USART_IT_TXE, ENABLE);
        }

        break;
    case uart_uart2:
        status = queue8_enqueue(&uart2TxQ, (uint8_t)c);

        if( !uart2TxPrimed )
        {
            uart2TxPrimed = true;
            USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
        }
        break;
    case uart_uart3:
        status = queue8_enqueue(&uart3TxQ, (uint8_t)c);

        if( !uart3TxPrimed )
        {
            uart3TxPrimed = true;
            USART_ITConfig(USART3, USART_IT_TXE, ENABLE);
        }

        break;
    default:
        status = -1;
    }

    return status;
}



int uart_getChar(uint8_t uart, char* c)
{
    // dequeue a char from a uart and put it in *c
    // returns 0 on success

    int status;

    switch( uart )
    {
    case uart_uart1:
        status = queue8_dequeue(&uart1RxQ, (uint8_t*)c);
        break;
    case uart_uart2:
        status = queue8_dequeue(&uart2RxQ, (uint8_t*)c);
        break;
    case uart_uart3:
        status = queue8_dequeue(&uart3RxQ, (uint8_t*)c);
        break;
    default:
        status = -1;
    }

    return status;
}



int uart_write(uint8_t uart, uint8_t* buffer, uint8_t size)
{
    // write a buffer of data to a uart
    // return the number of bytes written

    int     status;
    uint8_t bytes   = 0;
    Queue8* queue   = 0;

    USART_TypeDef* usart;

    switch( uart )
    {
    case uart_uart1:
        queue         = &uart1TxQ;
        uart1TxPrimed = true;
        usart         = USART1;
        break;
    case uart_uart2:
        queue         = &uart2TxQ;
        uart2TxPrimed = true;
        usart         = USART2;
        break;
    case uart_uart3:
        queue         = &uart3TxQ;
        uart3TxPrimed = true;
        usart         = USART3;
        break;
    default:
        break;
    }

    if( queue )
    {
        uint8_t* bufferPtr = buffer;

        for( bytes = 0; bytes < size; ++bytes )
        {
            status = queue8_enqueue(queue, *bufferPtr++);
            if( status )
                break;
        }

        USART_ITConfig(usart, USART_IT_TXE, ENABLE);
    }

    return bytes;
}



int uart_read(uint8_t uart, uint8_t* buffer, uint8_t size)
{
    // try to read size bytes of data into buffer from a uart
    // return the number of bytes read

    int     status;
    uint8_t bytes   = 0;
    Queue8* queue   = 0;

    switch( uart )
    {
    case uart_uart1:
        queue = &uart1RxQ;
        break;
    case uart_uart2:
        queue = &uart2RxQ;
        break;
    case uart_uart3:
        queue = &uart3RxQ;
        break;
    default:
        break;
    }

    if( queue )
    {
        uint8_t* bufferPtr = buffer;

        for( bytes = 0; bytes < size; ++bytes )
        {
            status = queue8_dequeue(queue, bufferPtr);
            if( status )
                break;

            ++bufferPtr;
        }
    }

    return bytes;
}



void uart_init(uint8_t uart)
{
    switch( uart )
    {
    case uart_uart1:
        // setup data structures
        queue8_init(&uart1TxQ, uart1TxBuffer, uart1TxBufferSize);
        queue8_init(&uart1RxQ, uart1RxBuffer, uart1RxBufferSize);
        
        // init h/w
        hal_setupUart1();

        break;
        
    case uart_uart2:
        queue8_init(&uart2TxQ, uart2TxBuffer, uart2TxBufferSize);
        queue8_init(&uart2RxQ, uart2RxBuffer, uart2RxBufferSize);

        hal_setupUart2();

        break;

    case uart_uart3:
        queue8_init(&uart3TxQ, uart3TxBuffer, uart3TxBufferSize);
        queue8_init(&uart3RxQ, uart3RxBuffer, uart3RxBufferSize);

#ifdef SIRF_MODULE
        hal_setupUart3(4800);
#endif
#ifdef MEDIATEK_MODULE
        hal_setupUart3(9600);
#endif

        break;

    default:
        // do nothing
        break;
    }
}



void uart_deinit(uint8_t uart)
{
    switch( uart )
    {
    case uart_uart1:
        hal_resetUart1();

        break;
        
    case uart_uart2:
        hal_resetUart2();

        break;

    case uart_uart3:
        hal_resetUart3();

        break;

    default:
        // do nothing
        break;
    }
}



void uart_enable(uint8_t uart)
{
    // enable a uart, enables rxne interrupt, does not set other config info

    switch( uart )
    {
    case uart_uart1:
        USART_Cmd(USART1, ENABLE);
        USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
        break;
    case uart_uart2:
        USART_Cmd(USART2, ENABLE);
        USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
        break;
    case uart_uart3:
        USART_Cmd(USART3, ENABLE);
        USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
        break;
    default:
        break;
    }
}



void uart_disable(uint8_t uart)
{
    // disable a uart and the uart rxne and txe itnerrupts but leave registers setup

    switch( uart )
    {
    case uart_uart1:
        USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
        USART_ITConfig(USART1, USART_IT_TXE, DISABLE);
        USART_Cmd(USART1, DISABLE);
        break;
    case uart_uart2:
        USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
        USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
        USART_Cmd(USART2, DISABLE);
        break;
    case uart_uart3:
        USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
        USART_ITConfig(USART3, USART_IT_TXE, DISABLE);
        USART_Cmd(USART3, DISABLE);
        break;
    default:
        break;
    }
}



void uart_purgeRxQueue(uint8_t uart)
{
    // purge the rx q for uart

    Queue8* queue   = 0;

    switch( uart )
    {
    case uart_uart1:
        queue = &uart1RxQ;
        break;
    case uart_uart2:
        queue = &uart2RxQ;
        break;
    case uart_uart3:
        queue = &uart3RxQ;
        break;
    default:
        break;
    }

    queue8_purge(queue);
}



int uart_getRxQueueCount(uint8_t uart)
{
    // get the count for the rx q for uart

    int     count;
    Queue8* queue   = 0;

    switch( uart )
    {
    case uart_uart1:
        queue = &uart1RxQ;
        break;
    case uart_uart2:
        queue = &uart2RxQ;
        break;
    case uart_uart3:
        queue = &uart3RxQ;
        break;
    default:
        break;
    }

    count = queue8_getCount(queue);

    return count;
}

//Checks for a particular character within a Queue
//Does not dequeueueue
int uart_queueHasChar(char c, uint8_t uart)
{
    Queue8* queue   = 0;
    switch( uart )
    {
    case uart_uart1:
        queue = &uart1RxQ;
        break;
    case uart_uart2:
        queue = &uart2RxQ;
        break;
    case uart_uart3:
        queue = &uart3RxQ;
        break;
    default:
        break;
    }


	//I think this has a bug where if the queue is empty (ie pRD == pWR) it scans through the whole queue intead of returning immediately.
	//Since the queue may actually contain the desired char in it's currently unused spaces, this returns true, but since the dequeue function
	//doesn't remove them, the while loops that this function is placed in never release.
	if(queue->qCount  == 0)
		return false;

	uint8_t i = 0;
	for(i = queue->readIndex; i != queue->writeIndex; i++)
	{
		i = i % queue->size;
		if(queue->data[i] == c) return true;
	}
	return false;
}

int uart_queueContains(char thingToFind[], int thingToFindLength, uint8_t uart)
{
    Queue8* queue   = 0;
    switch( uart )
    {
    case uart_uart1:
        queue = &uart1RxQ;
        break;
    case uart_uart2:
        queue = &uart2RxQ;
        break;
    case uart_uart3:
        queue = &uart3RxQ;
        break;
    default:
        break;
    }



	uint8_t i = 0;
	int examinationIndex = 0;

	for(i = queue->readIndex; i != queue->writeIndex; i++)
	{
		i = i % queue->size;
		if(queue->data[i] == thingToFind[examinationIndex])
			examinationIndex++;
		else
			examinationIndex = 0;
		if (examinationIndex >= thingToFindLength) //We've matched all the characters we were looking for, return true;
					return true;
	}
	return false;
}

USART_HandleTypeDef usart;
void USART1_IRQHandler(void)
{
    // uart 1 interrupt handler
    // this function name must match that in the startup file

    // handle uart rx char
    //if( USART_GetITStatus(USART1, USART_IT_RXNE) != RESET )
	if(HAL_USART_GetState(usart) != HAL_USART_STATE_RESET) ;
    {
        uint8_t data;

        //data = USART_ReceiveData(USART1) & 0xFF;
        //data = HAL_USART_Receive(usart, data, 1,
        HAL_USART_Receive_IT(usart, data, 1);
        if( queue8_enqueue(&uart1RxQ, data) )
            uart1RxOverflow = true;
    }

    // handle uart tx char empty
    if( USART_GetITStatus(USART1, USART_IT_TXE) != RESET )
    {
        uint8_t data;

        if( !queue8_dequeue(&uart1TxQ, &data) )
        {
            USART_SendData(USART1, data);
        }
        else
        {
            USART_ITConfig(USART1, USART_IT_TXE, DISABLE);
            uart1TxPrimed = false;
        }
    }
}



//void USART2_IRQHandler(void)
//{
//    // uart 2 interrupt handler
//    // this function name must match that in the startup file
//
//    // handle uart rx char
//    if( USART_GetITStatus(USART2, USART_IT_RXNE) != RESET )
//    {
//        uint8_t data;
//
//        data = USART_ReceiveData(USART2) & 0xFF;
//        if( queue8_enqueue(&uart2RxQ, data) )
//            uart2RxOverflow = true;
//    }
//
//    // handle uart tx char empty
//    if( USART_GetITStatus(USART2, USART_IT_TXE) != RESET )
//    {
//        uint8_t data;
//
//        if( !queue8_dequeue(&uart2TxQ, &data) )
//        {
//            USART_SendData(USART2, data);
//        }
//        else
//        {
//            USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
//            uart2TxPrimed = false;
//        }
//    }
//}



//void USART3_IRQHandler(void)
//{
//    // uart 3 interrupt handler
//    // this function name must match that in the startup file
//
//    // handle uart rx char
//    if( USART_GetITStatus(USART3, USART_IT_RXNE) != RESET )
//    {
//        uint8_t data;
//
//        data = USART_ReceiveData(USART3) & 0xFF;
//        if( queue8_enqueue(&uart3RxQ, data) )
//            uart3RxOverflow = true;
//    }
//
//    // handle uart tx char empty
//    if( USART_GetITStatus(USART3, USART_IT_TXE) != RESET )
//    {
//        uint8_t data;
//
//        if( !queue8_dequeue(&uart3TxQ, &data) )
//        {
//            USART_SendData(USART3, data); HAL_USART_Transmit()
//        }
//        else
//        {
//            USART_ITConfig(USART3, USART_IT_TXE, DISABLE);
//            uart3TxPrimed = false;
//        }
//    }
//}
