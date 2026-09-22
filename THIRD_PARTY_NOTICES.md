# Third-Party Notices

skee64 is distributed under the GNU General Public License version 3 or later
(see `LICENSE`). This file records the third-party components used by skee64
and their licenses, as required for the corresponding-source and notice
obligations of the GPL-3.0-or-later static link against CommonLibSSE-NG.

## Vendored in this repository

| Component | Location | License | Notes |
| --- | --- | --- | --- |
| CommonLibSSE-NG (pinned revision `25440f9a4`, project version 8.2.0) | `CommonLibSSE-NG/` | GPL-3.0-or-later with listed exceptions (see `CommonLibSSE-NG/COPYING.txt` and `CommonLibSSE-NG/EXCEPTIONS.md`) | Built from source as a subdirectory; statically linked. Its README states that plugins which statically link it must themselves be GPL-3.0-or-later or GPL-compatible — skee64 is released under GPL-3.0-or-later accordingly. |
| OpenVR | `CommonLibSSE-NG/extern/openvr/` (recursive submodule) | BSD-3-Clause (see its `LICENSE`) | Pinned by CommonLib; provides the Windows import library and API headers. |
| tinyxml2 | `skee64/tinyxml2.{h,cpp}` | Zlib License | Redistributed source for preset/XML parsing; see `skee64/tinyxml2.h` header notice. |

## Supplied by the vcpkg manifest (`vcpkg.json`, pinned baseline)

| Component | Minimum version | License |
| --- | --- | --- |
| DirectXTK (Microsoft) | 2025-10-27 | See `vcpkg_installed/x64-windows-static-md/share/directxtk/copyright` |
| DirectXMath (Microsoft) | 2025-04-03 | See `vcpkg_installed/x64-windows-static-md/share/directxmath/copyright` |
| DirectXTex (Microsoft) | Pinned vcpkg baseline | See `vcpkg_installed/x64-windows-static-md/share/directxtex/copyright` |
| jsoncpp | Pinned vcpkg baseline | See `vcpkg_installed/x64-windows-static-md/share/jsoncpp/copyright` |
| fmt | 12.1.0 | See `vcpkg_installed/x64-windows-static-md/share/fmt/copyright` |
| nlohmann-json | 3.12.0 | MIT License |
| rapidcsv | 8.90 | See `vcpkg_installed/x64-windows-static-md/share/rapidcsv/copyright` |
| simpleini | 4.25 | See `vcpkg_installed/x64-windows-static-md/share/simpleini/copyright` |
| spdlog | 1.16.0 | MIT License |
| toml11 | 4.4.0 | See `vcpkg_installed/x64-windows-static-md/share/toml11/copyright` |
| xbyak | 7.28 | See `vcpkg_installed/x64-windows-static-md/share/xbyak/copyright` |
| zlib | 1.3.1 (pinned baseline) | See `vcpkg_installed/x64-windows-static-md/share/zlib/copyright` |

## Configure-time fetched (via CommonLibSSE-NG)

| Component | Source | License | Notes |
| --- | --- | --- | --- |
| hde64 (instruction-length decoder, from MinHook) | `https://github.com/TsudaKageyu/minhook.git` tag `v1.3.4`, `src/hde/` | See the fetched MinHook source notice | Vendored by CommonLibSSE-NG's configure step when `SKSE_SUPPORT_PATCH_SAFETY=ON`; compiled into the CommonLibSSE static library only (PRIVATE). |

## Retired local third-party trees

The following local source trees were part of the legacy Visual Studio build
and are retired from the supported build by this migration; their functionality
is provided by the components above:

- `DirectXTex/` → the old source tree is replaced by the vcpkg DirectXTex dependency, still linked alongside DirectXTK.
- `jsoncpp/` → the old source tree is replaced by the vcpkg jsoncpp dependency, still linked alongside nlohmann-json.
- `spdlog/` → replaced by CommonLibSSE-NG's `SKSE::log` (spdlog is still a transitive dependency of CommonLibSSE-NG itself).

## Runtime prerequisites (end user)

Deployed builds require, in addition to the game:

- A matching SKSE for the selected Skyrim AE or VR runtime that supports the Address
  Library metadata declared by this plugin.
- The matching **Address Library for SKSE Plugins** or **Skyrim VR Address
  Library** installed and up to date. RaceMenu VR 2 enables only relocations
  whose runtime mapping and instruction window are independently qualified.
