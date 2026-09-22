// SPDX-License-Identifier: GPL-3.0-or-later
#include "SwfBytePatch.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <windows.h>
#include <bcrypt.h>
#include <zlib.h>

namespace SKEE::SwfBytePatch
{
    namespace
    {
        constexpr std::size_t kHeader = 120;
        struct AlgorithmHandle
        {
            BCRYPT_ALG_HANDLE value{};
            ~AlgorithmHandle() { if (value) BCryptCloseAlgorithmProvider(value,0); }
        };
        struct HashHandle
        {
            BCRYPT_HASH_HANDLE value{};
            ~HashHandle() { if (value) BCryptDestroyHash(value); }
        };
        std::uint32_t U32(std::span<const std::uint8_t> bytes, std::size_t at)
        {
            if (at > bytes.size() || bytes.size() - at < 4) throw std::runtime_error("Truncated patch integer");
            return bytes[at] | (std::uint32_t(bytes[at+1]) << 8) |
                (std::uint32_t(bytes[at+2]) << 16) | (std::uint32_t(bytes[at+3]) << 24);
        }
        void Verify(std::span<const std::uint8_t> bytes, std::span<const std::uint8_t> expected, const char* error, bool sourceMismatch = false)
        {
            const auto actual = Sha256(bytes);
            if (expected.size() != actual.size() || !std::equal(actual.begin(), actual.end(), expected.begin())) {
                if (sourceMismatch) throw IncompatibleSource(error);
                throw std::runtime_error(error);
            }
        }
    }

    Hash Sha256(std::span<const std::uint8_t> bytes)
    {
        if (bytes.size() > kLimit) throw std::runtime_error("SHA input exceeds limit");
        AlgorithmHandle handle;
        if (BCryptOpenAlgorithmProvider(&handle.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
            throw std::runtime_error("Cannot initialize SHA256");
        // CommonLib deliberately targets the Windows 7 API surface. Avoid the
        // Windows-10-only BCryptHash convenience entry point.
        ULONG objectLength{}, returned{};
        auto status = BCryptGetProperty(handle.value,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectLength),
            sizeof(objectLength),&returned,0);
        if (status < 0 || returned != sizeof(objectLength) || objectLength > kLimit) {
            throw std::runtime_error("Cannot obtain SHA256 object size");
        }
        Bytes object(objectLength);
        HashHandle hash;
        Hash result{};
        status = BCryptCreateHash(handle.value,&hash.value,object.data(),objectLength,nullptr,0,0);
        if (status >= 0) status = BCryptHashData(hash.value,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),0);
        if (status >= 0) status = BCryptFinishHash(hash.value,result.data(),static_cast<ULONG>(result.size()),0);
        if (status < 0) throw std::runtime_error("Cannot compute SHA256");
        return result;
    }

    Bytes Canonical(std::span<const std::uint8_t> file)
    {
        if (file.size() < 8 || file.size() > kLimit || file[1] != 'W' || file[2] != 'S' ||
            (file[0] != 'C' && file[0] != 'F')) throw std::runtime_error("Unsupported SWF");
        const auto length = U32(file, 4);
        if (length < 8 || length > kLimit) throw std::runtime_error("Invalid SWF length");
        Bytes output(length);
        std::copy_n(file.begin(), 8, output.begin());
        output[0] = 'F';
        if (file[0] == 'F') {
            if (file.size() != length) throw std::runtime_error("SWF length mismatch");
            std::copy(file.begin()+8, file.end(), output.begin()+8);
        } else {
            z_stream stream{};
            stream.next_in = const_cast<Bytef*>(file.data()+8);
            stream.avail_in = static_cast<uInt>(file.size()-8);
            stream.next_out = output.data()+8;
            stream.avail_out = length-8;
            if (inflateInit(&stream) != Z_OK) throw std::runtime_error("Cannot initialize SWF inflate");
            const auto status = inflate(&stream, Z_FINISH);
            const bool valid = status == Z_STREAM_END && stream.total_out == length-8 && stream.total_in == file.size()-8;
            inflateEnd(&stream);
            if (!valid) throw std::runtime_error("Invalid compressed SWF length/data");
        }
        return output;
    }

    Bytes Apply(std::span<const std::uint8_t> sourceFile, std::span<const std::uint8_t> patch)
    {
        if (patch.size() < kHeader || patch.size() > kLimit ||
            std::memcmp(patch.data(), "RMSWFP01", 8)) throw std::runtime_error("Invalid patch header");
        const auto sourceLength = U32(patch,12), outputLength = U32(patch,16), count = U32(patch,20);
        if (sourceLength < 8 || sourceLength > kLimit || outputLength < 8 || outputLength > kLimit ||
            !count || count > 200000) throw std::runtime_error("Invalid patch limits");
        if (sourceFile.size() != U32(patch,8)) throw IncompatibleSource("Original file size mismatch");
        Verify(sourceFile, patch.subspan(24,32), "Original file hash mismatch", true);
        const auto source = Canonical(sourceFile);
        if (source.size() != sourceLength) throw IncompatibleSource("Original canonical size mismatch");
        Verify(source, patch.subspan(56,32), "Original canonical hash mismatch", true);
        Bytes output;
        output.reserve(outputLength);
        std::size_t p = kHeader;
        for (std::uint32_t i=0; i<count; ++i) {
            if (p == patch.size()) throw std::runtime_error("Truncated patch operation");
            const auto kind = patch[p++];
            const auto value = U32(patch,p); p += 4;
            if (kind == 0) {
                const auto length = U32(patch,p); p += 4;
                if (!length || value > source.size() || length > source.size()-value || length > outputLength-output.size())
                    throw std::runtime_error("Invalid patch copy");
                output.insert(output.end(), source.begin()+value, source.begin()+value+length);
            } else if (kind == 1) {
                if (!value || value > patch.size()-p || value > outputLength-output.size())
                    throw std::runtime_error("Invalid patch insert");
                output.insert(output.end(), patch.begin()+p, patch.begin()+p+value); p += value;
            } else throw std::runtime_error("Unknown patch operation");
        }
        if (p != patch.size() || output.size() != outputLength) throw std::runtime_error("Patch length mismatch");
        Verify(output, patch.subspan(88,32), "Output hash mismatch");
        if (output[0] != 'F' || output[1] != 'W' || output[2] != 'S' || U32(output,4) != output.size())
            throw std::runtime_error("Invalid output SWF header");
        return output;
    }

    Bytes ApplyRelease(std::span<const std::uint8_t> sourceFile, std::span<const std::uint8_t> patch)
    {
        constexpr Hash patchHash{0x67,0x72,0xc1,0x75,0xe4,0xb9,0x3a,0x64,0x9b,0xa3,0x8d,0x34,0xf4,0x49,0x84,0x0b,
            0xd6,0xc9,0x70,0x1f,0x9b,0x1c,0x12,0x60,0x04,0x8f,0xc1,0x9a,0xfb,0x19,0xa7,0x9f};
        Verify(patch, patchHash, "Release patch hash mismatch (wrong/missing add-on version)");
        return Apply(sourceFile, patch);
    }
}
