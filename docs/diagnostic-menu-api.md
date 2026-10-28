# Draft: live menu diagnostics

Experimental reproduction interface, not a stable gameplay API. This draft is
not enabled or installed by this PR, is not part of PR66 or the equipment API,
and does not require Skyrim VR. The AS2 include contains original extension
code, not a redistributed upstream RaceMenu movie.

## Integration boundary

Attach the functions from `ui/diagnostics/Diagnostics.as.inc` to the live
RaceMenu owner on the UI thread. The consumer supplies the existing
`RaceMenuDefines`, GameDelegate and SKSE menu environment. Install lifecycle
invalidation on list/category rebuild, tab change and unload. Set
`diagnosticRebuilding` during a rebuild. An optional extension adapter can
supply `diagnosticExtensionDispatch(provider, control, value)`; it must use
that provider's menu callback, never direct actor writes.

Until source-SWF integration is qualified, this is a reviewable source draft,
not a compiled drop-in asset. Node tests execute the functions and mocked menu
collaborators; they do not prove GFx compilation or AE runtime integration.

## Contract

1. Invoke `RefreshDiagnosticControls()`. Read `diagnosticSnapshotJson`;
   require `status == "ready"` and take its `generation`.
2. Call `SetDiagnosticSlider(generation, slot, value)` for a listed enabled
   ordinary slider. Call `SelectDiagnosticSex(generation, slot, value)` for
   the listed sex selector, or `SelectDiagnosticRace(generation, raceId)`
   for an offered enabled race.
3. Read `diagnosticResultJson`. Success means the menu callback was
   dispatched, not that the game completed the change. Wait for the rebuild,
   query again, and verify resulting state.

Race selection uses the owner's actual `onItemPress` path. Slider and sex
actions use the existing GameDelegate callback and argument pair. Presets,
Sculpt, closed menus, loading/rebuilding states, disabled inputs and open
modals cannot be bypassed. A closed menu must be rejected by the caller's UI
transport as `menu-not-open`; the include cannot run without its owner.

Each query supersedes previous snapshots. A dispatched action consumes the
token before invoking the callback. Complete ordered entry/category object
identities plus frozen routing, range, availability and value fields are
checked before dispatch. A changed or reordered list requires a fresh query.
Finite values must match the live range and minimum-anchored step.

Snapshots include sliders (slot, ID, name, callback, range, step, current value,
enabled, category mask, action) and races (ID, name, enabled, active). Slots are
ephemeral, not persistent IDs. The stock All mask is 2044: it is not a guarantee
of every active slider, notably Race and separate paint/color categories.
Enumeration does not change the selected category. Paint dialogs, sculpt and
preset workflows are intentionally outside this interface.

Useful errors include `snapshot-required`, `stale-generation`,
`slider-identity-changed`, `sliders-tab-inactive`, `sliders-rebuilding`,
`race-change-pending`, `modal-open`, `slider-disabled`, `invalid-value`,
`race-disabled` and `callback-unavailable`. Results correlate the request
token and requested identity separately from the current generation.

## Open upstream decisions

Whether a diagnostic surface belongs in normal RaceMenu, which caller
permissions/opt-in policy to use, and its eventual public versioning and
loading mechanism remain open. Keep this PR draft. It is not a claim that
RaceMenu causes the intermittent crash being investigated.

Run source tests with `node --test tests/diagnostic-menu-api.test.cjs`.
