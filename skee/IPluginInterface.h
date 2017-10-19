#pragma once

#include <map>

struct SKSESerializationInterface;

class IPluginInterface
{
public:
	IPluginInterface() { };
	virtual ~IPluginInterface();

	virtual UInt32 GetVersion();
	virtual void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	virtual bool Load(SKSESerializationInterface * intfc, UInt32 kVersion);
	virtual void Revert();
};

class IInterfaceMap : public std::map<const char*, IPluginInterface*>
{
public:
	virtual IPluginInterface * QueryInterface(const char * name);
	virtual bool AddInterface(const char * name, IPluginInterface * pluginInterface);
	virtual IPluginInterface * RemoveInterface(const char * name);
};

struct InterfaceExchangeMessage
{
	enum
	{
		kMessage_ExchangeInterface = 0x9E3779B9
	};

	IInterfaceMap * interfaceMap = NULL;
};