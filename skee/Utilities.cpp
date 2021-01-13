#include "Utilities.h"

#include <memory>

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
