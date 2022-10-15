#pragma once

#include <mutex>

namespace utils
{
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

	size_t hash_lower(const char* str, size_t count);

	std::string format(const char* format, ...);

#if __cplusplus > 201703L
	template<typename T = std::mutex>
	using scoped_lock = std::scoped_lock<T>;
#else
	template<typename T = std::mutex>
	using scoped_lock = std::lock_guard<T>;
#endif
}
