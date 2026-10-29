// Compile the real StringTable.cpp/.h, copied byte-for-byte by the test driver.
// Only engine logging/fixed strings/serialization are stubbed at their boundary.
#include "StringTable.h"
#define NOMINMAX
#include <windows.h>
#include <sanitizer/asan_interface.h>
#include <cstdlib>
#include <cstdio>
#include <new>
#include <type_traits>
#include <thread>
#include <array>

namespace {
bool tracking = false;
struct Allocation { void* pointer; bool freed; } tableAllocations[128]{};
std::size_t allocationCount = 0;
bool holderDestroyed = false;
void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", message);
        std::abort();
    }
}
}

// Track only allocations made while the actual table facade is constructed.
// This includes its shared Storage allocation and the STL's initial buckets.
void* operator new(std::size_t size) {
    void* p = std::malloc(size ? size : 1);
    if (!p) throw std::bad_alloc();
    if (tracking) {
        check(allocationCount < 128, "construction allocation tracking capacity");
        tableAllocations[allocationCount++] = {p, false};
    }
    return p;
}
void operator delete(void* p) noexcept {
    for (std::size_t i = 0; i < allocationCount; ++i)
        if (tableAllocations[i].pointer == p) tableAllocations[i].freed = true;
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

// Deliberately retained longer than the last strong string and the facade.
std::weak_ptr<SKEEFixedString> survivingWeakObserver;
struct FinalCheck {
    ~FinalCheck() {
        check(holderDestroyed, "late morph holder destructor executed");
        check(survivingWeakObserver.expired(), "last strong string released");
        check(allocationCount > 0, "actual table construction allocations captured");
        for (std::size_t i = 0; i < allocationCount; ++i)
            check(tableAllocations[i].freed, "table Storage/buckets freed before weak observer destruction");
        std::puts("normal CRT teardown: all tracked storage freed; weak observer still exists");
    }
} finalCheck;

struct LateMorphHolder {
    // Real BodyMorphData ownership shape, without the unrelated game interfaces.
    std::unordered_map<std::uint32_t,
        std::unordered_map<StringTableItem, std::unordered_map<StringTableItem, float>>> actors;
    ~LateMorphHolder() {
        actors.clear();
        holderDestroyed = true;
    }
} lateMorphs;

struct PoisonAfterTable {
    ~PoisonAfterTable();
} poisonAfterTable;
struct TrackConstructionStart {
    TrackConstructionStart() { tracking = true; }
} trackConstructionStart;
StringTable g_stringTable;
struct TrackConstructionEnd {
    TrackConstructionEnd() { tracking = false; }
} trackConstructionEnd;

PoisonAfterTable::~PoisonAfterTable() {
    // Instrumentation only: the facade's ordinary destructor has already run.
    // Original DeleteStringEntry touches it; the fixed deleter touches only its
    // retained Storage. No product source or destructor is bypassed/modified.
    __asan_poison_memory_region(&g_stringTable, sizeof(g_stringTable));
}

static_assert(!std::is_copy_constructible_v<StringTable>);
static_assert(!std::is_move_constructible_v<StringTable>);

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    const std::string mode = argc > 1 ? argv[1] : "teardown";
    if (mode == "operations") {
        auto one = g_stringTable.GetString("Waist");
        auto duplicate = g_stringTable.GetString("wAiSt");
        check(one == duplicate, "case-insensitive live interning");
        check(g_stringTable.GetStringID(one) == 0, "actual table index");
        SKSE::SerializationInterface stream;
        g_stringTable.Save(&stream, StringTable::kSerializationVersion);
        check(stream.bytes.size() > 4, "real production serialization writes data");
        StringIdMap loaded;
        check(!g_stringTable.Load(&stream, StringTable::kSerializationVersion, loaded), "production load succeeds");
        check(loaded.at(0) == one, "production load preserves intern identity");
        loaded.clear(); duplicate.reset();
        g_stringTable.Revert();
        auto replacement = g_stringTable.GetString("Waist");
        check(replacement != one, "revert creates a new intern entry");
        one.reset();
        check(g_stringTable.GetString("WAIST") == replacement, "old callback cannot erase live replacement");
        replacement.reset();
        auto released = g_stringTable.GetString("Released");
        std::weak_ptr<SKEEFixedString> weak = released;
        released.reset();
        check(weak.expired(), "ordinary last-owner release");
    } else if (mode == "parallel") {
        std::array<std::thread, 8> workers;
        for (auto& worker : workers) worker = std::thread([] {
            for (int i = 0; i < 1000; ++i) {
                auto key = g_stringTable.GetString("Parallel-key");
                check(key && key->length() == 12, "concurrent interning and release");
            }
        });
        for (auto& worker : workers) worker.join();
    } else {
        check(mode == "teardown", "known test mode");
    }
    for (std::uint32_t actor = 0; actor < 6; ++actor) {
        for (const auto* name : {"Waist", "Breasts", "BodyWeight"}) {
            auto morph = g_stringTable.GetString(name);
            auto key = g_stringTable.GetString("OBody-provider");
            lateMorphs.actors[actor][morph][key] = 0.25f;
            survivingWeakObserver = key;
        }
    }
    std::printf("%s: populated six actor maps; normal return (no quick_exit/termination)\n", mode.c_str());
    return 0;
}
