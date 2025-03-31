#include "BufferQueue.h"

BufferQueue::BufferQueue(size_t capacity) 
    : queueCapacity(capacity), done(false)
{
    // Empty constructor body as initialization is done in the initializer list
}

void BufferQueue::push(const vector<float>& buffer) 
{
    // Lock to prevent other threads from accessing the queue
    unique_lock<mutex> lock(mtx);

    // Wait until there's space in the queue OR shut down
    // Lambda function performs repeated checks
    queueHasSpace.wait(lock, [this]() { return bufferQueue.size() < queueCapacity || done.load(); });
    
    // If shutting down, stop and don't add more data
    if (done.load())
    {
        return;
    }

    // Add the new buffer to the queue
    bufferQueue.push(buffer);

    // Release lock
    lock.unlock();

    // Notify one waiting consumer thread that data is ready
    queueHasData.notify_one();
}

bool BufferQueue::pop(vector<float>& buffer) 
{
    // Lock to prevent other threads from accessing the queue
    unique_lock<mutex> lock(mtx);

    // Wait until audio data is available in the queue OR shut down
    queueHasData.wait(lock, [this]() { return !bufferQueue.empty() || done.load(); });
    
    // No data and shutdown triggered
    if (bufferQueue.empty() && done.load())
    {
        return false; // Process failed :(
    }
    
    // Save the buffer at the front of the queue
    buffer = bufferQueue.front();

    // Remove that buffer
    bufferQueue.pop();

    // Release lock
    lock.unlock();

    // Wake up one waiting producer thread
    queueHasSpace.notify_one();

    return true; // Success :)
}

void BufferQueue::setDone() 
{
    // Atomic "done" flag set to true
    done.store(true);

    // Wake up all waiting consumers
    queueHasData.notify_all();

    // Wake up all waiting producers
    queueHasSpace.notify_all();
}