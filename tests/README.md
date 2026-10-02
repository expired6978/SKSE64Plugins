# Dynamic buffer transaction test

`tests/CMakeLists.txt` is a separate opt-in native test project for the exact
`AcquireDynamicData` and `ReplaceDynamicData` templates used by production.
It checks one-time locked snapshots, lock order, retained identity despite
subsequent source replacement, retain/release balance, abandoned/moved leases,
untracked and disabled-sharing copies, null/zero rejection, and failure-atomic
replacement of new/existing target buffers. It uses deterministic fake locks,
allocator storage and reference counts; it does not execute native engine hooks
or prove all third-party/engine writers obey the shape lock.

Compile through the registered Build Broker's separately qualified test lane.
Execute the returned test binary separately only after compilation is verified.
No broker lane for this standalone target is currently qualified, so the test
source/recipe is supplied without a build or test-pass claim. Do not use an
ad-hoc local compiler as a workaround.

Production acquisition retains a live `NiPointer` before reading the shape's
runtime fields. The shape data lock guards snapshot and copy; the allocation
registry lock then guards tracked increment or copied read relative to hooked
final free. Storage writers must honor the shape lock; an invalid raw geometry
passed at function entry or a writer freeing an owned buffer without updating
its owner/lock is outside that contract. No address-only scheme can make such
invalid input safe. Target replacement uses its shape lock, never holding
source and target shape locks together, and releases the superseded buffer
through the tracked/untracked release path.

Native runtime qualification must still check the engine's storage-update lock
contract and overlay refresh behavior, including first/third-person face/body
geometry and repeated reinstallation. The lease provides acquisition failure
atomicity, not rollback of every later shader/override operation.
