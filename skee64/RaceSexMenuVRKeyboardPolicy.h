#pragma once
#include <cstdint>
#include <string_view>
#include <string>
#include <array>
#include <cstring>

namespace SKEE::VR::KeyboardPolicy
{
	inline bool IsValidUTF8(std::string_view a_text)
	{
		for (std::size_t i = 0; i < a_text.size();) {
			const auto lead = static_cast<unsigned char>(a_text[i++]);
			if (lead < 0x80) continue;
			unsigned continuation{};
			std::uint32_t code{}, minimum{};
			if (lead >= 0xC2 && lead <= 0xDF) { continuation = 1; code = lead & 0x1F; minimum = 0x80; }
			else if (lead >= 0xE0 && lead <= 0xEF) { continuation = 2; code = lead & 0x0F; minimum = 0x800; }
			else if (lead >= 0xF0 && lead <= 0xF4) { continuation = 3; code = lead & 0x07; minimum = 0x10000; }
			else return false;
			if (a_text.size() - i < continuation) return false;
			while (continuation--) {
				const auto next = static_cast<unsigned char>(a_text[i++]);
				if ((next & 0xC0) != 0x80) return false;
				code = (code << 6) | (next & 0x3F);
			}
			if (code < minimum || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) return false;
		}
		return true;
	}

    enum class BufferResult { Skipped, Applied, Invalid, TooLong };
    // Buffered backends may not emit per-key overlay events. Read their own
    // full draft once per UI poll, including an empty draft after deleting all
    // text. Never query this API for minimal/streamed SteamVR sessions.
    template <class Reader>
    inline BufferResult ReadBufferedDraft(bool streamed, std::string& draft, Reader&& reader)
    {
        if (streamed) return BufferResult::Skipped;
        constexpr std::size_t maximumBytes = 255;
        std::array<char, maximumBytes * 4 + 1> buffer;
        buffer.fill('\x7F');
        const auto count = reader(buffer.data(), static_cast<std::uint32_t>(buffer.size()));
        const auto* end = static_cast<const char*>(std::memchr(buffer.data(), '\0', buffer.size()));
        if (count > buffer.size() || !end || end - buffer.data() > maximumBytes)
            return BufferResult::TooLong;
        const std::string candidate(buffer.data(), static_cast<std::size_t>(end - buffer.data()));
        if (!IsValidUTF8(candidate)) return BufferResult::Invalid;
        draft = candidate;
        return BufferResult::Applied;
    }

    enum class EditResult { Applied, Cancel, Invalid, TooLong };
    // Minimal-mode payloads are bounded UTF-8 chunks, not whole drafts. Apply
    // atomically; backspace removes a Unicode scalar, never a continuation byte.
    inline EditResult EditDraft(std::string& draft, std::string_view input, std::size_t maxBytes = 255)
    {
        if (!IsValidUTF8(input)) return EditResult::Invalid;
        auto candidate = draft;
        for (std::size_t i = 0; i < input.size();) {
            const auto byte = static_cast<unsigned char>(input[i]);
            if (byte == 0x1B) return EditResult::Cancel;
            if (byte == '\b') {
                if (!candidate.empty()) {
                    auto start = candidate.size() - 1;
                    while (start && (static_cast<unsigned char>(candidate[start]) & 0xC0) == 0x80) --start;
                    candidate.resize(start);
                }
                ++i;
            } else {
                if (byte < 0x20 || byte == 0x7F) return EditResult::Invalid;
                const auto count = byte < 0x80 ? 1U : byte < 0xE0 ? 2U : byte < 0xF0 ? 3U : 4U;
                candidate.append(input.substr(i, count));
                i += count;
            }
        }
        if (candidate.size() > maxBytes) return EditResult::TooLong;
        draft = std::move(candidate);
        return EditResult::Applied;
    }
}
