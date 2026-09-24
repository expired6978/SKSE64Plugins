// SPDX-License-Identifier: GPL-3.0-or-later
#include "AvatarLightingPolicy.h"
#include <iostream>
#include <stdexcept>
int main()
{
    using namespace SKEE::AvatarLightingPolicy;
    const auto check = [](bool b) { if (!b) throw std::runtime_error("Avatar lighting policy failed"); };
    check(Number("nan", 1, 0, 3) == 1); check(Number("inf", 1, 0, 3) == 1);
    check(Number("4", 1, 0, 3) == 1); check(Number("1garbage", 2, 0, 3) == 2);
    check(Number("", 1, 0, 3) == 1); check(Number("0", 1, 0, 3) == 0);
    check(Number("0.45 \t", 1, 0, 3) == 0.45F);
    check(Position(defaults[0], 130)[2] == 145);
    check(Position(defaults[1], 130)[2] == 95);
    check(Position(defaults[2], 130)[2] == 90);
    for (auto s : defaults) check(Position(s, 130)[1] > 0 && s.radius >= s.front);
    check(WorldRadius(defaults[0], 1) == 220);
    check(WorldRadius(defaults[1], 0.5F) == 95);
    check(WorldRadius(defaults[2], 1.25F) == 375);
    auto edge = defaults[0]; edge.radius = 50; check(WorldRadius(edge, 0.1F) == 5);
    edge.radius = 800; check(WorldRadius(edge, 10) == 8000);
    int movie{}, replacement{};
    Requests requests;
    requests.SetDesired(true); check(requests.TryQueue());
    requests.Complete(); check(requests.Desired());
    check(requests.TryQueue()); // periodic refresh already waiting
    requests.SetDesired(false); check(!requests.TryQueue());
    check(!requests.Desired()); // that worker must turn OFF, not refresh ON
    requests.SetDesired(true); check(!requests.TryQueue());
    requests.SetDesired(false); check(!requests.TryQueue());
    requests.Complete(); check(!requests.Desired()); // last explicit click wins
    check(requests.TryQueue()); // another refresh must not alter OFF intent
    check(!requests.Desired()); requests.Reset();
    check(!requests.Desired()); check(requests.TryQueue());
    check(SessionMatches(1, 1, &movie, &movie));
    check(!SessionMatches(1, 2, &movie, &movie));
    check(!SessionMatches(1, 1, &movie, &replacement));
    check(!SessionMatches(1, 1, nullptr, nullptr));
    std::cout << "Avatar lighting configuration, front/height geometry and session policy passed\n";
}
