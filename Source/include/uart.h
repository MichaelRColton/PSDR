/* uart.h
 *
 * gunbox uart interface
 *
 * Copyright 2013 RPH Engineering, VPI Engineering
 */

#ifndef UART_H_
    #define UART_H_

    #include <stdint.h>



    enum Uarts
    {
        uart_uartNone,
        uart_uart1,
        uart_uart2,
        uart_uart3,
    };



    int uart_putChar(uint8_t uart, char c);
        // enqueue a char to a uart
        // returns 0 on success

    int uart_getChar(uint8_t uart, char* c);
        // dequeue a char from a uart and put it in *c
        // returns 0 on success

    int uart_write(uint8_t uart, uint8_t* buffer, uint8_t size);
        // write a buffer of data to a uart
        // return the number of bytes written

    int uart_read(uint8_t uart, uint8_t* buffer, uint8_t size);
        // try to read size bytes of data into buffer from a uart
        // return the number of bytes read

    void uart_init(uint8_t uart);
        // initialize specified uart

    void uart_deinit(uint8_t uart);
        // deinitialize specified uart

    void uart_enable(uint8_t uart);
        // enable a uart, enables rxne interrupt, does not set other config info

    void uart_disable(uint8_t uart);
        // disable a uart and the uart rxne and txe itnerrupts but leave registers setup

    void uart_purgeRxQueue(uint8_t uart);
        // purge the rx q for uart

    int uart_getRxQueueCount(uint8_t uart);
        // get the count for the rx q for uart

    int uart_queueHasChar(char c, uint8_t uart);
    	// return true if the rx q contains c

    int uart_queueContains(char thingToFind[], int thingToFindLength, uint8_t);


#endif /* UART_H_ */
