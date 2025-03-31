#ifndef BUFFER_QUEUE_H
#define BUFFER_QUEUE_H

#include "../common.h"

/*
Thread-safe queue of audio buffers
    - Passes audio between threads
    - Producers add audio data to the buffer
    - Consumers receive data for further processing
*/
class BufferQueue 
{
private:
    queue<vector<float>> bufferQueue;
    size_t queueCapacity;
    mutex mtx;
    condition_variable queueHasData;
    condition_variable queueHasSpace;
    atomic<bool> done;

public:
    // Constructor - creates an EMPTY queue that can hold up to "capacity" buffers
    BufferQueue(size_t capacity = 10);
    
    // Producer - add a new audio buffer to the queue
    void push(const vector<float>& buffer);
    
    // Consumer - Remove the next audio buffer from the queue and return it
    bool pop(vector<float>& buffer);
    
    // Shutdown - Signal all waiting threads
    void setDone();
};

#endif // BUFFER_QUEUE_H