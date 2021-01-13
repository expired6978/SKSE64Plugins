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

inline void hash_combine(std::size_t& seed) { }

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, Rest... rest) {
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	hash_combine(seed, rest...);
}

std::string format(const char* format, ...);