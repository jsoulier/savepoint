#pragma once

#include <mutex>
#include <thread>

class SavepointMutex
{
public:
    SavepointMutex();
    void SetEnabled(bool enabled);
    void lock();
    void unlock();

private:
    bool Enabled;
    std::recursive_mutex Mutex;
#ifndef NDEBUG
    std::thread::id ThreadID;
#endif
};
