#include "AdjustedDynamicData.h"
#include <future>
#include <stdexcept>
#include <unordered_set>
#include <iostream>

int main()
{
    std::recursive_mutex mutex;
    int buffer = 0, references = 1;
    std::unordered_set<void*> tracked{&buffer};
    bool retained = false;
    if (SKEE::RetainTrackedDynamicData(nullptr, mutex, tracked, [&](void*) { retained = true; }) || retained)
        throw std::runtime_error("Null buffer retained");
    int unknown;
    if (SKEE::RetainTrackedDynamicData(&unknown, mutex, tracked, [&](void*) { retained = true; }) || retained)
        throw std::runtime_error("Untracked buffer retained");
    // Attempt the free hook's critical section while inside the retain callback.
    // A second thread cannot acquire it until the retain operation returns.
    if (!SKEE::RetainTrackedDynamicData(&buffer, mutex, tracked, [&](void*) {
        auto freeAttempt = std::async(std::launch::async, [&] {
            if (!mutex.try_lock()) return false;
            mutex.unlock(); return true;
        });
        if (freeAttempt.get()) throw std::runtime_error("Retain callback was not locked");
        ++references;
    })) throw std::runtime_error("Tracked buffer not retained");
    {
        std::scoped_lock lock(mutex);
        if (--references != 1) throw std::runtime_error("Original free lost retained reference");
        if (--references != 0) throw std::runtime_error("Retained reference leaked");
        tracked.erase(&buffer);
    }
    if (SKEE::RetainTrackedDynamicData(&buffer, mutex, tracked, [&](void*) { ++references; }))
        throw std::runtime_error("Freed buffer retained");
    std::cout << "Tracked overlay buffer: null/untracked rejection, locked retain and release passed\n";
}
