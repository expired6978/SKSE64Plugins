#include "../skee64/VRHookQualificationPolicy.h"
#include "../skee64/VRHookTransactionPolicy.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

int main()
{
	using namespace SKEE::VR::HookQualification;

	auto noPointer = [](std::uintptr_t, std::uintptr_t&) { return false; };
	const std::array relativeJump{
		std::uint8_t{ 0xE9 }, std::uint8_t{ 0xFB }, std::uint8_t{ 0x00 }, std::uint8_t{ 0x00 }, std::uint8_t{ 0x00 }
	};
	auto branch = DecodeEntryBranch(0x1000, relativeJump, noPointer);
	assert(branch && branch->kind == EntryBranchKind::kRelativeJump && branch->target == 0x1100);

	const std::array shortJump{ std::uint8_t{ 0xEB }, std::uint8_t{ 0xFE } };
	branch = DecodeEntryBranch(0x2000, shortJump, noPointer);
	assert(branch && branch->kind == EntryBranchKind::kShortJump && branch->target == 0x2000);

	const std::array ripIndirectJump{
		std::uint8_t{ 0xFF }, std::uint8_t{ 0x25 }, std::uint8_t{ 0x10 },
		std::uint8_t{ 0x00 }, std::uint8_t{ 0x00 }, std::uint8_t{ 0x00 }
	};
	branch = DecodeEntryBranch(
		0x3000,
		ripIndirectJump,
		[](std::uintptr_t a_slot, std::uintptr_t& a_target) {
			if (a_slot != 0x3016) return false;
			a_target = 0x9000;
			return true;
		});
	assert(branch && branch->kind == EntryBranchKind::kRipIndirectJump && branch->target == 0x9000);

	std::array<std::uint8_t, 12> absoluteRaxJump{ 0x48, 0xB8 };
	const std::uintptr_t absoluteTarget = 0x123456789ABCDEF0ULL;
	std::memcpy(absoluteRaxJump.data() + 2, &absoluteTarget, sizeof(absoluteTarget));
	absoluteRaxJump[10] = 0xFF;
	absoluteRaxJump[11] = 0xE0;
	branch = DecodeEntryBranch(0x4000, absoluteRaxJump, noPointer);
	assert(branch && branch->kind == EntryBranchKind::kAbsoluteRaxJump && branch->target == absoluteTarget);

	std::array<std::uint8_t, 13> absoluteR11Jump{ 0x49, 0xBB };
	std::memcpy(absoluteR11Jump.data() + 2, &absoluteTarget, sizeof(absoluteTarget));
	absoluteR11Jump[10] = 0x41;
	absoluteR11Jump[11] = 0xFF;
	absoluteR11Jump[12] = 0xE3;
	branch = DecodeEntryBranch(0x4800, absoluteR11Jump, noPointer);
	assert(branch && branch->kind == EntryBranchKind::kAbsoluteR11Jump && branch->target == absoluteTarget);

	const std::array ordinaryProlog{
		std::uint8_t{ 0x48 }, std::uint8_t{ 0x89 }, std::uint8_t{ 0x5C }, std::uint8_t{ 0x24 }, std::uint8_t{ 0x10 }
	};
	assert(!DecodeEntryBranch(0x5000, ordinaryProlog, noPointer));

	using namespace SKEE::VR::HookTransaction;

	RelativeCall original{};
	assert(EncodeRelativeCall(0x1000, 0x2400, original));
	std::uintptr_t decoded{};
	assert(DecodeRelativeCall(0x1000, original, decoded));
	assert(decoded == 0x2400);
	assert(!EncodeRelativeCall(0x1000, 0x100000000ULL, original));

	std::uintptr_t firstSite = 0x1111;
	constexpr std::uintptr_t originalFirst = 0x1111;
	constexpr std::uintptr_t hookFirst = 0x2222;
	RelativeCall callSite{};
	assert(EncodeRelativeCall(0x4000, 0x5000, callSite));
	const auto qualifiedCall = callSite;
	RelativeCall hookCall{};
	assert(EncodeRelativeCall(0x4000, 0x6000, hookCall));

	// Failure injection: another owner changes the call after qualification but
	// before commit. The second site rejects the stale expectation and the first
	// site is restored, leaving neither of our hooks resident.
	callSite[4] ^= 0x40;
	const auto competingCall = callSite;
	auto result = Commit(
		[&] {
			if (firstSite != originalFirst) return false;
			firstSite = hookFirst;
			return true;
		},
		[&] {
			if (callSite != qualifiedCall) return false;
			callSite = hookCall;
			return true;
		},
		[&] {
			if (firstSite != hookFirst) return false;
			firstSite = originalFirst;
			return true;
		});
	assert(result == Result::kSecondSiteRejectedRolledBack);
	assert(firstSite == originalFirst);
	assert(callSite == competingCall);

	// Uncontested control: both sites install exactly once.
	callSite = qualifiedCall;
	result = Commit(
		[&] { firstSite = hookFirst; return true; },
		[&] { callSite = hookCall; return true; },
		[&] { firstSite = originalFirst; return true; });
	assert(result == Result::kInstalled);
	assert(firstSite == hookFirst);
	assert(callSite == hookCall);

	std::puts("VR entry-detour and two-site hook transaction tests passed.");
}
