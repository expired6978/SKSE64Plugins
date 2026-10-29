# String table lifetime during normal DLL shutdown

This fork starts from official `expired6978/SKSE64Plugins` branch
`CommonLibSSE-NG`, commit `eb39bdd185f0ee5b3154b2ae1910d2465b2627ed`.
That source revision is not an attestation of the exact official B6 DLL build.
All existing GPL and third-party notices remain applicable.

The original main translation unit constructs BodyMorph/Override/FaceMorph and
other owners before `g_stringTable`. Normal CRT teardown destroys the table
first. Later release of retained interned strings invokes `DeleteStringEntry`,
which re-enters the already-destructed global table/mutex/map. This is a lifetime
error; reordering one owner or forcing process termination does not repair every
cross-translation-unit owner.

The table facade now owns shared internal `Storage`. Each interned string's
deleter retains that exact storage, removes its entry there and then releases
the capture. It never calls a destroyed facade/global. Map entries remain weak;
ordinary string, morph-map, facade and storage destructors all run. Resetting the
capture ensures external weak observers do not retain an otherwise unused table.
The storage is reclaimed normally after its last actual owner, rather than leaked
for process lifetime.

`g_stringTable` remains an object with matching extern declarations. StringTable
copy/move remain disabled. Public SKEE interface exchange, BodyMorph-v5 virtual
methods, Papyrus signatures and serialized records are unchanged. Private facade
layout changes require rebuilding all plugin translation units; these sources are
not a binary memory patch. Late pre-Revert releases cannot remove a live new entry
with equal text; expired entries are safely replaced if their old callback is
waiting for the mutex.

## Offline regression

```powershell
python tools/test-string-table-lifetime.py --output "<fresh external project output>"
```

The driver requires Windows MSVC with AddressSanitizer and accepts an explicit
`--vcvars` path. It stages byte-identical production `StringTable.cpp/.h` for
the pinned baseline and this checkout. Our narrow test headers stub only game
fixed strings, logging and in-memory serialization, not table/lifetime behavior.
No game, plugin loader, installed mod, microphone, registry or global SDK setup
is touched. Compiler/source/log pins are retained in the external report.

The real normal CRT teardown destroys the facade before a populated six-actor
nested morph/key map. A sentinel poisons only the already-destructed facade for
ASan: the old production deleter must fail with use-after-poison through
`DeleteStringEntry`/`RemoveString`; repaired code must return normally. Additional
modes cover live interning/release, serialization roundtrip, Revert/reintern,
and parallel interning. Allocation tracking brackets actual table construction
and verifies its storage/bucket allocations are freed before a deliberately
retained external weak string observer is destroyed. This is targeted storage
reclamation evidence, not a claim that all plugin memory is leak-free.

An offline harness pass does not prove the full DLL builds or a game exits cleanly.
The full plugin requires the exact gitlink CommonLib revision (currently
`7a60f4de794095d7b0f8928d1b930a52e9a7da83`), manifest dependencies/baseline and a
pinned Windows compiler/SDK. A later separately authorized full-profile run must
verify unchanged morph behavior and actual normal exit0. Do not install or run a
candidate based on this source test alone, relabel an abnormal exit as success,
or suppress destructors to obtain a passing exit code.
