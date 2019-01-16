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