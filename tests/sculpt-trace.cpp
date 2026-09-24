#include "SculptTrace.h"
#include "SculptHistoryDispatch.h"
#include <string>
#include <iostream>
#include <stdexcept>
using namespace SKEE::SculptTrace;
void Check(bool ok) { if (!ok) throw std::runtime_error("Sculpt trace assertion failed"); }
int main()
{
    for (bool vr : {false, true}) {
        unsigned invocations = 0;
        const std::string action = "opaque-action-object";
        auto invoke = [&](const char* method, const std::string* args, std::uint32_t count) {
            ++invocations;
            Check(std::string(method) == (vr ? "call" : "AddAction"));
            Check(count == (vr ? 2u : 1u));
            if (vr) Check(args[0] == "AddAction");
            Check(args[vr ? 1 : 0] == action);
            return false;
        };
        Check(!SKEE::DispatchSculptHistory(vr, action, invoke));
        Check(invocations == 1); // No fallback that could duplicate a History entry.
    }
    Count(Event::HistoryQueued); Check(Read().samples[static_cast<unsigned>(Event::HistoryQueued)].calls == 0);
    Start(); const auto first = Read(); Check(first.active && first.generation > 0);
    Count(Event::StrokeCommit, 12); Count(Event::UndoPush); Count(Event::HistoryQueued);
    { Scope scope(Event::Paint, 3); }
    auto result = Read(true);
    Check(!result.active && result.samples[static_cast<unsigned>(Event::StrokeCommit)].units == 12);
    Check(result.samples[static_cast<unsigned>(Event::HistoryQueued)].calls == 1);
    Check(result.samples[static_cast<unsigned>(Event::Paint)].calls == 1);
    Count(Event::HistoryRun); Check(Read().samples[static_cast<unsigned>(Event::HistoryRun)].calls == 0);
    Start(); Check(Read().generation == first.generation + 1);
    Check(Read().samples[static_cast<unsigned>(Event::UndoPush)].calls == 0);
    { std::lock_guard guard(mutex); deadline = Clock::now(); }
    Count(Event::PointerUp); Check(!Read().active && Read().samples[static_cast<unsigned>(Event::PointerUp)].calls == 0);
    Start();
    { Scope old(Event::Hover); Start(); }
    Check(Read(true).samples[static_cast<unsigned>(Event::Hover)].calls == 0);
    std::cout << "Sculpt trace and History dispatch: off/reset/stop/deadline/generation, counters, VR envelope and unchanged non-VR route passed.\n";
}
