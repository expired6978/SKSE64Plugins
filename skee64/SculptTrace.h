#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

// Explicitly armed, bounded aggregate diagnostics. No coordinates, vertex data,
// per-event logging or allocations in the pointer path. Shared with API probes.
namespace SKEE::SculptTrace
{
    enum class Event : unsigned { Hover, Begin, Paint, End, ScenePick, MeshPick, Upload, RejectedPaint,
        PointerDown, PointerUp, EndCallback, BrushChange, StrokeCommit, UndoPush,
        HistoryQueued, HistoryRun, HistoryStale, HistoryNoMovie, HistoryInvoked, HistoryInvokeFailed, HistoryNoTask, Count };
    inline constexpr std::array names{"hover", "begin", "paint", "end", "scenePick", "meshPick", "upload", "rejectedPaint",
        "pointerDown", "pointerUp", "endCallback", "brushChange", "strokeCommit", "undoPush",
        "historyQueued", "historyRun", "historyStale", "historyNoMovie", "historyInvoked", "historyInvokeFailed", "historyNoTask"};
    struct Sample { std::uint64_t calls{}, nanoseconds{}, maximumNanoseconds{}, units{}; };
    using Clock = std::chrono::steady_clock;
    struct Snapshot { bool active{}; std::uint64_t generation{}; std::array<Sample, static_cast<unsigned>(Event::Count)> samples{}; };
    inline std::mutex mutex;
    inline std::atomic<bool> armed{false};
    inline Clock::time_point deadline{};
    inline Snapshot state;
    inline void Start()
    {
        std::lock_guard guard(mutex);
        const auto generation = state.generation + 1;
        state = {}; state.generation = generation;
        deadline = Clock::now() + std::chrono::seconds(60);
        armed.store(true, std::memory_order_relaxed);
    }
    inline Snapshot Read(bool stop = false)
    {
        std::lock_guard guard(mutex);
        if (stop || Clock::now() >= deadline) armed.store(false, std::memory_order_relaxed);
        auto result = state; result.active = armed.load(std::memory_order_relaxed); return result;
    }
    // Count lifecycle boundaries without allocating or timing their body.
    inline void Count(Event event, std::uint64_t units = 0)
    {
        if (!armed.load(std::memory_order_relaxed)) return;
        std::lock_guard guard(mutex);
        if (Clock::now() >= deadline) { armed.store(false, std::memory_order_relaxed); return; }
        auto& sample = state.samples[static_cast<unsigned>(event)];
        ++sample.calls; sample.units += units;
    }
    class Scope
    {
        Event event;
        std::uint64_t generation{}, units;
        Clock::time_point start{};
    public:
        explicit Scope(Event e, std::uint64_t u = 0) : event(e), units(u)
        {
            if (!armed.load(std::memory_order_relaxed)) return;
            std::lock_guard guard(mutex);
            start = Clock::now();
            if (start >= deadline) { armed.store(false, std::memory_order_relaxed); return; }
            generation = state.generation;
        }
        ~Scope()
        {
            if (!generation) return;
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
            std::lock_guard guard(mutex);
            if (generation != state.generation) return;
            auto& sample = state.samples[static_cast<unsigned>(event)];
            ++sample.calls; sample.nanoseconds += elapsed; sample.units += units;
            sample.maximumNanoseconds = (std::max)(sample.maximumNanoseconds, static_cast<std::uint64_t>(elapsed));
        }
    };
}
