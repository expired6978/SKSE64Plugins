#include "Utilities.h"

#include <memory>

namespace utils
{
	size_t hash_lower(const char* str, size_t count)
	{
		const size_t _FNV_offset_basis = 14695981039346656037ULL;
		const size_t _FNV_prime = 1099511628211ULL;

		size_t _Val = _FNV_offset_basis;
		for (size_t _Next = 0; _Next < count; ++_Next)
		{	// fold in another byte
			_Val ^= (size_t)tolower(str[_Next]);
			_Val *= _FNV_prime;
		}
		return _Val;
	}

	std::string format(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		size_t size = std::snprintf(nullptr, 0, format, args) + 1; // Extra space for '\0'
		std::unique_ptr<char[]> buf(new char[size]);
		std::vsnprintf(buf.get(), size, format, args);
		va_end(args);
		return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
	}
}
