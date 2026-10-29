#pragma once
// Test boundary only: no copied game SDK or external implementation.
#include <string>
#include <cstddef>
#include <cctype>
#include <cstring>
#include <climits>
namespace utils {
inline std::size_t hash_lower(const char* text, std::size_t length) {
    std::size_t value = 0;
    for (std::size_t i = 0; i < length; ++i)
        value = value * 131 + static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(text[i])));
    return value;
}
}
template<class... Args> inline void Console_Print(const char*, Args&&...) {}
