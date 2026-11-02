# VR Address Library follow-up to PR66

This topic is separate from PR80's export robustness changes. Its comparison base
is PR66 head `6c254bf4b94d711bb59fe7caac822bb62bc6f916`; inherited PR66 code is a
dependency, not newly reviewed by this focused change. There is no intended hook
behaviour or ABI change and no new field-crash claim.

The 32 entries in `skee64/VR/AddressLibrary.h` have exact mappings in the VR
Address Library **0.276.1** release CSV. The published database at
`alandtse/skyrim_vr_address_library` commit
`70c3f5c9a9d6d8b3eb1b5ec8e8591c64c012a308` was checked independently for gaps.
The third `RelocationID` component is the VR/SSE identifier; these must not be
confused with the existing AE-keyed constants. Flat wrapper branches keep their
existing IDs. 0.276.1 is the verified compatibility baseline for this topic, not
a claim that all older releases are incompatible or that this is the earliest
release containing every mapping.

VR builds distributed from this topic require Address Library **0.276.1 or
later**. The compiled baseline diagnostic is 0.276.1, matching the exact release
CSV version, size and SHA-256 retained in
`tests/VR/address-library-baseline.json`. This replaces the inherited, unqualified
0.270.0 label; it is a compatibility-claim correction, not a newly observed
game fault or a claim that earlier releases necessarily fail.

Direct helper calls, helper-entry qualification and mapped hook anchors/targets
now resolve those IDs. `VRCallPatch` stores resolved absolute addresses, and both
validation and installation consume that same value. Qualified instruction and
CALL-target checks, VR-specific local offsets, ABI signatures, hook toggles,
trampoline setup and commit ordering are retained.

## Exact mapping gaps

This is a **partial migration**. Neither checked data source has an exact entry
for the following qualified VR addresses. Their existing RVA/signature paths
remain explicit; no substitute ID or distant unrelated anchor was invented.

| VR RVA | Role |
|---|---|
| 0x00175DA0 | Head-part list target |
| 0x001DB680 | Armor-node creation target |
| 0x00373740 | Head preprocessing anchor |
| 0x003E1E50 | Update-all-morphs caller |
| 0x003E3F20 | Race-morph caller |
| 0x008B60A0 | Inventory model setter |
| 0x008B6220 | New inventory model caller |
| 0x008DF4E0 | Secondary morph callback caller |
| 0x008E2DC0 | Category-list caller |
| 0x008E8930 | FaceGen part-cache entry |
| 0x00C69680 | Geometry allocator |
| 0x00C698E0 | Geometry deallocator |
| 0x00CB82B0 | Dynamic geometry copy |
| 0x00CB8390 | Dynamic geometry assignment |
| 0x00CB8600 | Dynamic geometry constructor |
| 0x00CB8700 | Dynamic geometry destructor |
| 0x00CB8800 | Dynamic geometry resize |
| 0x00CB9000 | Dynamic geometry deleting destructor |
| 0x00F342E0 | FxDelegate invoke target |

Full removal of these RVA paths requires validated exact mappings in the
upstream VR Address Library. The absence of those entries is a data prerequisite,
not evidence of a game fault or permission to manufacture mappings. The original
VR 1.4.15 qualification remains in force for these exceptions.

## Validation

Run the read-only mapping/equivalence checks with the actual release CSV and
comparison commit:

```text
CODEX_PYTHON tests/VR/address-library.test.py <version-1-4-15-0.csv> 6c254bf4b94d711bb59fe7caac822bb62bc6f916
```

These checks compare resolved helper addresses and pointer types, all 21 call
patch site/target pairs, instruction byte windows, and the mapping inventory.
They also bind the compiled baseline to the exact qualified CSV and print its
version and SHA-256 into the test receipt. A different CSV must be separately
qualified before changing that retained baseline.
They do not load the Address Library or plugin in a game and do not prove native
compilation, runtime input, graphics, or stability. Native compile and focused
review receipts are separate. No SWF/asset, installed profile, or release change
belongs to this topic.
