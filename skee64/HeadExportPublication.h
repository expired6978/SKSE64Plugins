#pragma once

#include <array>
#include <filesystem>
#include <cstdint>

namespace HeadExportPublication
{
    struct Identity
    {
        std::uint32_t volume{}, high{}, low{};
        bool operator==(const Identity& other) const noexcept
        { return volume == other.volume && high == other.high && low == other.low; }
    };

    struct Output
    {
        std::filesystem::path finalPath, stagedPath, backupPath, directory;
        Identity original{}, staged{};
        bool enabled{}, existed{}, backedUp{}, installed{};
    };

    // Ops must distinguish missing from unreadable, compare file identities,
    // and rename without replacing an existing destination. This transaction
    // is recoverable within the process, NOT a crash-atomic two-file commit.
    template <class Ops, std::size_t N>
    bool Rollback(Ops& ops, std::array<Output, N>& outputs)
    {
        bool restored = true;
        for (auto it = outputs.rbegin(); it != outputs.rend(); ++it) {
            auto& out = *it;
            if (out.installed) {
                if (!ops.Matches(out.finalPath, out.staged) ||
                    !ops.Move(out.finalPath, out.stagedPath)) {
                    restored = false;
                    continue;  // Never overwrite a competing writer's file.
                }
                out.installed = false;
            }
            if (out.backedUp) {
                if (!ops.Matches(out.backupPath, out.original) ||
                    !ops.Move(out.backupPath, out.finalPath)) restored = false;
                else out.backedUp = false;
            }
        }
        return restored;
    }

    template <class Ops, std::size_t N>
    bool Commit(Ops& ops, std::array<Output, N>& outputs, bool& recoveryRequired)
    {
        recoveryRequired = false;
        for (auto& out : outputs) {
            if (!out.enabled) continue;
            const bool unchanged = out.existed ? ops.Matches(out.finalPath, out.original) : ops.Missing(out.finalPath);
            if (!unchanged || !ops.Matches(out.stagedPath, out.staged)) {
                recoveryRequired = !Rollback(ops, outputs);
                return false;
            }
            if (out.existed) {
                if (!ops.Move(out.finalPath, out.backupPath)) {
                    recoveryRequired = !Rollback(ops, outputs);
                    return false;
                }
                out.backedUp = true;
            }
            if (!ops.Move(out.stagedPath, out.finalPath)) {
                recoveryRequired = !Rollback(ops, outputs);
                return false;
            }
            out.installed = true;
        }
        return true;
    }
}
