# VR Address Library follow-up to PR66

This topic follows PR66 head `6c254bf4b94d711bb59fe7caac822bb62bc6f916`.
It is independent of PR80's export hardening. The inherited VR adapter and its
ABI/signature qualifications remain dependencies; resolving an Address Library
ID does not establish an engine prototype or make an instruction patch portable.
There is no newly diagnosed in-the-field fault attributed to this migration.

## Required library

VR builds from this topic require **VR Address Library 0.294.0 or later**.
The compiled baseline is 0.294.0. The exact qualified release CSV is bound by
version, size and SHA-256 in `tests/VR/address-library-baseline.json`.
The external library is not bundled into this mod.

The 52 entries in `skee64/VR/AddressLibrary.h` resolve to the qualified VR 1.4.15
addresses in that release. The third `REL::RelocationID` component is the VR/SSE
identifier, not the AE identifier. Flat wrappers retain their existing IDs and
pointer types. Existing helper-entry windows, CALL-target checks, local patch
offsets, hook toggles, trampoline allocation and commit ordering are preserved.

## Completed mapping gaps

The 19 previously documented helper/hook-anchor gaps are now covered. Eighteen
new mappings were accepted in Address Library PRs308–314; head preprocessing
ID24227 was already contributed in PR304. All are present in the published
[0.294.0 release](https://github.com/alandtse/skyrim_vr_address_library/releases/tag/v0.294.0).
The existing MenuCursor data ID517043 is resolved during VR 1.4.15 runtime
qualification. A missing entry or a resolved pointer slot outside the data
segment disables RaceMenu pointer hooks. Input dispatch uses the validated
pointer slot with its original type and local cursor layout.

Two old dynamic-geometry anchors were interior addresses, not function entries:

| Hook | Released function entry | Local offset | Same final CALL RVA |
|---|---|---|---|
| Resize free | ID69569 / 0xCB8830 | +0x1E | 0xCB884E |
| Resize allocate | ID69569 / 0xCB8830 | +0x25 | 0xCB8855 |
| Deleting destructor free | ID69573 / 0xCB9010 | +0x2F | 0xCB903F |

This replaces the old 0xCB8800/+0x4E,+0x55 and 0xCB9000/+0x3F representations.
It does not move a patch site or change the instruction bytes being qualified.
The catalogue records the real released entries; no ID is invented for an
interior address. Qualified pointer signatures remain unchanged even where the
upstream database has since acquired a descriptive function name.

## Remaining exact-version exceptions

The following separate data/input/loader endpoints still have no exact entry
in the 0.294.0 database. They retain their existing VR 1.4.15 paths and guards;
this topic does not claim that every raw address in the whole adapter is gone.
No distant ID or inferred function signature is substituted.

| VR RVA / anchor | Role |
|---|---|
| 0x01EA71B0 | Preprocessed-head data flag |
| 0x03013620 | Scaleform event-pool global |
| 0x00F37680 | Queue GFx mouse event |
| 0x00F209A0 | Dispatch GFx event |
| 0x00F217F0 | Memory-file constructor |
| 0x008DD500 + 0x66 | RaceSexMenu movie-load CALL site |

Those entries need their own exact mapping qualification before a later
conversion. The completed 19-entry migration does not loosen the retained
runtime, range, original-target or instruction-signature checks.

## Validation

Run the read-only checks with the actual 0.294.0 release CSV:

```text
CODEX_PYTHON tests/VR/address-library.test.py <version-1-4-15-0.csv> 6c254bf4b94d711bb59fe7caac822bb62bc6f916
```

The checks bind the compiled baseline to the complete release-file identity,
check all 52 mappings against the independent CSV, compare the 27 helper windows
and all 21 CALL patch site/target pairs with the original qualified source,
and verify the extra armor/head-preprocessing/inventory/cache/cursor addresses.
They preserve instruction-byte arrays and function/data pointer types and reject
a reintroduced mapped RVA exception.

These are source/data equivalence checks, not native compilation or a loaded
plugin/game test. Native build and automatic source review have separate exact
receipts. This change does not compile or activate a different SWF, alter a
dependency pin, install a mod, or publish a package.
