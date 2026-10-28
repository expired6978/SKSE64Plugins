#pragma once

#include "HeadExportPublication.h"
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <string>

// Private sibling staging, with DDS installed before NIF. No recursive removal;
// failed rollback retains staging/backups for recovery instead of deleting them.
// Cooperating exports run on the game task queue. Unrelated external writers
// and process/power loss are not covered by a two-file atomicity guarantee.
class HeadExportFiles
{
    using Output = HeadExportPublication::Output;
    using Identity = HeadExportPublication::Identity;
    enum class State { missing, regular, error };
public:
    HeadExportFiles(const char* nif, const char* dds, bool withDDS)
    {
        outputs_[0].enabled = withDDS;
        outputs_[0].finalPath = std::filesystem::absolute(dds).lexically_normal();
        outputs_[1].enabled = true;
        outputs_[1].finalPath = std::filesystem::absolute(nif).lexically_normal();
    }
    HeadExportFiles(const HeadExportFiles&) = delete;
    HeadExportFiles& operator=(const HeadExportFiles&) = delete;

    ~HeadExportFiles() noexcept
    {
        try {
            if (!committed_ && !recoveryRequired_)
                recoveryRequired_ = !HeadExportPublication::Rollback(*this, outputs_);
            if (recoveryRequired_) return;
            for (auto& out : outputs_) {
                if (out.directory.empty()) continue;
                std::error_code error;
                // These names are inside our exclusively created directory.
                // Do not remove an unexpected same-path replacement.
                if (out.installed && out.backedUp && Matches(out.backupPath, out.original))
                    std::filesystem::remove(out.backupPath, error);
                if (Matches(out.stagedPath, out.staged))
                    std::filesystem::remove(out.stagedPath, error);
                std::filesystem::remove(out.directory, error); // only if empty
            }
        } catch (...) {}  // Cleanup must not escape a game-task destructor.
    }

    bool Prepare()
    {
        static std::atomic<std::uint64_t> ordinal{};
        for (auto& out : outputs_) {
            if (!out.enabled) continue;
            std::error_code error;
            std::filesystem::create_directories(out.finalPath.parent_path(), error);
            if (error) return false;
            const auto state = Read(out.finalPath, out.original);
            if (state == State::error) return false;
            out.existed = state == State::regular;
            bool reserved = false;
            for (unsigned attempt = 0; attempt < 3 && !reserved; ++attempt) {
                const auto id = std::to_string(::GetCurrentProcessId()) + "-" +
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                    std::to_string(++ordinal);
                auto candidate = out.finalPath.parent_path() / (".skee-export-" + id);
                reserved = std::filesystem::create_directory(candidate, error);
                if (error) return false;
                if (reserved) out.directory = std::move(candidate);
            }
            if (!reserved) return false;
            out.stagedPath = out.directory / out.finalPath.filename();
            out.backupPath = out.directory / "previous-output";
        }
        return true;
    }

    const std::filesystem::path& DDS() const { return outputs_[0].stagedPath; }
    const std::filesystem::path& NIF() const { return outputs_[1].stagedPath; }
    void RememberStaged()
    {
        // Also remember empty/partial writer outputs for bounded cleanup.
        for (auto& out : outputs_)
            if (out.enabled) Read(out.stagedPath, out.staged);
    }

    bool Commit()
    {
        for (auto& out : outputs_) {
            Identity actual;
            if (out.enabled && (Read(out.stagedPath, actual, true) != State::regular || !(actual == out.staged))) return false;
        }
        committed_ = HeadExportPublication::Commit(*this, outputs_, recoveryRequired_);
        return committed_;
    }

    bool RecoveryRequired() const { return recoveryRequired_; }
    std::string RecoveryDirectories() const
    {
        return outputs_[0].directory.string() + " ; " + outputs_[1].directory.string();
    }

    bool Matches(const std::filesystem::path& path, const Identity& expected) const
    {
        Identity actual;
        return !path.empty() && Read(path, actual) == State::regular && actual == expected;
    }
    bool Missing(const std::filesystem::path& path) const
    {
        Identity unused;
        return Read(path, unused) == State::missing;
    }
    bool Move(const std::filesystem::path& from, const std::filesystem::path& to) const
    {
        // MoveFileW fails if 'to' exists. Do not use filesystem::rename here:
        // replacement semantics vary, and rollback must preserve other writers.
        return ::MoveFileW(from.c_str(), to.c_str()) != FALSE;
    }
private:
    static State Read(const std::filesystem::path& path, Identity& id, bool nonempty = false)
    {
        const auto handle = ::CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        // The project PCH intentionally undefines these two Win32 macros.
        if (handle == reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-1))) {
            const auto error = ::GetLastError();
            return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? State::missing : State::error;
        }
        BY_HANDLE_FILE_INFORMATION info{};
        const bool read = ::GetFileInformationByHandle(handle, &info) != FALSE;
        ::CloseHandle(handle);
        if (!read || (info.dwFileAttributes & (0x10u /* directory */ | FILE_ATTRIBUTE_REPARSE_POINT)) ||
            (nonempty && info.nFileSizeHigh == 0 && info.nFileSizeLow == 0)) return State::error;
        id = {info.dwVolumeSerialNumber, info.nFileIndexHigh, info.nFileIndexLow};
        return State::regular;
    }

    std::array<Output, 2> outputs_{};
    bool committed_{}, recoveryRequired_{};
};
