#pragma once
#include <mutex>

namespace SKEE
{
    // The allocation/free hooks and this operation must use the same mutex.
    template <class Mutex, class TrackedSet, class Retain>
    bool RetainTrackedDynamicData(void* data, Mutex& mutex, const TrackedSet& tracked, Retain retain)
    {
        if (!data) return false;
        std::scoped_lock lock(mutex);
        if (tracked.find(data) == tracked.end()) return false;
        retain(data);
        return true;
    }
}
