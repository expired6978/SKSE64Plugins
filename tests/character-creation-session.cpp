#include "../skee64/VRSessionLeasePolicy.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

enum class State { inactive, opening, ready, finishing };

int main()
{
	using SKEE::VR::SessionLease::IsCurrent;

	std::uint64_t generation = 4;
	State state = State::ready;
	const auto captured = generation;
	state = State::finishing;
	assert(IsCurrent(captured, generation, state, State::finishing));

	// Deterministic close/reopen before the queued task executes. The old task
	// must be abandoned and must not change the new session's ready state.
	state = State::inactive;
	++generation;
	state = State::opening;
	++generation;
	state = State::ready;
	const auto beforeOldTask = state;
	if (IsCurrent(captured, generation, state, State::finishing)) {
		state = State::inactive;
	}
	assert(state == beforeOldTask);

	// Same-session control remains eligible.
	const auto current = generation;
	state = State::finishing;
	assert(IsCurrent(current, generation, state, State::finishing));

	std::puts("Character-creation session lease tests passed.");
}
