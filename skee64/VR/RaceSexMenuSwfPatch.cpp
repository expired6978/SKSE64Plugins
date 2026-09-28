// SPDX-License-Identifier: GPL-3.0-or-later
#include "RaceSexMenuSwfPatch.h"
#if defined(ENABLE_SKYRIM_VR)
#include "SwfBytePatch.h"
#include "RaceSexMenuSwfFailurePolicy.h"
#include <RE/B/BSResourceNiBinaryStream.h>
#include <RE/B/BSScaleformManager.h>
#include <RE/G/GFxLoader.h>
#include <RE/G/GMemory.h>
#include <RE/G/GFxState.h>
#include <RE/M/MessageBoxMenu.h>
#include <RE/Offsets_VTABLE.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <atomic>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <Windows.h>

namespace SKEE::RaceSexMenuSwfPatch
{
    namespace
    {
        constexpr char kMoviePath[] = "Interface/VR/RaceSex_menu.swf";
        constexpr char kPatchPath[] = "SKSE/Plugins/RaceMenuVR2/racesex-menu.rmp";
        // Skyrim VR 1.4.15, statically qualified against retained memory dump.
        // OpenFile (slot 1) dispatches through OpenFileEx (slot 3). All other
        // opener methods and all other URLs continue through the engine unchanged.
        constexpr std::uintptr_t kOpenFileExRva = 0xF20B40;
        constexpr std::uintptr_t kMemoryFileCtorRva = 0xF217F0;
        // SKSE VR 2.0.12 replaces the loader state with SKSEFileLoader. Its
        // OpenFileEx calls the vanilla function directly, not through its
        // vtable. Hook the active qualified wrapper and chain unrelated URLs.
        constexpr std::uintptr_t kSkseOpenerTableRva = 0xF6140;
        constexpr SwfBytePatch::Hash kSkseOpenCodeHash{
            0xff,0xeb,0x7a,0x9c,0x92,0xa0,0x98,0x04,0x53,0x76,0x6c,0x4f,0xb7,0x9d,0x94,0xb7,
            0x6f,0x3f,0xb8,0x8c,0xd2,0x5c,0x41,0x2d,0xbe,0xf0,0x06,0x0d,0x09,0x98,0x97,0x49};
        constexpr std::size_t kMemoryFileSize = 0x30;
        using OpenFileEx = void* (*)(void*, const char*, void*, int, int);
        using MemoryFileCtor = void* (*)(void*, const char*, const std::uint8_t*, int);
        std::atomic<OpenFileEx> original{};
        std::once_flag preparation;
        bool ready{};
        std::atomic<bool> published{};
        // GMemoryFile borrows its backing bytes. Retain the immutable movie for
        // the process lifetime, including asynchronous loads and menu reopens.
        SwfBytePatch::Bytes movie;
        std::atomic<std::uint64_t> served{};

        void QueueFailureWarning(FailureKind kind) noexcept
        {
            // Prepare runs inside the menu constructor. Defer the modal until
            // it returns, on SKSE's game-thread task queue, not the loader thread.
            // Capture only the enum: no dying menu/movie or exception reference.
            // Called solely inside preparation's call_once, so no reopen spam.
            try {
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([kind] {
                        try {
                            // A plain OK callback cannot finish/name the player.
                            if (!RE::MessageBoxMenu::Create(FailureMessage(kind), nullptr, 0, 4, 10, "OK"))
                                SKSE::log::error("RaceMenu SWF failure warning: message box creation declined");
                        } catch (const std::exception& error) {
                            SKSE::log::error("RaceMenu SWF failure warning: {}", error.what());
                        } catch (...) {
                            SKSE::log::error("RaceMenu SWF failure warning: unknown display failure");
                        }
                    });
                } else {
                    SKSE::log::error("RaceMenu SWF failure warning: SKSE task interface unavailable");
                }
            } catch (...) {
                SKSE::log::error("RaceMenu SWF failure warning: could not queue message box");
            }
        }

        bool IsMovie(const char* path)
        {
            if (!path) return false;
            constexpr std::string_view expected = "interface/vr/racesex_menu.swf";
            for (std::size_t i=0; i<expected.size(); ++i) {
                auto ch = static_cast<unsigned char>(path[i]);
                if (!ch) return false;
                if (ch == '\\') ch = '/';
                if (ch >= 'A' && ch <= 'Z') ch += 'a'-'A';
                if (ch != expected[i]) return false;
            }
            return path[expected.size()] == '\0';
        }

        SwfBytePatch::Bytes ReadResource(const char* path)
        {
            RE::BSResourceNiBinaryStream stream(path);
            if (!stream.good() || !stream.stream) throw std::runtime_error(std::string("Missing resource: ")+path);
            const auto size = stream.stream->totalSize;
            if (!size || size > SwfBytePatch::kLimit) throw std::runtime_error("Resource size exceeds patch limit");
            SwfBytePatch::Bytes bytes(size);
            if (!stream.read(bytes.data(), size)) throw std::runtime_error(std::string("Short resource read: ")+path);
            return bytes;
        }

