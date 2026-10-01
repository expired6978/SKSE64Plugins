// SPDX-License-Identifier: GPL-3.0-or-later
#include <RE/I/Inventory3DManager.h>
#include "InventoryPreviewPolicy.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace
{
    void Check(bool result, const char* message)
    { if (!result) throw std::runtime_error(message); }

    // Populate the real CommonLib array ABI without calling Skyrim's allocator.
    // Restore its empty inline header before destruction, including on failure.
    struct Fixture
    {
        RE::Inventory3DManager::VR_RUNTIME_DATA runtime{};
        void Word(std::size_t offset, std::uint32_t value)
        { std::memcpy(reinterpret_cast<std::byte*>(&runtime) + offset, &value, sizeof(value)); }
        void Header(std::uint32_t count, std::uint32_t capacity = 7, bool local = true)
        {
            Word(0, capacity | (local ? 0x80000000U : 0U));
            Word(0x200, count);
        }
        ~Fixture() { Header(0, 0); }
    };
}

int main()
{
    try {
        using SKEE::InventoryPreview::FindLoadedModel;
        static_assert(sizeof(RE::LoadedInventoryModel) == 0x20);
        static_assert(sizeof(RE::LoadedInventoryModelVR) == 0x48);
        static_assert(sizeof(RE::Inventory3DManager::VR_RUNTIME_DATA) == 0x208);
        static_assert(0x58 + 0x200 == 0x258);

        Fixture fixture;
        auto& models = fixture.runtime.loadedModels;
        auto* first = reinterpret_cast<RE::TESForm*>(0x1000);
        auto* target = reinterpret_cast<RE::TESForm*>(0x2000);
        fixture.Header(2);
        models[0].itemBase = first;
        models[1].itemBase = target;
        Check(reinterpret_cast<std::byte*>(&models[1]) - reinterpret_cast<std::byte*>(&models[0]) == 0x48,
            "VR preview stride is not 0x48");
        // The previous flat layout read its count here, inside VR entry 3.
        fixture.Word(0xE8, 0xFFFFFFFFU);
        Check(models.size() == 2, "VR count came from the flat layout");
        Check(FindLoadedModel(models, target) == &models[1], "Second VR preview not found");
        Check(!FindLoadedModel(models, reinterpret_cast<RE::TESForm*>(0x3000)), "Absent form matched");
        Check(!FindLoadedModel(models, static_cast<RE::TESForm*>(nullptr)), "Null form matched");
        Check(!models[1].spModel, "Null preview fixture not preserved");

        fixture.Header(0);
        Check(!FindLoadedModel(models, target), "Empty list indexed");
        fixture.Header(0xFFFFFFFFU);
        Check(!FindLoadedModel(models, target), "Malformed count not rejected");
        fixture.Header(8);
        Check(!FindLoadedModel(models, target), "Engine seven-preview limit not enforced");
        fixture.Header(2, 1);
        Check(!FindLoadedModel(models, target), "Count exceeding capacity not rejected");

        std::array<RE::LoadedInventoryModelVR, 7> heap{};
        heap[1].itemBase = target;
        fixture.Header(2, 7, false);
        auto* heapPointer = heap.data();
        std::memcpy(reinterpret_cast<std::byte*>(&fixture.runtime) + 8, &heapPointer, sizeof(heapPointer));
        Check(FindLoadedModel(models, target) == &heap[1], "Heap preview lookup failed");
        heapPointer = nullptr;
        std::memcpy(reinterpret_cast<std::byte*>(&fixture.runtime) + 8, &heapPointer, sizeof(heapPointer));
        Check(!FindLoadedModel(models, target), "Null heap storage indexed");
        fixture.Header(0, 0);

        // Same production lookup must retain the flat entry representation.
        struct FlatModels
        {
            std::array<RE::LoadedInventoryModel, 2> entries{};
            std::size_t size() const { return entries.size(); }
            std::size_t capacity() const { return entries.size(); }
            auto* data() { return entries.data(); }
            auto& operator[](std::size_t i) { return entries[i]; }
        } flat;
        flat.entries[1].itemBase = target;
        Check(FindLoadedModel(flat, target) == &flat.entries[1], "Flat preview lookup regressed");
        std::cout << "Inventory preview tests passed: VR inline/heap ABI, poisoned flat count, bounded lookup and flat compatibility\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
