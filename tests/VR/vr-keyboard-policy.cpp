#include "RaceSexMenuVRKeyboardPolicy.h"
#include <iostream>
#include <string>

int main()
{
	using SKEE::VR::KeyboardPolicy::IsValidUTF8;
	unsigned failures{};
	const auto check = [&](std::string_view text, bool expected, const char* name) {
		if (IsValidUTF8(text) != expected) { std::cerr << "FAIL " << name << '\n'; ++failures; }
	};
	check("", true, "empty");
	check("Prisoner", true, "ascii");
	check("\xC3\xA9", true, "two_byte");
	check("\xE2\x82\xAC", true, "three_byte");
	check("\xF0\x9F\x98\x80", true, "four_byte");
	check("\xF4\x8F\xBF\xBF", true, "maximum_scalar");
	check("\x80", false, "orphan_continuation");
	check("\xC0\x80", false, "overlong_two");
	check("\xE0\x80\x80", false, "overlong_three");
	check("\xF0\x80\x80\x80", false, "overlong_four");
	check("\xED\xA0\x80", false, "surrogate");
	check("\xF4\x90\x80\x80", false, "above_unicode_maximum");
	check("\xF5\x80\x80\x80", false, "invalid_lead");
	check("\xF0\x9F\x98", false, "truncated");
	check("\xE2\x28\xAC", false, "bad_continuation");
	std::string multi;
	for (unsigned i = 0; i < 128; ++i) multi += "\xC3\xA9";
	check(multi, true, "256_byte_valid_unicode");
	if (multi.size() != 256) ++failures;
    using SKEE::VR::KeyboardPolicy::ReadBufferedDraft;
    using SKEE::VR::KeyboardPolicy::BufferResult;
    std::string buffered = "initial", authoritative = "OCU";
    unsigned reads{};
    const auto reader = [&](char* output, std::uint32_t capacity) {
        ++reads;
        if (authoritative.size() + 1 > capacity) return capacity + 1;
        std::memcpy(output, authoritative.c_str(), authoritative.size() + 1);
        return static_cast<std::uint32_t>(authoritative.size());
    };
    const auto bufferCheck = [&](bool streamed, BufferResult expected, std::string_view text) {
        if (ReadBufferedDraft(streamed, buffered, reader) != expected || buffered != text)
            { ++failures; std::cerr << "FAIL buffered draft\n"; }
    };
    // No character events needed: each UI poll reflects the current full buffer.
    bufferCheck(false, BufferResult::Applied, "OCU");
    authoritative = "OCU edit"; bufferCheck(false, BufferResult::Applied, authoritative);
    authoritative = "OCU edi"; bufferCheck(false, BufferResult::Applied, authoritative);
    authoritative.clear(); bufferCheck(false, BufferResult::Applied, "");
    authoritative = "\xC3\xA9"; bufferCheck(false, BufferResult::Applied, authoritative);
    const auto beforeStreamed = reads;
    authoritative = "last-key"; bufferCheck(true, BufferResult::Skipped, "\xC3\xA9");
    if (reads != beforeStreamed) ++failures;
    authoritative.assign(256, 'a'); bufferCheck(false, BufferResult::TooLong, "\xC3\xA9");
    authoritative = "\xC3"; bufferCheck(false, BufferResult::Invalid, "\xC3\xA9");
    if (ReadBufferedDraft(false, buffered, [](char*, std::uint32_t) { return 0U; }) != BufferResult::TooLong || buffered != "\xC3\xA9") ++failures;
    using SKEE::VR::KeyboardPolicy::EditDraft;
    using SKEE::VR::KeyboardPolicy::EditResult;
    const auto edit = [&](std::string& draft, std::string_view input, EditResult expected, std::string_view text) {
        if (EditDraft(draft, input) != expected || draft != text) { ++failures; std::cerr << "FAIL draft edit\n"; }
    };
    std::string draft;
    edit(draft, "H", EditResult::Applied, "H");
    edit(draft, "ero", EditResult::Applied, "Hero");
    edit(draft, "\b", EditResult::Applied, "Her");
    edit(draft, "\xC3\xA9", EditResult::Applied, "Her\xC3\xA9");
    edit(draft, "\b", EditResult::Applied, "Her");
    edit(draft, "\xF0\x9F\x98\x80", EditResult::Applied, "Her\xF0\x9F\x98\x80");
    edit(draft, "\b", EditResult::Applied, "Her");
    edit(draft, "\x1b", EditResult::Cancel, "Her");
    edit(draft, "\xC3", EditResult::Invalid, "Her");
    edit(draft, "\n", EditResult::Invalid, "Her");
    edit(draft, std::string("ab\0cd",5), EditResult::Invalid, "Her");
    draft.clear(); edit(draft, "\b", EditResult::Applied, "");
    draft.assign(254, 'a');
    const auto limit = draft;
    edit(draft, "\xC3\xA9", EditResult::TooLong, limit);
    edit(draft, "b", EditResult::Applied, limit + "b");
    edit(draft, "c", EditResult::TooLong, limit + "b");
    edit(draft, "\bZ", EditResult::Applied, limit + "Z");
	if (failures) return 1;
	std::cout << "PASS: native VR keyboard UTF-8 policy cases (not runtime keyboard qualification)\n";
	return 0;
}