        bool IsQualifiedSkseOpener(std::uintptr_t activeTable)
        {
            const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"sksevr_1_4_15.dll"));
            if (!module || activeTable != module+kSkseOpenerTableRva) return false;
            const auto* slots = reinterpret_cast<const std::uintptr_t*>(activeTable);
            constexpr std::uintptr_t expected[]{0x17F40,0x12630,0x6AC0,0x17D40};
            for (std::size_t i=0; i<std::size(expected); ++i)
                if (slots[i] != module+expected[i]) return false;
            // Pin actual loaded code as well as module-relative table entries;
            // do not accept a wrapper whose OpenFileEx was detoured in place.
            return SwfBytePatch::Sha256({reinterpret_cast<const std::uint8_t*>(slots[3]),192}) == kSkseOpenCodeHash;
        }

        void* Open(void* self, const char* path, void* log, int flags, int mode)
        {
            if (!IsMovie(path)) return original.load(std::memory_order_acquire)(self,path,log,flags,mode);
            if (!published.load(std::memory_order_acquire)) return nullptr;
            // The engine constructor initializes the GString path, refcount=1,
            // borrowed buffer at +18, signed length at +20, cursor at +24 and
            // valid flag at +28. Its virtual destructor frees the same GFx heap.
            auto* storage = RE::GMemory::Alloc(kMemoryFileSize);
            if (!storage) {
                SKSE::log::error("RaceMenu runtime SWF patch: GFx memory-file allocation failed");
                return nullptr;
            }
            REL::Relocation<MemoryFileCtor> ctor{REL::Offset(kMemoryFileCtorRva)};
            auto* file = ctor(storage,path,movie.data(),static_cast<int>(movie.size()));
            const auto ordinal = served.fetch_add(1, std::memory_order_relaxed)+1;
            SKSE::log::info("RaceMenu runtime SWF patch: served verified FWS, bytes={}, open={}",movie.size(),ordinal);
            return file;
        }

        void Initialize(RE::BSScaleformManager* manager)
        {
            if (!manager || !manager->loader) throw std::runtime_error("Scaleform loader unavailable");
            auto opener = manager->loader->GetState(RE::GFxState::StateType::kFileOpener);
            REL::Relocation<std::uintptr_t> table{RE::VTABLE_BSScaleformFileOpener[0]};
            const auto expectedOpen = REL::Module::get().base()+kOpenFileExRva;
            const auto actualTable = opener ? *reinterpret_cast<std::uintptr_t*>(opener.get()) : 0;
            const auto actualOpen = reinterpret_cast<std::uintptr_t*>(table.address())[3];
            SKSE::log::info("RaceMenu runtime SWF adapter identity: base=0x{:X}, manager=0x{:X}, loader=0x{:X}, opener=0x{:X}, state={}, actualVtable=0x{:X}, expectedVtable=0x{:X}, nativeSlot3=0x{:X}, expectedSlot3=0x{:X}",
                REL::Module::get().base(),reinterpret_cast<std::uintptr_t>(manager),reinterpret_cast<std::uintptr_t>(manager->loader),reinterpret_cast<std::uintptr_t>(opener.get()),
                opener ? static_cast<int>(opener->GetStateType()) : -1,actualTable,table.address(),actualOpen,expectedOpen);
            if (!opener) throw std::runtime_error("Scaleform file opener state unavailable");
            if (actualTable != table.address() && !IsQualifiedSkseOpener(actualTable))
                throw std::runtime_error("Scaleform file opener object vtable mismatch; refusing an unqualified state");
            if (actualOpen != expectedOpen)
                throw std::runtime_error("Scaleform native OpenFileEx slot modified; refusing to replace another hook");
            constexpr std::uint8_t ctorPrefix[]{0x48,0x89,0x4c,0x24,0x08,0x57,0x48,0x83,0xec,0x30};
            constexpr std::uint8_t openPrefix[]{0x4c,0x8b,0xdc,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x70};
            if (std::memcmp(reinterpret_cast<void*>(REL::Module::get().base()+kMemoryFileCtorRva),ctorPrefix,sizeof(ctorPrefix)) ||
                std::memcmp(reinterpret_cast<void*>(expectedOpen),openPrefix,sizeof(openPrefix)))
                throw std::runtime_error("Runtime SWF adapter code signature mismatch");
            const auto source = ReadResource(kMoviePath);
            const auto patch = ReadResource(kPatchPath);
            movie = SwfBytePatch::ApplyRelease(source,patch);
            // Publish only fully verified immutable bytes. Set the forwarder
            // before patching the vtable so concurrent unrelated opens are safe.
            const auto selectedOpen = reinterpret_cast<std::uintptr_t*>(actualTable)[3];
            original.store(reinterpret_cast<OpenFileEx>(selectedOpen),std::memory_order_release);
            published.store(true,std::memory_order_release);
            REL::Relocation<std::uintptr_t> selectedTable{actualTable};
            selectedTable.write_vfunc(3,Open);
            ready = true;
            SKSE::log::info("RaceMenu runtime SWF patch: exact original verified; reconstructed {} bytes in memory",movie.size());
            SKSE::log::info("RaceMenu runtime SWF adapter installed on {} opener; other URLs chain unchanged",actualTable == table.address() ? "vanilla" : "SKSE VR 2.0.12");
        }
    }

    bool Prepare(RE::BSScaleformManager* manager)
    {
        std::call_once(preparation,[&] {
            try { Initialize(manager); }
            catch (const SwfBytePatch::IncompatibleSource& error) {
                SKSE::log::error("RaceMenu runtime SWF patch FAILED: {}. Install original RaceMenu SE 0.4.20.0; disable loose VR layout/generated SWFs. Original files were not changed.",error.what());
                QueueFailureWarning(FailureKind::IncompatibleMovie);
            }
            catch (const std::exception& error) {
                SKSE::log::error("RaceMenu runtime SWF patch FAILED: {}. Install original RaceMenu SE 0.4.20.0 and matching add-on; disable loose VR layout/generated SWFs. Original files were not changed.",error.what());
                QueueFailureWarning(FailureKind::Initialization);
            }
        });
        return ready;
    }

    bool HasServedMovie() { return served.load(std::memory_order_relaxed) != 0; }
}
#endif
