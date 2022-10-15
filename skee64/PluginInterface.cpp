#include "PluginInterface.h"

IPluginInterface * InterfaceMap::QueryInterface(const char * name)
{
	for (auto iface : *this)
	{
		if (_stricmp(iface.first, name) == 0) {
			return iface.second;
		}
	}

	return NULL;
}

bool InterfaceMap::AddInterface(const char * name, IPluginInterface * pluginInterface)
{
	if (QueryInterface(name) != NULL)
		return false;

	emplace(name, pluginInterface);
	return true;
}

IPluginInterface * InterfaceMap::RemoveInterface(const char * name)
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