#include "AdjustedDynamicData.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace
{
    unsigned checks = 0;
    std::unordered_map<void*, unsigned> references;
    unsigned released = 0;
    void Check(bool value)
    {
        ++checks;
        if (!value) throw std::runtime_error("Dynamic data transaction assertion failed");
    }
    void Release(void* data)
    {
        auto found = references.find(data);
        Check(found != references.end() && found->second > 0);
        if (--found->second == 0) references.erase(found);
        ++released;
    }
    struct Runtime
    {
        void* dynamicData{};
        std::uint32_t dataSize{}, frameCount{}, unk178{}, unk17C{};
    };
    struct CheckedMutex
    {
        bool held = false;
        CheckedMutex* prerequisite = nullptr;
        void lock() { Check(!held && (!prerequisite || prerequisite->held)); held = true; }
        void unlock() { Check(held); held = false; }
    };
}

int main()
{
    // Both mismatch directions reject before common publication. This is the
    // exact admission predicate wired ahead of fields/callbacks in InstallOverlay;
    // mock counters are not execution of the native shader/attachment path.
    for (bool sourceDynamic : {false, true}) {
        for (bool targetDynamic : {false, true}) {
            for (bool existing : {false, true}) {
                unsigned callbacks = 0, attachments = 0;
                Runtime target{nullptr, 5, 6, 7, 8};
                if (SKEE::OverlayGeometryKindsMatch(sourceDynamic, targetDynamic)) {
                    target.dataSize = 12;
                    ++callbacks;
                    if (!existing) ++attachments;
                }
                const bool matches = sourceDynamic == targetDynamic;
                Check(target.dataSize == (matches ? 12u : 5u));
                Check(target.frameCount == 6 && target.unk178 == 7 && target.unk17C == 8);
                Check(callbacks == (matches ? 1u : 0u));
                Check(attachments == (matches && !existing ? 1u : 0u));
            }
        }
    }

    // Fault injection calls the production allocation/registration transaction.
    // The fake allocator returns a sufficiently sized local block even at the
    // arithmetic boundary; no SIZE_MAX allocation is attempted.
    alignas(std::uintptr_t) std::array<std::byte, 32> block{};
    std::unordered_set<void*> allocationRegistry;
    unsigned mallocCalls = 0, freeCalls = 0, publishCalls = 0;
    std::size_t requested = 0;
    enum class Failure { None, NullAllocate, ThrowAllocate, ThrowInitialize, ThrowPublish, RejectPublish };
    Failure failure = Failure::None;
    auto allocateAdjusted = [&](std::size_t size) {
        return SKEE::AllocateAdjustedData(size,
            [&](std::size_t total) -> void* {
                ++mallocCalls; requested = total;
                if (failure == Failure::ThrowAllocate) throw std::bad_alloc();
                return failure == Failure::NullAllocate ? nullptr : block.data();
            },
            [&](void* base) {
                if (failure == Failure::ThrowInitialize) throw std::bad_alloc();
                const std::array<std::uintptr_t, 2> header{1, 0};
                std::memcpy(base, header.data(), SKEE::kAdjustedDataHeaderSize);
            },
            [&](void* adjusted) {
                ++publishCalls;
                if (failure == Failure::ThrowPublish) throw std::bad_alloc();
                if (failure == Failure::RejectPublish) return false;
                return allocationRegistry.insert(adjusted).second;
            },
            [&](void* base) noexcept { if (base == block.data()) ++freeCalls; });
    };
    const auto maxSize = (std::numeric_limits<std::size_t>::max)();
    Check(!allocateAdjusted(maxSize) && mallocCalls == 0 && publishCalls == 0 && freeCalls == 0);
    Check(!allocateAdjusted(maxSize - SKEE::kAdjustedDataHeaderSize + 1) && mallocCalls == 0);
    for (auto injected : {Failure::NullAllocate, Failure::ThrowAllocate, Failure::ThrowInitialize,
             Failure::ThrowPublish, Failure::RejectPublish}) {
        failure = injected;
        auto beforeFree = freeCalls;
        Check(!allocateAdjusted(4));
        Check(allocationRegistry.empty());
        Check(freeCalls == beforeFree + (injected == Failure::NullAllocate || injected == Failure::ThrowAllocate ? 0u : 1u));
    }
    failure = Failure::None;
    void* adjusted = allocateAdjusted(maxSize - SKEE::kAdjustedDataHeaderSize);
    Check(adjusted == block.data() + SKEE::kAdjustedDataHeaderSize && requested == maxSize);
    Check(allocationRegistry.contains(adjusted));
    std::array<std::uintptr_t, 2> header{};
    std::memcpy(header.data(), block.data(), SKEE::kAdjustedDataHeaderSize);
    Check(header[0] == 1 && header[1] == 0);
    // Fake final tracked release. The native InterlockedDecrement/NiFree hook
    // itself still needs separately qualified engine tests.
    Check(allocationRegistry.erase(adjusted) == 1);
    ++freeCalls;
    Check(allocationRegistry.empty());
    adjusted = allocateAdjusted(4);
    Check(adjusted == block.data() + SKEE::kAdjustedDataHeaderSize && requested == 20);
    Check(allocationRegistry.erase(adjusted) == 1);
    ++freeCalls;

    std::array<unsigned char, 4> original{1, 2, 3, 4}, replacement{5, 6, 7, 8}, copy{};
    std::unordered_set<void*> tracked{original.data(), replacement.data()};
    CheckedMutex sourceLock, registryLock;
    registryLock.prerequisite = &sourceLock;
    Runtime source{original.data(), 4, 3, 9, 1};
    unsigned reads = 0, allocations = 0, retains = 0;
    bool failAllocation = false;
    auto acquire = [&](bool share) {
        return SKEE::AcquireDynamicData(sourceLock, registryLock, tracked, share,
            [&] {
                Check(sourceLock.held && registryLock.held);
                ++reads;
                return SKEE::DynamicDataSnapshot{source.dynamicData, source.dataSize, source.frameCount, source.unk178};
            },
            [&](void* data) {
                Check(sourceLock.held && registryLock.held);
                ++references.at(data); ++retains;
            },
            [&](std::uint32_t size) -> void* {
                Check(sourceLock.held && registryLock.held && size == copy.size());
                ++allocations;
                if (failAllocation) return nullptr;
                references[copy.data()] = 1;
                return copy.data();
            }, Release);
    };

    // Exact identity survives a source replacement AFTER successful acquisition.
    references[original.data()] = 1;
    references[replacement.data()] = 2; // Source replacement and old target own one each.
    {
        auto acquired = acquire(true);
        Check(acquired && acquired.Get().data == original.data());
        Check(reads == 1 && retains == 1 && allocations == 0);
        source.dynamicData = replacement.data();
        Runtime target{replacement.data(), 4, 100, 100, 100};
        Check(SKEE::ReplaceDynamicData(target, acquired, Release));
        Check(!acquired && target.dynamicData == original.data());
        Check(target.dataSize == 4 && target.frameCount == 3 && target.unk178 == 9 && target.unk17C == 0);
        Check(references.at(replacement.data()) == 1);
        Release(original.data()); // Source releases old storage after replacement.
        Check(references.at(original.data()) == 1);
        Release(target.dynamicData); // Overlay final release.
    }
    Release(replacement.data()); // Source's replacement reference.
    Check(references.empty());

    // A replacement before acquisition is captured afresh under both locks;
    // there is no borrowed argument p from before the source guard to reuse.
    references[replacement.data()] = 1;
    {
        auto acquired = acquire(true);
        Check(acquired.Get().data == replacement.data());
        Check(references.at(replacement.data()) == 2);
    } // Abandoned retained lease balances its reference.
    Check(references.at(replacement.data()) == 1);

    // Same-pointer tracked reinstall retains once then releases the old target's
    // reference, so the two live owners still have exactly two references.
    {
        ++references.at(replacement.data()); // Existing overlay owner.
        Runtime target{replacement.data(), 4, 0, 0, 0};
        auto acquired = acquire(true);
        Check(references.at(replacement.data()) == 3);
        Check(SKEE::ReplaceDynamicData(target, acquired, Release));
        Check(!acquired && target.dynamicData == replacement.data());
        Check(references.at(replacement.data()) == 2);
        Release(target.dynamicData);
        Check(references.at(replacement.data()) == 1);
    }

    // Live untracked and disabled-sharing copies own distinct exact storage.
    for (bool share : {true, false}) {
        if (share) tracked.erase(replacement.data());
        else tracked.insert(replacement.data());
        auto acquired = acquire(share);
        Check(acquired && acquired.Get().data == copy.data());
        Check(copy == replacement && source.dynamicData == replacement.data());
        auto moved = std::move(acquired);
        Check(!acquired && moved.Get().data == copy.data());
    }
    Check(!references.contains(copy.data()));

    // Allocation failure leaves new AND existing destination metadata/storage
    // unchanged. InstallOverlay returns before any callback/attachment on failure.
    failAllocation = true;
    for (void* previous : {static_cast<void*>(nullptr), static_cast<void*>(replacement.data())}) {
        Runtime target{previous, 4, 17, 18, 19};
        const auto beforeRelease = released;
        auto acquired = acquire(false);
        Check(!acquired && !SKEE::ReplaceDynamicData(target, acquired, Release));
        Check(target.dynamicData == previous && target.dataSize == 4 && target.frameCount == 17 && target.unk178 == 18 && target.unk17C == 19);
        Check(released == beforeRelease && references.at(replacement.data()) == 1);
    }

    // Invalid positive-size null source and zero-size non-null source neither
    // retain, allocate, read payload nor publish geometry.
    const auto beforeAllocations = allocations, beforeRetains = retains;
    source.dynamicData = nullptr;
    Check(!acquire(true));
    source.dynamicData = replacement.data(); source.dataSize = 0;
    Check(!acquire(true));
    Check(allocations == beforeAllocations && retains == beforeRetains);
    Release(replacement.data());
    Check(references.empty() && !sourceLock.held && !registryLock.held);
    std::cout << "Adjusted dynamic data production helper checks: " << checks << " passed\n";
}
