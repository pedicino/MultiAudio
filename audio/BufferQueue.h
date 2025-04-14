#ifndef BUFFER_QUEUE_H
#define BUFFER_QUEUE_H

#include "../common.h"

#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace audio {

/**
 * Thread-safe queue for audio buffer management.
 * Facilitates thread communication through producer-consumer pattern.
 */
class BufferQueue
{
private:
    //--------------------------------------------------------------------------
    // Internal State
    //--------------------------------------------------------------------------
    std::queue<std::vector<float>> bufferQueue;
    size_t queueCapacity;
    std::mutex mtx;
    std::condition_variable queueHasData;
    std::condition_variable queueHasSpace;
    std::atomic<bool> done;

public:
    //--------------------------------------------------------------------------
    // Lifecycle
    //--------------------------------------------------------------------------
    /**
     * Creates an empty queue with specified capacity.
     * @param capacity Maximum number of buffers that can be held (default: 10)
     */
    explicit BufferQueue(size_t capacity = 10);

    //--------------------------------------------------------------------------
    // Queue Operations
    //--------------------------------------------------------------------------
    /**
     * Adds a new audio buffer to the queue.
     * Blocks if queue is full until space becomes available.
     * @param buffer Audio data to be added
     */
    void push(const std::vector<float>& buffer);

    /**
     * Removes the next audio buffer from the queue.
     * Blocks if queue is empty until data becomes available.
     * @param buffer Reference to store the retrieved data
     * @return true if successful, false if queue is empty and shutdown signaled
     */
    bool pop(std::vector<float>& buffer);

    /**
     * Signals shutdown to all waiting threads.
     * Wakes all blocked producers and consumers.
     */
    void setDone();
};

} // namespace audio

#endif // BUFFER_QUEUE_H