#ifndef CHIAKI_PY_EVENT_QUEUE_H
#define CHIAKI_PY_EVENT_QUEUE_H

#include <queue>
#include <mutex>
#include <functional>
#include <condition_variable>

class EventQueue
{
public:
    void post(std::function<void()> func)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(func);
        cond_var_.notify_one();
    }

    void process()
    {
        while (true)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cond_var_.wait(lock, [this]
                               { return !queue_.empty(); });
                task = queue_.front();
                queue_.pop();
            }
            task(); // execute
        }
    }

private:
    std::queue<std::function<void()>> queue_;
    std::mutex mutex_;
    std::condition_variable cond_var_;
};

#endif // CHIAKI_PY_EVENT_QUEUE_H