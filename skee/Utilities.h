#pragma once

class ScopedCriticalSection
{
public:
	ScopedCriticalSection(LPCRITICAL_SECTION cs) : m_cs(cs)
	{
		EnterCriticalSection(m_cs);
	};
	~ScopedCriticalSection()
	{
		LeaveCriticalSection(m_cs);
	}

private:
	LPCRITICAL_SECTION m_cs;
};

namespace VirtualMachine
{
	UInt64 GetHandle(void * src, UInt32 typeID);
	void * GetObject(UInt64 handle, UInt32 typeID);
}
