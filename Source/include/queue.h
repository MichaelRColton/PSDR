/* queue.h
 *
 * queue/circular buffer
 *
 * Copyright 2013 RPH Engineering, VPI Engineering
 */

#ifndef QUEUE_H_
    #define QUEUE_H_

    #include <stdint.h>
    #include <stdbool.h>



    // a Q that can store up to 256 bytes
    typedef struct _Queue8
    {
        uint8_t     qCount;             // number of bytes in the Q
        uint8_t     size;               // size of q, in bytes
        uint8_t     readIndex;
        uint8_t     writeIndex;
        uint8_t*    data;
    } Queue8;



    void queue8_init(Queue8* queue, uint8_t* dataPtr, uint8_t size);
        // initialize a Queue8

    bool queue8_isQFull(Queue8* queue);
        // return true if the Q is full, else false

    bool queue8_isQEmpty(Queue8* queue);
        // return true if the Q is empty, else false

    int queue8_enqueue(Queue8* queue, uint8_t data);
        // enqueue data on queue
        // returns 0 on success, negative o/w

    int queue8_dequeue(Queue8* queue, uint8_t* data);
        // pull a byte of data off queue and put it in *data
        // returns 0 on success, negative o/w

    void queue8_purge(Queue8* queue);
        // purge a q

    int queue8_getCount(Queue8* queue);
        // get the count of entries in the q


#endif /* QUEUE_H_ */
