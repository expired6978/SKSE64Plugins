// SPDX-License-Identifier: GPL-3.0-or-later
// Engine-free tests of the production wire policy; no SKSE/plugin/game loading.
#include "SerializationPolicy.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
	unsigned checks{};
	void Check(bool condition, const char* reason)
	{
		++checks;
		if (!condition) throw std::runtime_error(reason);
	}
	struct Archive {
		mutable std::vector<std::uint8_t> bytes;
		mutable std::size_t cursor{};
		mutable unsigned writes{};
		unsigned failWrite{};
		bool WriteRecordData(const void* data, std::uint32_t size) const
		{
			if (++writes == failWrite) return false;
			const auto* begin = static_cast<const std::uint8_t*>(data);
			bytes.insert(bytes.end(), begin, begin + size);
			return true;
		}
		std::uint32_t ReadRecordData(void* data, std::uint32_t requested) const
		{
			const auto size = std::min<std::size_t>(requested, bytes.size() - cursor);
			if (size) std::memcpy(data, bytes.data() + cursor, size);
			cursor += size;
			return static_cast<std::uint32_t>(size);
		}
	};
}

int main()
{
	using namespace Serialization::Detail;
	try {
		{
			Archive a;
			const std::uint32_t original = 0x12345678;
			Check(WriteScalar(&a, &original), "scalar write failed");
			Check(a.bytes == std::vector<std::uint8_t>({0x78, 0x56, 0x34, 0x12}), "scalar wire bytes changed");
			std::uint32_t read{};
			Check(ReadScalar(&a, &read) && read == original, "scalar roundtrip failed");
		}
		{
			Archive a;
			const bool no = false, yes = true;
			Check(WriteScalar(&a, &no) && WriteScalar(&a, &yes), "bool write failed");
			Check(a.bytes == std::vector<std::uint8_t>({0, 1}), "bool is not canonical one-byte wire value");
			bool read = true;
			Check(ReadScalar(&a, &read) && !read, "false roundtrip failed");
			Check(ReadScalar(&a, &read) && read, "true roundtrip failed");
			Check(!ReadScalar(&a, &read) && read, "missing bool changed destination");
		}
		{
			Archive a; a.bytes = {2};
			bool value = true;
			Check(!ReadScalar(&a, &value) && value, "noncanonical bool accepted");
		}
		{
			Archive a; a.bytes = {1, 2, 3};
			std::uint32_t value = 0xaabbccdd;
			Check(!ReadScalar(&a, &value) && value == 0xaabbccdd, "short scalar read published partial value");
		}
		for (std::size_t size : {std::size_t{0}, std::size_t{9}, MaxStringBytes}) {
			Archive a;
			const std::string value(size, 'x');
			Check(WriteString(&a, value.c_str(), value.size()), "valid string write failed");
			Check(a.bytes.size() == size + 2 && a.bytes[0] == (size & 255) && a.bytes[1] == (size >> 8), "string length/payload framing changed");
			std::string read = "previous";
			Check(ReadString(&a, read) && read == value, "string roundtrip or empty clearing failed");
			Check(a.cursor == a.bytes.size(), "string consumed wrong byte count");
		}
		for (std::size_t size : {MaxStringBytes + 1, std::size_t{65536}}) {
			Archive a;
			const std::string value(size, 'x');
			Check(!WriteString(&a, value.c_str(), value.size()) && a.bytes.empty(), "long string truncated before bounds check");
		}
		const std::vector<std::vector<std::uint8_t>> malformed{{1}, {3, 0, 'x', 'y'}, {255, 255}, {3, 0, 'x', 0, 'y'}};
		for (const auto& bytes : malformed) {
			Archive a; a.bytes = bytes;
			std::string value = "unchanged";
			Check(!ReadString(&a, value) && value == "unchanged", "malformed string accepted or published partial destination");
		}
		for (unsigned failure : {1u, 2u}) {
			Archive a; a.failWrite = failure;
			Check(!WriteString(&a, "abc", 3), "writer failure was reported as success");
		}
		{
			Archive a;
			Check(!WriteString(&a, "a\0b", 3) && a.bytes.empty(), "embedded null accepted");
			Check(!WriteString(&a, nullptr, 1) && a.bytes.empty(), "null nonempty string accepted");
			Check(WriteString(&a, nullptr, 0), "null empty string rejected");
		}
		std::cout << checks << " serialization-policy checks passed\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
