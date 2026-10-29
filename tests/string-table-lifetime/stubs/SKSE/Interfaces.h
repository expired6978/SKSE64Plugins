#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <algorithm>
#include <climits>
namespace SKSE {
namespace log {
template<class... Args> inline void error(const char*, Args&&...) {}
template<class... Args> inline void info(const char*, Args&&...) {}
}
class SerializationInterface {
public:
    mutable std::vector<unsigned char> bytes;
    mutable std::size_t cursor = 0;
    bool OpenRecord(std::uint32_t, std::uint32_t) const { return true; }
    bool WriteRecordData(const void* value, std::uint32_t size) const {
        const auto p = static_cast<const unsigned char*>(value);
        bytes.insert(bytes.end(), p, p + size);
        return true;
    }
    std::uint32_t ReadRecordData(void* value, std::uint32_t size) const {
        if (cursor + size > bytes.size()) return 0;
        std::memcpy(value, bytes.data() + cursor, size);
        cursor += size;
        return size;
    }
};
}
