#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace SKEE::MenuAppearance
{
	// -1 means retain the movie's original colour/alpha treatment.
	inline int ParseColor(std::string_view value)
	{
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
		if (!value.empty() && value.front() == '#') value.remove_prefix(1);
		if (value.size() != 6) return -1;
		int result = 0;
		for (char c : value) {
			int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
			if (digit < 0) return -1;
			result = (result << 4) | digit;
		}
		return result;
	}

	inline bool BoundedPNG(const std::uint8_t* bytes, std::size_t size)
	{
		constexpr std::uint8_t magic[] = {137, 80, 78, 71, 13, 10, 26, 10};
		if (size < 33 || size > 16 * 1024 * 1024) return false;
		for (std::size_t i = 0; i < 8; ++i) if (bytes[i] != magic[i]) return false;
		if (bytes[8] != 0 || bytes[9] != 0 || bytes[10] != 0 || bytes[11] != 13 ||
			bytes[12] != 'I' || bytes[13] != 'H' || bytes[14] != 'D' || bytes[15] != 'R') return false;
		auto read = [&](std::size_t offset) {
			return (std::uint32_t(bytes[offset]) << 24) | (std::uint32_t(bytes[offset + 1]) << 16) |
				(std::uint32_t(bytes[offset + 2]) << 8) | bytes[offset + 3];
		};
		auto width = read(16), height = read(20);
		return width > 0 && height > 0 && width <= 4096 && height <= 4096;
	}
}
