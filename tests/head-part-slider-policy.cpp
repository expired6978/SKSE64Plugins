// SPDX-License-Identifier: GPL-3.0-or-later
#include "HeadPartSliderPolicy.h"

#include <iostream>
#include <limits>
#include <stdexcept>

int main()
{
    using namespace SKEE::HeadPartSlider;
    const auto check = [](bool result, const char* message) {
        if (!result) throw std::runtime_error(message);
    };

    try {
        check(ValueForPartIndex(-1) == kNoPart, "absent head part did not initialise to -1");
        check(ValueForPartIndex(0) == 0.0f, "first head part did not initialise to zero");
        check(ValueForPartIndex(4) == 4.0f, "later head part index changed");
        check(PartIndexForValue(0.0, 5) == 0, "first part could not be selected");
        check(PartIndexForValue(4.0, 5) == 4, "last part could not be selected");
        check(!PartIndexForValue(kNoPart, 5), "no-part sentinel became a list index");
        check(!PartIndexForValue(0.0, 0), "empty list accepted an index");
        check(!PartIndexForValue(5.0, 5), "upper bound accepted an index");
        check(!PartIndexForValue(1.5, 5), "fractional slider value accepted an index");
        check(!PartIndexForValue(std::numeric_limits<double>::quiet_NaN(), 5), "NaN accepted an index");
        check(!PartIndexForValue(std::numeric_limits<double>::infinity(), 5), "infinity accepted an index");
        check(!PartIndexForValue(-2.0, 5), "non-sentinel negative value accepted an index");
        check(!PartIndexForValue(-std::numeric_limits<double>::infinity(), 5), "negative infinity accepted an index");
        check(!PartIndexForValue(4294967296.0, std::numeric_limits<std::size_t>::max()), "uint32 overflow accepted an index");

        // Exercise the same mutation boundary used by DoubleMorphCallback_Hook,
        // with injected base/read-back/rebuild operations rather than the game.
        int oldPart = 0, selectedPart = 1, defaultPart = 2;
        int* currentPart = &oldPart;
        float reportedValue = 3.0f;
        int rebuilds = 0;
        const auto readPart = [&]() { return currentPart; };
        const auto noChange = [](int*) {};
        const auto rebuild = [&](int*, int*) { ++rebuilds; };
        check(!ApplySelection(reportedValue, 0.0f, &selectedPart, readPart, noChange, rebuild), "no-op selection reported success");
        check(reportedValue == 3.0f && rebuilds == 0, "failed selection changed value or rebuilt actor");
        check(!ApplySelection(reportedValue, kNoPart, &defaultPart, readPart, noChange, rebuild), "no-op clear reported success");
        check(reportedValue == 3.0f && rebuilds == 0, "failed clear changed value or rebuilt actor");
        const auto change = [&](int* part) { currentPart = part; };
        check(ApplySelection(reportedValue, 0.0f, &selectedPart, readPart, change, rebuild), "selection did not commit confirmed part");
        check(reportedValue == 0.0f && currentPart == &selectedPart && rebuilds == 1, "selection state/rebuild mismatch");
        check(ApplySelection(reportedValue, 0.0f, &selectedPart, readPart, noChange, rebuild), "existing selection did not commit");
        check(rebuilds == 1, "existing selection unnecessarily rebuilt actor");
        check(ApplySelection(reportedValue, kNoPart, &defaultPart, readPart, change, rebuild), "default clear did not commit");
        check(reportedValue == kNoPart && currentPart == &defaultPart && rebuilds == 2, "clear state/rebuild mismatch");
        check(ApplySelection(reportedValue, kNoPart, &defaultPart, readPart, noChange, rebuild), "reopened default did not remain clear");
        check(rebuilds == 2, "reopened default unnecessarily rebuilt actor");
        check(!ApplySelection(reportedValue, 0.0f, &selectedPart, readPart, change,
            [&](int*, int*) { currentPart = &oldPart; }), "post-rebuild mismatch reported success");
        check(reportedValue == kNoPart, "post-rebuild mismatch committed requested value");
        currentPart = nullptr;
        check(ApplySelection(reportedValue, kNoPart, static_cast<int*>(nullptr), readPart, noChange, rebuild), "already absent part did not clear");
        check(reportedValue == kNoPart && rebuilds == 2, "absent part changed value or rebuilt actor");
        std::cout << "Head-part slider index policy passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
