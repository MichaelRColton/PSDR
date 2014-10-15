/* queue.c
 *
 * queue / circular buffer
 *
 * Copyright 2013 RPH Engineering, VPI Engineering
 */

#include "queue.h"



void queue8_init(Queue8* queue, uint8_t* dataPtr, uint8_t size)
{
    // initialize a Queue8

    queue->qCount       = 0;
    queue->size         = size;
    queue->readIndex    = 0;
    queue->writeIndex   = 0;
    queue->data         = dataPtr;
}



bool queue8_isQFull(Queue8* queue)
{
    // return true if the Q is full, else false

    return queue->qCount == queue->size;
}



bool queue8_isQEmpty(Queue8* queue)
{
    // return true if the Q is empty, else false

    return queue->qCount == 0;
}



int queue8_enqueue(Queue8* queue, uint8_t data)
{
    // enqueue data on queue
    // returns 0 on success, negative o/w

    int status = 0;

    //if( queue->qCount == queue->size )
    if(    (queue->readIndex == 0 && queue->writeIndex == queue->size - 1)
        || (queue->writeIndex + 1) == queue->readIndex
      )
    {
        status = -1;
    }
    else
    {
        queue->data[queue->writeIndex] = data;
        if( ++queue->writeIndex >= queue->size )
            queue->writeIndex = 0;

        //++queue->qCount;
    }

    //Am I wrong that nothing was ever updating the count?
    queue->qCount = queue8_getCount(queue);

    return status;
}



int queue8_dequeue(Queue8* queue, uint8_t* data)
{
    // pull a byte of data off queue and put it in *data
    // returns 0 on success, negative o/w

    int status = 0;

    //if( queue->qCount == 0 )
    if( queue->readIndex == queue->writeIndex )
    {
        status = -1;
    }
    else
    {
        *data = queue->data[queue->readIndex];
        if( ++queue->readIndex >= queue->size )
            queue->readIndex = 0;

        //--queue->qCount;
    }

    //Am I wrong that nothing was ever updating the count?
    queue->qCount = queue8_getCount(queue);

    return status;
}



void queue8_purge(Queue8* queue)
{
    // purge a q

    queue->qCount       = 0;
    queue->readIndex    = 0;
    queue->writeIndex   = 0;
}



int queue8_getCount(Queue8* queue)
{
    // get the count of entries in the q

    int count;

    if( queue->readIndex <= queue->writeIndex )
        count = queue->writeIndex - queue->readIndex;
    else
        count = queue->size - queue->readIndex + queue->writeIndex;

    return count;
}
