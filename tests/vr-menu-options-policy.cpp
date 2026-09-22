#include "VRMenuOptionsPolicy.h"
#include <iostream>
#include <string>
#include <cstdint>
#ifdef _WIN32
#include <Windows.h>
#include <fstream>
#endif

enum class Flag : std::uint32_t { Cursor = 4, Update = 1024, Other = 128 };
struct Flags
{
    std::uint32_t bits;
    void set(Flag flag) { bits |= static_cast<std::uint32_t>(flag); }
    void reset(Flag flag) { bits &= ~static_cast<std::uint32_t>(flag); }
};

int main()
{
    using namespace SKEE::VR::MenuOptionsPolicy;
    unsigned failures{};
    const auto check = [&](bool result, const char* label) {
        if (!result) { ++failures; std::cerr << "FAIL " << label << '\n'; }
    };
    using SKEE::VR::Runtime;
    using SKEE::VR::RuntimeForModules;
    check(RuntimeForModules(false, true, false, true, true) == Runtime::SteamVR, "direct SteamVR implementation");
    check(RuntimeForModules(false, false, false, true, true) == Runtime::SteamVR, "SkyrimVRTools proxy over SteamVR loader");
    check(RuntimeForModules(true, false, true, true, false) == Runtime::OpenComposite, "direct OCU implementation");
    check(RuntimeForModules(false, false, true, true, true) == Runtime::OpenComposite, "OCU proxy wins over incidental SteamVR client");
    check(RuntimeForModules(false, false, false, false, true) == Runtime::Unknown, "SteamVR client presence alone is insufficient");
    check(RuntimeForModules(false, false, false, true, false) == Runtime::Unknown, "unclassified loader without SteamVR client");
    check(SKEE::VR::UsesStreamedKeyboard(Runtime::SteamVR), "SteamVR drafts accumulate");
    check(!SKEE::VR::UsesStreamedKeyboard(Runtime::OpenComposite), "OCU retains authoritative full buffer");
    check(SKEE::VR::UsesStreamedKeyboard(Runtime::Unknown), "unknown backend cannot silently replace streamed draft");
    check(UseQuill(Runtime::SteamVR, -1, -1, -1), "SteamVR default quill enabled");
    check(!UseQuill(Runtime::OpenComposite, -1, -1, -1), "OCU default quill disabled");
    check(!UseQuill(Runtime::Unknown, -1, -1, -1), "unknown runtime retains cursor flags");
    check(!UseQuill(Runtime::SteamVR, 0, 1, 1), "SteamVR specific zero beats legacy one");
    check(UseQuill(Runtime::OpenComposite, 0, 1, 0), "OCU specific one beats legacy zero");
    check(UseQuill(Runtime::SteamVR, 1, 0, -1), "independent SteamVR on");
    check(!UseQuill(Runtime::OpenComposite, 1, 0, -1), "independent OCU off");
    check(!UseQuill(Runtime::SteamVR, -1, -1, 0), "preserve explicit legacy off");
    check(UseQuill(Runtime::OpenComposite, -1, -1, 1), "preserve explicit legacy on");
    check(ValidPlayerName("Dragonborn") && ValidPlayerName("Jean-Luc O'Brien"), "ASCII names");
    check(ValidPlayerName("\xC3\xA9"), "UTF-8 name");
    check(ValidPlayerName(std::string(255, 'a')), "255-byte boundary");
    check(!ValidPlayerName(std::string(256, 'a')), "overlength rejection");
    check(!ValidPlayerName(std::string(4095, 'a')), "INI truncation remains overlength");
    check(!ValidPlayerName("") && !ValidPlayerName("   "), "empty/blank rejection");
    check(!ValidPlayerName("name\n") && !ValidPlayerName("name\t"), "control rejection");
    check(!ValidPlayerName(std::string("ab\0cd", 5)), "embedded NUL rejection");
    check(!ValidPlayerName("\xC3") && !ValidPlayerName("\xED\xA0\x80"), "invalid UTF-8 rejection");
    check(ApplyPlayerName(true, false, "New Name"), "new-game name");
    check(!ApplyPlayerName(false, false, "New Name"), "saved names protected / request consumed");
    check(ApplyPlayerName(false, true, "New Name"), "explicit saved-character override");
    check(!ApplyPlayerName(true, true, ""), "blank override disabled");
    NameStartIntent intent;
    check(!intent.Pending(), "startup does not authorize rename");
    intent.SaveLoading(); intent.Revert();
    check(!intent.Pending(), "ordinary save load remains protected");
    intent.StartFromMenu();
    intent.SaveLoading(); intent.Revert(); intent.SaveLoading();
    check(intent.Pending(), "confirmed New Game survives internal template loads and revert");
    intent.EngineNewGame();
    check(intent.Pending(), "engine notification retains confirmed start");
    intent.Consume(); intent.EngineNewGame();
    check(!intent.Pending(), "late engine notification cannot rename reopened menu");
    intent.StartFromMenu(); intent.Cancel(); intent.SaveLoading();
    check(!intent.Pending(), "Load/Continue/abandoned start cancels intent");
    intent.EngineNewGame();
    check(intent.Pending(), "engine-only New Game fallback");
    intent.SaveLoading();
    check(!intent.Pending(), "engine-only intent cannot cross a save load");
    intent.EngineNewGame(); intent.Revert();
    check(!intent.Pending(), "engine-only intent cannot cross revert");
    intent.StartFromMenu(); intent.Consume();
    check(!intent.Pending(), "creation menu close consumes unfulfilled request");
    intent.StartFromMenu();
    check(intent.Pending(), "subsequent confirmed New Game rearms once");
    intent.Cancel();
    check(!ApplyPlayerName(intent.Pending(), false, "New Name"), "cancelled start cannot rename");
#ifdef _WIN32
    // Exercise the same Windows ANSI-profile API used by the plugin, including
    // raw UTF-8 INIs without BOM. Only this uniquely owned test file is removed.
    char temporaryDirectory[MAX_PATH]{}, temporaryFile[MAX_PATH]{};
    const auto directoryLength = GetTempPathA(MAX_PATH, temporaryDirectory);
    if (directoryLength && directoryLength < MAX_PATH && GetTempFileNameA(temporaryDirectory, "rm2", 0, temporaryFile)) {
        {
            std::ofstream output(temporaryFile, std::ios::binary | std::ios::trunc);
            output << "[VR]\r\nsPlayerName=Jean-Luc\r\nsUnicode=\xC3\xA9\r\nsTooLong=" << std::string(256, 'a') << "\r\n";
            check(output.good(), "write uniquely owned INI test fixture");
        }
        char buffer[4096]{};
        GetPrivateProfileStringA("VR", "sPlayerName", "", buffer, sizeof(buffer), temporaryFile);
        check(std::string_view(buffer) == "Jean-Luc", "Windows INI name read");
        GetPrivateProfileStringA("VR", "sUnicode", "", buffer, sizeof(buffer), temporaryFile);
        check(std::string_view(buffer) == "\xC3\xA9" && ValidPlayerName(buffer), "Windows INI UTF-8 read");
        GetPrivateProfileStringA("VR", "sTooLong", "", buffer, sizeof(buffer), temporaryFile);
        check(std::string_view(buffer).size() == 256 && !ValidPlayerName(buffer), "Windows INI overlength not accepted as truncated name");
        check(DeleteFileA(temporaryFile) != 0, "remove owned INI test fixture");
    } else check(false, "create INI test fixture");
#endif
    Flags defaults{1024 | 128};
    ConfigureQuill(defaults, false, Flag::Cursor, Flag::Update);
    check(defaults.bits == (1024 | 128), "default flags unchanged");
    ConfigureQuill(defaults, true, Flag::Cursor, Flag::Update);
    check(defaults.bits == (4 | 128), "cursor ownership, unrelated flags preserved");
    ConfigureQuill(defaults, true, Flag::Cursor, Flag::Update);
    check(defaults.bits == (4 | 128), "idempotent construction policy");
    if (failures) return 1;
    std::cout << "PASS: VR INI name, New Game/load/reopen intent and constructor quill policy (not live qualification)\n";
}
