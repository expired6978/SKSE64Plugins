#include "../skee64/VRHookTransactionPolicy.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

int main()
{
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

	std::puts("VR two-site hook transaction tests passed.");
}
