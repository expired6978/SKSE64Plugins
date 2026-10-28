#include "CDXUndo.h"
#include "SculptStrokeSession.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void Check(bool result) { if (!result) throw std::runtime_error("Sculpt history assertion failed"); }
struct Command : CDXUndoCommand {
    unsigned undos{}, redos{};
    void Undo() override { ++undos; }
    void Redo() override { ++redos; }
};
struct Pending {
    CDXUndoStack::Ticket ticket;
    int copiedId;
    bool Publish(CDXUndoStack& stack, const std::function<void()>& prepare) const {
        if (!stack.IsCurrent(ticket)) return false;
        prepare(); // Simulates re-entry during GFx CreateObject/SetMember.
        return stack.IsCurrent(ticket); // Production tasks recheck before Invoke.
    }
};
Pending Queue(CDXUndoStack& stack, const std::shared_ptr<Command>& command) {
    const auto id = stack.Push(command);
    return {stack.Capture(command.get(), id), id};
}
}

int main()
{
    CDXUndoStack stack;
    auto a = std::make_shared<Command>();
    auto b = std::make_shared<Command>();
    auto pa = Queue(stack, a);
    auto pb = Queue(stack, b);
    Check(pa.copiedId == 0 && pb.copiedId == 1);
    Check(pa.Publish(stack, [] {}) && pb.Publish(stack, [] {})); // ordinary append
    Check(!stack.IsCurrent(stack.Capture(a.get(), 1))); // mismatched command/slot
    Check(!stack.IsCurrent(stack.Capture(a.get(), -1)));
    Check(!stack.IsCurrent(stack.Capture(a.get(), 100)));
    Check(stack.Undo(true) == 0 && b->undos == 1);
    Check(stack.IsCurrent(pa.ticket) && stack.IsCurrent(pb.ticket)); // cursor only
    Check(stack.Redo(true) == 1 && b->redos == 1);
    Check(stack.Undo(false) == 0 && b->undos == 1);
    auto replacement = Queue(stack, std::make_shared<Command>());
    Check(replacement.copiedId == 1); // exact numeric reuse after branch trim
    Check(!stack.IsCurrent(pa.ticket) && !stack.IsCurrent(pb.ticket)); // revision
    Check(stack.IsCurrent(replacement.ticket));
    Check(!replacement.Publish(stack, [&] { stack.Release(); })); // final validation
    Check(stack.GetIndex() == -1 && !stack.IsCurrent(replacement.ticket));
    auto reused = Queue(stack, b); // same object, same index after release is not same ticket
    Check(!stack.IsCurrent(pb.ticket) && stack.IsCurrent(reused.ticket));
    Check(stack.Push(nullptr) == -1 && stack.IsCurrent(reused.ticket));

    stack.Release();
    std::vector<Pending> pending;
    for (unsigned i = 0; i < stack.GetLimit(); ++i) {
        pending.push_back(Queue(stack, std::make_shared<Command>()));
    }
    Check(stack.IsCurrent(pending.front().ticket) && stack.IsCurrent(pending.back().ticket));
    auto evicted = Queue(stack, std::make_shared<Command>());
    Check(evicted.copiedId == static_cast<int>(stack.GetLimit()) - 1);
    for (const auto& p : pending) Check(!stack.IsCurrent(p.ticket));
    Check(stack.IsCurrent(evicted.ticket));
    Check(!evicted.Publish(stack, [&] { Queue(stack, std::make_shared<Command>()); }));

    stack.Release();
    auto held = std::make_shared<Command>();
    std::weak_ptr<Command> lifetime = held;
    const auto weakTicket = Queue(stack, held);
    held.reset();
    Check(!lifetime.expired());
    stack.Release();
    Check(lifetime.expired() && !stack.IsCurrent(weakTicket.ticket));

    // Real session helper + real undo stack, mock brush/geometry: finish stroke
    // before the import inspects its baseline, then preserve undo/redo order.
    std::vector<int> order;
    struct Brush {
        CDXUndoStack& history;
        std::vector<int>& order;
        std::shared_ptr<Command> stroke;
        void EndStroke() { order.push_back(1); history.Push(stroke); }
    } brush{stack, order, std::make_shared<Command>()};
    SculptStrokeSession<Brush> session;
    session.Begin(&brush);
    session.End(); // same ordering as LoadImportedHead's entry boundary
    Check(!session.Get() && stack.GetIndex() == 0);
    order.push_back(2); // import baseline inspection after finalized brush
    auto imported = std::make_shared<Command>();
    Check(stack.Push(imported) == 1);
    Check(order == std::vector<int>({1, 2}));
    Check(stack.Undo(true) == 0 && imported->undos == 1 && brush.stroke->undos == 0);
    Check(stack.Undo(true) == -1 && brush.stroke->undos == 1);
    Check(stack.Redo(true) == 0 && brush.stroke->redos == 1);
    Check(stack.Redo(true) == 1 && imported->redos == 1);
    std::cout << "Production history identities, append/trim/eviction, final validation, weak lifetime and helper import order passed.\n";
}
