#include "SculptStrokeSession.h"
#include <functional>
#include <stdexcept>
#include <iostream>
struct Brush { unsigned ends{}; std::function<void()> callback; void EndStroke() { ++ends; if (callback) callback(); } };
void Check(bool ok) { if (!ok) throw std::runtime_error("Sculpt stroke ownership assertion failed"); }
int main()
{
    SculptStrokeSession<Brush> session;
    Brush a, b;
    session.End(); Check(!session.Get()); // orphan release
    session.Begin(&a); Check(session.Get()==&a && a.ends==0);
    session.Begin(&a); Check(session.Get()==&a && a.ends==1); // duplicate begin
    session.Begin(&b); Check(session.Get()==&b && a.ends==2 && b.ends==0);
    b.callback=[&]{ Check(!session.Get()); session.End(); }; // re-entrant release
    session.End(); session.End(); Check(!session.Get() && b.ends==1);
    session.Begin(&a); session.Begin(nullptr); Check(!session.Get() && a.ends==3);
    session.Begin(&b); session.End(); Check(b.ends==2); // close before owner destruction
    std::cout << "Sculpt stroke ownership: orphan/repeated releases, duplicate presses, brush replacement and re-entrancy passed.\n";
}
