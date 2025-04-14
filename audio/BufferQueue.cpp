#include "BufferQueue.h"

namespace audio {

//--------------------------------------------------------------------------
// Lifecycle
//--------------------------------------------------------------------------

BufferQueue::BufferQueue(size_t capacity)
    : queueCapacity(capacity), done(false)
{
}

//--------------------------------------------------------------------------
// Queue Operations
//--------------------------------------------------------------------------

void BufferQueue::push(const std::vector<float>& buffer)
{
    std::unique_lock<std::mutex> lock(mtx);

    queueHasSpace.wait(lock, [this]() {
        return bufferQueue.size() < queueCapacity || done.load();
    });

    if (done.load())
    {
        return;
    }

    bufferQueue.push(buffer);
    lock.unlock();
    queueHasData.notify_one();
}

bool BufferQueue::pop(std::vector<float>& buffer)
{
    std::unique_lock<std::mutex> lock(mtx);

    queueHasData.wait(lock, [this]() {
        return !bufferQueue.empty() || done.load();
    });

    if (bufferQueue.empty() && done.load())
    {
        return false;
    }

    buffer = bufferQueue.front();
    bufferQueue.pop();

    lock.unlock();
    queueHasSpace.notify_one();

    return true;
}

//--------------------------------------------------------------------------
// Shutdown
//--------------------------------------------------------------------------

void BufferQueue::setDone()
{
    done.store(true);
    queueHasData.notify_all();
    queueHasSpace.notify_all();
}

} // namespace audio