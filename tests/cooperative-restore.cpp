#include "../skee64/CooperativeRestorePolicy.h"

#include <cassert>
#include <cstdio>

int main()
{
	using namespace SKEE::VR::CooperativeRestore;
	const auto equal = [](float a, float b) { return Near(a, b); };

	// Uncontested control restores the captured baseline.
	float current = 2.0F;
	const float applied = 2.0F;
	const float baseline = 1.0F;
	if (ShouldRestore(current, applied, true, equal)) current = baseline;
	assert(Near(current, baseline));

	// A competing writer's newer value survives cleanup.
	current = 3.0F;
	if (ShouldRestore(current, applied, true, equal)) current = baseline;
	assert(Near(current, 3.0F));

	// Even matching values are not restored after identity/topology changes.
	current = applied;
	if (ShouldRestore(current, applied, false, equal)) current = baseline;
	assert(Near(current, applied));

	std::puts("Cooperative placement restoration tests passed.");
}
