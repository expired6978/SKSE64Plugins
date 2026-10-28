# Sculpt session/history qualification

Opt-in `SKEE_BUILD_SCULPT_SESSION_TESTS=ON` registers `sculpt-stroke-session`
and `sculpt-history` CTest targets with 30-second timeouts. Compile through the
qualified Build Broker only. Neither target has yet been compiled or executed.

`sculpt-history` links the production CDXUndo.cpp, not a duplicate history model.
It covers copied numeric IDs, invalid capture, ordinary append retention, cursor
movement, branch trimming, capacity eviction, release/reuse, same command reuse,
weak task lifetime and a simulated GFx preparation re-entry/final check. A real
SculptStrokeSession helper and real history stack with mock brush/geometry cover
finalization before import baseline inspection and subsequent undo/redo ordering.

These are **not** production GFx, scene or NIF import integration tests. The
actual LoadImportedHead boundary and both deferred task final checks require an
exact-head plugin build and engine qualification. Source inspection confirms the
EndPaint call precedes import inspection and construction; it is not runtime proof.

History owns per-insertion weak identities and a structural revision. Trim,
eviction and Release invalidate old publication tickets; ordinary appends and
cursor-only changes do not. Dropped stale metadata is fail-closed, not a promise
to rebuild all previously displayed history after arbitrary queue delays.

Threading remains the existing editor-owner contract: edit callbacks, scene
teardown and UI publication must be serialized on that owner. History is private
composition so callers cannot bypass ticket invalidation through vector mutation.
No new mutex is held across GFx or engine/renderer calls (which would risk lock
inversion). Concurrent cross-thread editor mutation is **not** made safe by weak
tickets, and engine/task serialization has not been qualified. Before accepting
this candidate, verify that contract or implement an owner-thread dispatch policy;
do not infer it from the standalone sequential tests.
