#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

// Engine-free wire policy shared by scalar and interned-string adapters.
// String records retain the existing uint16 length + unterminated byte payload.
namespace Serialization::Detail
{
	inline constexpr std::size_t MaxStringBytes = std::numeric_limits<std::int16_t>::max();

	template<class Interface, class T>
	bool WriteScalar(const Interface* intfc, const T* value)
	{
		static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>);
		if (!intfc || !value) return false;
		if constexpr (std::is_same_v<T, bool>) {
			const std::uint8_t encoded = *value ? 1 : 0;
			return intfc->WriteRecordData(&encoded, sizeof(encoded));
		} else {
			return intfc->WriteRecordData(value, sizeof(T));
		}
	}

	template<class Interface, class T>
	bool ReadScalar(const Interface* intfc, T* value)
	{
		static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>);
		if (!intfc || !value) return false;
		if constexpr (std::is_same_v<T, bool>) {
			std::uint8_t encoded{};
			if (intfc->ReadRecordData(&encoded, sizeof(encoded)) != sizeof(encoded) || encoded > 1) return false;
			*value = encoded != 0;
		} else {
			T decoded{};
			if (intfc->ReadRecordData(&decoded, sizeof(decoded)) != sizeof(decoded)) return false;
			*value = decoded;
		}
		return true;
	}

	template<class Interface>
	bool WriteString(const Interface* intfc, const char* value, std::size_t size)
	{
		if (!intfc || size > MaxStringBytes || (size && (!value || std::memchr(value, '\0', size)))) return false;
		const auto length = static_cast<std::uint16_t>(size);
		return WriteScalar(intfc, &length) && (size == 0 || intfc->WriteRecordData(value, length));
	}

	template<class Interface>
	bool ReadString(const Interface* intfc, std::string& value)
	{
		std::uint16_t length{};
		if (!ReadScalar(intfc, &length) || length > MaxStringBytes) return false;
		std::string decoded(length, '\0');
		if (length && intfc->ReadRecordData(decoded.data(), length) != length) return false;
		if (decoded.find('\0') != std::string::npos) return false;
		value = std::move(decoded);
		return true;
	}
}
