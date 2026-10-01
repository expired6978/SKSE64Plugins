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
        std::cout << "Head-part slider index policy passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
