// SPDX-License-Identifier: GPL-3.0-or-later
// Host-independent tests of the actual production validation/transaction policy.
#include "HeadExportPolicy.h"
#include "HeadExportPublication.h"
#ifdef _WIN32
#include "HeadExportFiles.h"
#include <fstream>
#endif
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

using namespace HeadExportPublication;
namespace {
    unsigned checks = 0;
    void Check(bool value, const char* message)
    {
        ++checks;
        if (!value) throw std::runtime_error(message);
    }
    Identity Id(unsigned value) { return {1, 0, value}; }
    struct FakeFiles {
        std::map<std::filesystem::path, Identity> files;
        std::set<unsigned> failures;
        unsigned moves{};
        bool compete{};
        bool Matches(const std::filesystem::path& path, const Identity& expected)
        { auto it = files.find(path); return it != files.end() && it->second == expected; }
        bool Missing(const std::filesystem::path& path) { return files.count(path) == 0; }
        bool Move(const std::filesystem::path& from, const std::filesystem::path& to)
        {
            ++moves;
            if (failures.count(moves)) {
                if (compete) files["nif"] = Id(99);
                return false;
            }
            if (Missing(from) || !Missing(to)) return false;
            files.emplace(to, files.at(from));
            files.erase(from);
            return true;
        }
    };
    std::array<Output, 2> Outputs(bool existing = true)
    {
        std::array<Output, 2> out{};
        out[0] = {"dds", "staged-dds", "backup-dds", {}, Id(1), Id(3), true, existing};
        out[1] = {"nif", "staged-nif", "backup-nif", {}, Id(2), Id(4), true, existing};
        return out;
    }
    FakeFiles Files(bool existing = true)
    {
        FakeFiles files;
        files.files = {{"staged-dds", Id(3)}, {"staged-nif", Id(4)}};
        if (existing) { files.files["dds"] = Id(1); files.files["nif"] = Id(2); }
        return files;
    }
}

int main()
{
    try {
        using HeadExportPolicy::CoherentSkinLayout;
        Check(CoherentSkinLayout(0, 0, false, false, false), "empty skin rejected");
        Check(CoherentSkinLayout(0, 0, true, false, false), "zero-bone data rejected");
        Check(CoherentSkinLayout(3, 3, true, true, true), "coherent skin rejected");
        Check(!CoherentSkinLayout(4, 3, true, true, true), "extra live bone could escape remapping");
        Check(!CoherentSkinLayout(2, 3, true, true, true), "short transform array accepted");
        Check(!CoherentSkinLayout(3, 0, false, true, true), "nonempty skin without data accepted");
        Check(!CoherentSkinLayout(3, 3, true, false, true), "missing bones accepted");
        Check(!CoherentSkinLayout(3, 3, true, true, false), "missing worlds accepted");

        for (bool existing : {false, true}) {
            auto out = Outputs(existing); auto files = Files(existing); bool recovery{};
            Check(Commit(files, out, recovery), "normal publication failed");
            Check(!recovery && files.Matches("dds", Id(3)) && files.Matches("nif", Id(4)), "wrong published pair");
            Check(Rollback(files, out), "explicit rollback failed");
            Check(existing ? files.Matches("dds", Id(1)) && files.Matches("nif", Id(2)) :
                files.Missing("dds") && files.Missing("nif"), "rollback did not restore original state");
        }
        // Fail each successive install/backup move, including after the DDS
        // was installed. Prior outputs must be restored, staged bytes retained.
        for (unsigned failure = 1; failure <= 4; ++failure) {
            auto out = Outputs(); auto files = Files(); bool recovery{};
            files.failures.insert(failure);
            Check(!Commit(files, out, recovery), "injected move failure reported success");
            Check(!recovery && files.Matches("dds", Id(1)) && files.Matches("nif", Id(2)), "old pair not restored");
            Check(files.Matches("staged-dds", Id(3)) && files.Matches("staged-nif", Id(4)), "staged bytes lost");
        }
        {
            auto out = Outputs(); auto files = Files(); bool recovery{};
            files.failures = {4, 5};
            Check(!Commit(files, out, recovery) && recovery, "failed rollback not surfaced");
            Check(files.Matches("backup-nif", Id(2)), "recovery backup discarded");
            Check(files.Matches("dds", Id(1)), "independent DDS rollback skipped");
        }
        {
            auto out = Outputs(); auto files = Files(); bool recovery{};
            files.failures = {4}; files.compete = true;
            Check(!Commit(files, out, recovery) && recovery, "competing writer not surfaced");
            Check(files.Matches("nif", Id(99)) && files.Matches("backup-nif", Id(2)), "competing output overwritten");
        }
        {
            auto out = Outputs(); auto files = Files(); bool recovery{};
            files.files["nif"] = Id(99);
            Check(!Commit(files, out, recovery) && !recovery, "changed destination accepted");
            Check(files.Matches("dds", Id(1)) && files.Matches("nif", Id(99)), "preflight rollback corrupted files");
        }
        {
            auto out = Outputs(); auto files = Files(); bool recovery{};
            files.files["staged-nif"] = Id(99);
            Check(!Commit(files, out, recovery) && !recovery, "changed stage accepted");
            Check(files.Matches("dds", Id(1)) && files.Matches("nif", Id(2)), "stage mismatch lost old pair");
        }
        {
            auto out = Outputs(); auto files = Files(); bool recovery{};
            out[0].enabled = false;
            Check(Commit(files, out, recovery), "NIF-only export failed");
            Check(files.Matches("dds", Id(1)) && files.Matches("nif", Id(4)), "NIF-only export changed DDS");
        }
#ifdef _WIN32
        // Exercise the real no-replace Windows operations inside a unique local
        // test directory. No game paths, installed plugin, or engine loading.
        const auto root = std::filesystem::temp_directory_path() /
            ("head-export-test-" + std::to_string(::GetCurrentProcessId()) + "-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Check(std::filesystem::create_directory(root), "test directory not reserved");
        auto cleanup = [&]() {
            std::filesystem::remove(root / "head.nif");
            std::filesystem::remove(root / "head.dds");
            Check(std::filesystem::is_empty(root), "unexpected retained staging files");
            std::filesystem::remove(root);
        };
        const auto nif = root / "head.nif", dds = root / "head.dds";
        std::ofstream(nif) << "old-nif"; std::ofstream(dds) << "old-dds";
        {
            HeadExportFiles transaction(nif.string().c_str(), dds.string().c_str(), true);
            Check(transaction.Prepare(), "real preparation failed");
            std::ofstream(transaction.DDS()) << "new-dds";
            std::ofstream(transaction.NIF()) << "new-nif";
            transaction.RememberStaged();
            Check(transaction.Commit(), "real pair commit failed");
        }
        std::string text;
        std::ifstream(nif) >> text; Check(text == "new-nif", "real NIF wrong");
        std::ifstream(dds) >> text; Check(text == "new-dds", "real DDS wrong");
        {
            HeadExportFiles transaction(nif.string().c_str(), dds.string().c_str(), true);
            Check(transaction.Prepare(), "second real preparation failed");
            std::ofstream(transaction.DDS()) << "partial";
            std::ofstream(transaction.NIF()); // serialization returned an empty file
            transaction.RememberStaged();
            Check(!transaction.Commit(), "empty NIF accepted");
        }
        std::ifstream(nif) >> text; Check(text == "new-nif", "failed writer replaced NIF");
        std::ifstream(dds) >> text; Check(text == "new-dds", "failed writer replaced DDS");
        cleanup();
#endif
        std::cout << checks << " head-export checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
