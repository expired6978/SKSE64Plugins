#pragma once

#include "IPluginInterface.h"
#include "Utilities.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

class TESObjectREFR;

class CommandInterface : public ICommandInterface
{
private:
    struct CommandData
    {
        std::string desc;
        CommandCallback cb = nullptr;

        bool operator()(const CommandData& x, const CommandData& y) const
        {
            return x.cb == y.cb;
        }
        size_t operator()(const CommandData& x) const
        {
            size_t seed = 0;
            utils::hash_combine(seed, std::hash<std::string>{}(desc), size_t(cb));
            return seed;
        }
    };

public:
    virtual UInt32 GetVersion() override { return 0; }
    virtual void Revert() override { }
    virtual bool RegisterCommand(const char* command, const char* desc, CommandCallback cb) override;

    bool ExecuteCommand(const char* command, TESObjectREFR* ref, const char* argumentString);

    void RegisterCommands();

private:
    struct iequal_to
    {
        bool operator()(const std::string& x, const std::string& y) const
        {
            return _stricmp(x.c_str(), y.c_str()) == 0;
        }
    };

    struct ihash
    {
        size_t operator()(const std::string& x) const
        {
            return utils::hash_lower(x.c_str(), x.length());
        }
    };

    std::mutex m_lock;
    std::unordered_map<std::string, std::unordered_set<CommandData, CommandData, CommandData>, ihash, iequal_to> m_commandMap;
};