#ifndef CHIAKI_PY_TIMER_H
#define CHIAKI_PY_TIMER_H


#include <time.h>
#include <ctime>
#include <iostream>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstddef>

class Timer
{
public:
    Timer() : running(false), intervalMs(0) {} // Default interval: 1000ms

    ~Timer()
    {
        stop();
    }

    void setInterval(int ms)
    {
        intervalMs = ms;
    }

    void start(std::function<void()> callback)
    {
        stop(); // Ensure no previous timer is running
        running = true;
        worker = std::thread([this, callback]()
                             {
            while (running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
                if (running && callback) callback();
            } });
    }

    void stop()
    {
        running = false;
        if (worker.joinable())
            worker.join();
    }

    static void singleShot(int delayMs, std::function<void()> callback)
    {
        std::thread([delayMs, callback]()
                    {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            callback(); })
            .detach(); // Detach to run independently
    }

private:
    std::atomic<bool> running;
    std::thread worker;
    int intervalMs;
};

#endif // CHIAKI_PY_TIMER_H