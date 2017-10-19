#include "IPluginInterface.h"

IPluginInterface::~IPluginInterface()
{

}

UInt32 IPluginInterface::GetVersion()
{
	return 0;
}

void IPluginInterface::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{

}

bool IPluginInterface::Load(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	return false;
}

void IPluginInterface::Revert()
{

}

IPluginInterface * IInterfaceMap::QueryInterface(const char * name)
{
	for (auto iface : *this)
	{
		if (_stricmp(iface.first, name) == 0) {
			return iface.second;
		}
	}

	return NULL;
}

bool IInterfaceMap::AddInterface(const char * name, IPluginInterface * pluginInterface)
{
	if (QueryInterface(name) != NULL)
		return false;

	emplace(name, pluginInterface);
	return true;
}

IPluginInterface * IInterfaceMap::RemoveInterface(const char * name)
{
	for (auto it = begin(); it != end(); ++it)
	{
		if (_stricmp(it->first, name) == 0) {
			IPluginInterface * foundInterface = it->second;
			erase(it);
			return foundInterface;
		}
	}

	return NULL;
}