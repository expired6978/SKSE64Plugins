# Head export review follow-up

This is code-review-driven robustness work following PR74. There is no confirmed
in-the-field incident attributable to these particular malformed-layout,
allocation-failure, or partial-output cases. It does not claim to fix an observed
new crash. The earlier, separately diagnosed export-clone aliasing crash is not
evidence that these additional cases occurred in the field.

The clean comparison base is upstream `eb39bdd185f0ee5b3154b2ae1910d2465b2627ed`.
That accepted upstream state is not a claim of independent review coverage over
the entire tree. This topic owns only export robustness and the coupled internal
skin-copy caller. It does not contain the independent VR address migration.

## Behaviour

- Validate matching skin-data/matrix bone counts and required pointer arrays
  before copying. Check allocations and factories, release partial buffers and
  retain the source critical-section lock with RAII where the pinned layout has
  one. VR has no critical section at the flat layout's offset.
- Remap every export bone, retaining temporary ownership through failure paths.
  Refuse failed or aliased property/geometry clones and incomplete skinned heads.
  The overlay caller retains its previous skin when the shared helper refuses a
  copy. The helper now returns `NiPointer`; this is an internal C++ interface.
- Finish scene construction before writing anything. Serialize DDS/NIF into
  exclusively reserved sibling directories, check writer results and nonempty
  staged files, then install DDS followed by NIF using no-replace moves.
  If installation fails, roll back each installed member and restore backups.
  If rollback fails, preserve recovery files and log their directories.
- The Scaleform export return is `true` when queued, `false` when refused.
  Completion is separate: the deferred task logs success/failure and emits
  `RaceMenu_HeadExport`, with the NIF path in `strArg`, `numArg` equal to 1 for
  completed publication or 0 for failure, and the actor form as `sender` when
  still available. No new UI/HUD notification is promised by the event.

Two files are **not crash-atomic**. Process/power loss can interrupt publication;
unrelated external writers are not synchronized. Identity checks and no-replace
moves avoid overwriting a distinct competing destination during normal rollback,
but are not a general adversarial-filesystem guarantee. An exception before a
writer result is captured may retain its private staging file for recovery.

Engine deep-copy implementations still determine nested material/partition copy
semantics. The existing animation guard restores the engine's default expression,
not a captured arbitrary pre-export expression. This change does not claim to
prove those engine behaviours or arbitrary-thread export safety.

## Focused validation

`tests/head-export-policy.cpp` exercises the production count policy and generic
publication algorithm, including failure at every backup/install move, rollback
failure, competing/replaced destinations, staged identity mismatch and NIF-only
output. On Windows it additionally exercises the production filesystem wrapper
in an exclusive test directory, including existing-output replacement and an
empty-NIF failure that must preserve the previous pair.

The test has no game dependencies and must be compiled separately with the
repository's `skee64` include directory. It is intentionally not added to the
production build graph. Broker compilation and requester execution are separate
evidence. Source inspection or the presence of these cases is not an executed
test pass; exact build/test receipts are recorded with the PR/review disposition.
No live test, install, release or broader regression claim is implied.
