#ifndef CHIAKI_PY_ELAPSED_TIMER_H
#define CHIAKI_PY_ELAPSED_TIMER_H

#include <time.h>
#include <ctime>
#include <iostream>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstddef>

class ElapsedTimer
{
public:
    ElapsedTimer() : started(false) {}

    void start()
    {
        start_time = std::chrono::high_resolution_clock::now();
        started = true;
    }

    int64_t elapsed() const
    {
        if (!started)
            return 0;
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
    }

    bool isValid() const
    {
        return started;
    }

    void invalidate()
    {
        started = false;
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    bool started;
};

#endif // CHIAKI_PY_ELAPSED_TIMER_H