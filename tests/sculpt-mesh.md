# Sculpt buffer regression

This opt-in Windows/MSVC target links the production CDXMesh and CDXEditableMesh
implementations, uses a D3D11 software WARP device, and has a 30-second CTest
timeout. Build with the separately qualified Build Broker profile selecting
`SKEE_BUILD_SCULPT_TESTS=ON`, target `sculpt-mesh`; do not bypass admission with a
local build. Execution is separate from compilation and release qualification.

Coverage in the test source includes null device/context, either GPU buffer
allocation failure, non-standard fill exception, invalid counts/indices, empty
GPU success, failed replacement preserving the old mesh, null storage unwind,
strip expansion winding and count checks, read-only picking, nested edits,
actual D3D Map failure and dirty retry, malformed mapped-data success, derived
normal edits, wireframe double-pass upload batching, barrier-ordered concurrent
writer/flush and GPU byte readback.

Shader methods and material construction are explicit test stubs: this test
does not establish shader/texture correctness or image quality. NIF factories
and actual engine geometry, allocator, scene/callback and renderer contracts are
not executed here. CDXNifMesh inherits the tested access/flush authority rather
than overriding it, but a base/derived C++ assay is not an engine failure test.
No timing or performance claim follows from repeating 1,000 picks.

Current recovery status: source prepared; exact-head native compilation and
test execution unavailable until broker qualification. This is not a test pass.
