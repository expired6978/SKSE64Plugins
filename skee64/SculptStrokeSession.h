#pragma once
#include <utility>

// Non-owning stroke session; the scene ends it before deleting brushes/meshes.
// Clearing the owner before callbacks makes repeated/re-entrant release safe.
template <class Brush>
class SculptStrokeSession
{
    Brush* owner{};
public:
    Brush* Get() const { return owner; }
    void Begin(Brush* next) { End(); owner = next; }
    void End() { if (auto previous = std::exchange(owner, nullptr)) previous->EndStroke(); }
};
