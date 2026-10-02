#pragma once

#include <cstdint>
#include <cstring>
#include <mutex>
#include <utility>

namespace SKEE
{
    // Adapts the engine's Lock/Unlock interface for scope-bound guards.
    template <class Lock>
    struct DynamicDataMutex
    {
        Lock& value;
        void lock() { value.Lock(); }
        void unlock() { value.Unlock(); }
    };

    struct DynamicDataSnapshot
    {
        void* data{};
        std::uint32_t size{};
        std::uint32_t frameCount{};
        std::uint32_t unk178{};
    };

    // One exact retained/copied buffer, released unless transferred to geometry.
    // Acquisition requires a live source owner and its data lock, not a borrowed
    // address obtained before the lock.
    class DynamicDataLease
    {
    public:
        using Release = void (*)(void*);
        DynamicDataLease() = default;
        DynamicDataLease(DynamicDataSnapshot snapshot, Release release) noexcept :
            m_snapshot(snapshot), m_release(release) {}
        DynamicDataLease(const DynamicDataLease&) = delete;
        DynamicDataLease& operator=(const DynamicDataLease&) = delete;
        DynamicDataLease(DynamicDataLease&& other) noexcept :
            m_snapshot(std::exchange(other.m_snapshot, {})),
            m_release(std::exchange(other.m_release, nullptr)) {}
        ~DynamicDataLease() { if (m_snapshot.data) m_release(m_snapshot.data); }
        explicit operator bool() const noexcept { return m_snapshot.data != nullptr; }
        const DynamicDataSnapshot& Get() const noexcept { return m_snapshot; }
        DynamicDataSnapshot Transfer() noexcept { return std::exchange(m_snapshot, {}); }

    private:
        DynamicDataSnapshot m_snapshot{};
        Release m_release{};
    };

    // Lock order: source data lock, then allocation registry. The source owner
    // remains live; writers replacing storage must honor its data lock. Copy
    // finishes under both guards. Untracked alone never proves source lifetime.
    template <class SourceMutex, class RegistryMutex, class TrackedSet,
        class Read, class Retain, class Allocate>
    DynamicDataLease AcquireDynamicData(SourceMutex& sourceMutex,
        RegistryMutex& registryMutex, const TrackedSet& tracked, bool share,
        Read read, Retain retain, Allocate allocate, DynamicDataLease::Release release)
    {
        std::scoped_lock sourceLock(sourceMutex);
        std::scoped_lock registryLock(registryMutex);
        auto snapshot = read();
        if (!snapshot.data || snapshot.size == 0) return {};
        if (share && tracked.find(snapshot.data) != tracked.end()) {
            retain(snapshot.data);
            return {snapshot, release};
        }
        void* copy = allocate(snapshot.size);
        if (!copy) return {};
        std::memcpy(copy, snapshot.data, snapshot.size);
        snapshot.data = copy;
        return {snapshot, release};
    }

    // Caller holds target's data lock. Failed acquisition leaves every field
    // unchanged. Release superseded storage after successful transfer only.
    template <class Runtime, class Release>
    bool ReplaceDynamicData(Runtime& target, DynamicDataLease& lease, Release release)
    {
        if (!lease) return false;
        const auto acquired = lease.Transfer();
        void* previous = target.dynamicData;
        target.dynamicData = acquired.data;
        target.dataSize = acquired.size;
        target.frameCount = acquired.frameCount;
        target.unk178 = acquired.unk178;
        target.unk17C = 0;
        if (previous) release(previous);
        return true;
    }
}
