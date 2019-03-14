#include "Utilities.h"

#include "skse64/PapyrusVM.h"

namespace VirtualMachine
{
UInt64 GetHandle(void * src, UInt32 typeID)
{
	VMClassRegistry		* registry = (*g_skyrimVM)->GetClassRegistry();
	IObjectHandlePolicy	* policy = registry->GetHandlePolicy();

	return policy->Create(typeID, (void*)src);
}

void * GetObject(UInt64 handle, UInt32 typeID)
{
	VMClassRegistry		* registry = (*g_skyrimVM)->GetClassRegistry();
	IObjectHandlePolicy	* policy = registry->GetHandlePolicy();

	if (handle == policy->GetInvalidHandle()) {
		return NULL;
	}

	return policy->Resolve(typeID, handle);
}
}
