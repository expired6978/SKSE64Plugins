cmake_minimum_required(VERSION 3.21)

project(skee64 VERSION 1.0.0 LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "skee64 is a Windows/MSVC SKSE plugin")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# MSVC CRT linkage is chosen per build preset (see CMakePresets.json):
#   *-msvc-vcpkg-flatrim : x64-windows-static-md triplet, dynamic CRT (/MD /MDd)
#   *-msvc-vcpkg-static  : x64-windows-static    triplet, static CRT   (/MT /MTd)
# Honor a preset-provided value (passed via -DCMAKE_MSVC_RUNTIME_LIBRARY) so the
# static presets actually take effect; fall back to the dynamic default only when
# configured without one. Do NOT hardcode the old unconditional /MT here.
if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Emit a PDB for every configuration (Release included) so crash dumps can be
# symbolized. MSVC Debug already does this by default; /Zi + /DEBUG extend it
# to Release.
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase")
add_link_options(/DEBUG)

# Whole-program optimization via MSVC link-time code generation (/GL at compile,
# /LTCG at link), Release only so Debug stays fast to build. Applies to every
# target in the tree (skee64 + CommonLibSSE-NG). Prebuilt vcpkg static libs were
# not compiled with /GL, so they aren't LTCG-folded across their boundary, but
# they still link fine alongside the /GL objects.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# Emit a linker map file (<target>.map) for Release builds so code/data size can be
# analyzed per symbol and section.
add_link_options($<$<CONFIG:Release>:/MAP>)

# Build the vendored CommonLib source, not its release/prebuilt bundle.
set(COMMONLIB_PREBUILT OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

set(ENABLE_SKYRIM_SE OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(SKSE_SUPPORT_XBYAK ON CACHE BOOL "" FORCE)
set(SKSE_SUPPORT_PATCH_SAFETY ON CACHE BOOL "" FORCE)

add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

find_package(xbyak CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)
find_package(DirectXTex CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(directxmath CONFIG REQUIRED)

add_subdirectory(CommonLibSSE-NG EXCLUDE_FROM_ALL)
# CommonLib's source-build path does not include this consumer helper itself.
include("${CMAKE_CURRENT_SOURCE_DIR}/CommonLibSSE-NG/cmake/CommonLibSSE.cmake")
# SKSE_SUPPORT_XBYAK is public API in CommonLib's trampoline header and SKEEHooks uses it too.
target_link_libraries(CommonLibSSE PUBLIC xbyak::xbyak)

# nlohmann-json and Microsoft::DirectXTK are resolved by the CommonLibSSE-NG
# dependency graph. Project code is migrated to them instead of compiling jsoncpp/DirectXTex.
set(SKEE_SOURCES
    skee64/ActorArmorTangentUpdater.cpp
    skee64/ActorUpdateManager.cpp
    skee64/AttachmentInterface.cpp
    skee64/BodyMorphInterface.cpp
    skee64/CommandInterface.cpp
    skee64/FaceMorphInterface.cpp
    skee64/FileUtils.cpp
    skee64/FormTagInterface.cpp
    skee64/ItemDataInterface.cpp
    skee64/main.cpp
    skee64/Morpher.cpp
    skee64/NifUtils.cpp
    skee64/NiTransformInterface.cpp
    skee64/OverlayInterface.cpp
    skee64/OverrideInterface.cpp
    skee64/OverrideVariant.cpp
    skee64/PapyrusCharGen.cpp
    skee64/PapyrusNiOverride.cpp
    skee64/PartHandler.cpp
    skee64/PluginInterface.cpp
    skee64/PresetInterface.cpp
    skee64/ScaleformCharGenFunctions.cpp
    skee64/ScaleformFunctions.cpp
    skee64/ScaleformUtils.cpp
    skee64/ShaderUtilities.cpp
    skee64/SKEEHooks.cpp
    skee64/SkeletonExtender.cpp
    skee64/SkinLayerInterface.cpp
    skee64/StringTable.cpp
    skee64/tinyxml2.cpp
    skee64/TintMaskInterface.cpp
    skee64/Utilities.cpp
    skee64/CDXBSShaderResource.cpp
    skee64/CDXBrush.cpp
    skee64/CDXBrushMesh.cpp
    skee64/CDXCamera.cpp
    skee64/CDXEditableMesh.cpp
    skee64/CDXEditableScene.cpp
    skee64/CDXMaterial.cpp
    skee64/CDXMesh.cpp
	skee64/CDXNifPixelShaderCache.cpp
    skee64/CDXNifCommands.cpp
    skee64/CDXNifMesh.cpp
    skee64/CDXNifMaterial.cpp
    skee64/CDXNifScene.cpp
    skee64/CDXNifTextureRenderer.cpp
    skee64/CDXNifBrush.cpp
    skee64/CDXPicker.cpp
    skee64/CDXPixelShaderCache.cpp
    skee64/CDXRenderState.cpp
    skee64/CDXResetMask.cpp
    skee64/CDXScene.cpp
    skee64/CDXShader.cpp
    skee64/CDXShaderCompile.cpp
    skee64/CDXShaderFactory.cpp
    skee64/CDXStroke.cpp
    skee64/CDXTextureRenderer.cpp
    skee64/CDXUndo.cpp
    skee64/Resource.rc
)

add_commonlibsse_plugin(skee64
    NAME "skee"
    AUTHOR "Expired6978"
    EMAIL "expired6978@gmail.com"
    VERSION 0.5.0.0
    USE_ADDRESS_LIBRARY
    SOURCES ${SKEE_SOURCES}
)
target_include_directories(skee64 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/skee64")
target_compile_definitions(skee64 PRIVATE
    XBYAK_NO_OP_NAMES
)
target_compile_options(skee64 PRIVATE /permissive- /bigobj)
# pch.h keeps the required include order (RE/Skyrim.h before SKSE/SKSE.h so
# that REX::W32::MAX_PATH users inside CommonLib compile before any Windows
# API header appears mid-aggregate) and drops the MAX_PATH macro that
# DirectXTK's SimpleMath.h leaves defined via its transitive <windows.h>.
target_precompile_headers(skee64 PRIVATE skee64/pch.h)

# Imported targets from this build's own vcpkg install (per-config Debug/Release libs).
target_link_libraries(skee64 PRIVATE
    xbyak::xbyak
    Microsoft::DirectXTK
    Microsoft::DirectXTex
    nlohmann_json::nlohmann_json
    JsonCpp::JsonCpp
    shlwapi.lib
)
set_target_properties(skee64 PROPERTIES OUTPUT_NAME "skee64")
target_link_options(skee64 PRIVATE /FORCE:UNRESOLVED)

# --- Precompiled shaders ---------------------------------------------------------
# Compile the HLSL/.fx sources with fxc (Windows SDK) into .cso. The runtime
# loads them from SKSE/Plugins/<Module>/Shaders/Compiled/ and falls back to
# compiling the source at load time; profiles match the D3DCompile targets in
# CDXShaderFactory.cpp (vs_5_0 / ps_5_0 / gs_5_0).
find_program(SKEE_FXC_EXE NAMES fxc.exe DOC "fxc.exe HLSL compiler from the Windows SDK")
if(NOT SKEE_FXC_EXE)
    message(FATAL_ERROR "fxc.exe not found on PATH — configure from a Visual Studio build environment (or via ./build.sh).")
endif()

set(SKEE_SHADER_SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/skee64/Shaders")
set(SKEE_SHADER_OUT_ROOT "${CMAKE_BINARY_DIR}/Shaders")

# fxc exits 0 even on some write failures, so each shader compiles to a temp
# name and is renamed into place — the rename is what verifies the output.
function(skee_add_shader src profile entry out)
    get_filename_component(out_dir "${out}" DIRECTORY)
    get_filename_component(out_name "${out}" NAME)
    add_custom_command(
        OUTPUT "${out}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${out_dir}"
        COMMAND "${SKEE_FXC_EXE}" /nologo /T "${profile}" /E "${entry}" /Fo${out}.tmp "${src}"
        COMMAND ${CMAKE_COMMAND} -E rename "${out}.tmp" "${out}"
        DEPENDS "${src}"
        COMMENT "fxc ${out_name} (${profile})"
        VERBATIM
    )
    list(APPEND SKEE_SHADER_OUTPUTS "${out}")
    set(SKEE_SHADER_OUTPUTS "${SKEE_SHADER_OUTPUTS}" PARENT_SCOPE)
endfunction()

set(SKEE_SHADER_OUTPUTS)

# Each entry: <source file>|<profile>|<entry point> ('|' because ';' would
# flatten the list — CMake lists are semicolon-separated).
set(SKEE_SHADERS
    "CharGen/brush_vs.hlsl|vs_5_0|BrushVShader"
    "CharGen/brush_ps.hlsl|ps_5_0|BrushPShader"
    "CharGen/shader_vs.hlsl|vs_5_0|LightVertexShader"
    "CharGen/shader_ps.hlsl|ps_5_0|LightPixelShader"
    "CharGen/wireframe_vs.hlsl|vs_5_0|WireframeVertexShader"
    "CharGen/wireframe_ps.hlsl|ps_5_0|WireframePixelShader"
    "CharGen/wireframe_gs.hlsl|gs_5_0|WireframeGeometryShader"
    "NiOverride/texture.fx|vs_5_0|TextureVertex"
)
foreach(spec IN LISTS SKEE_SHADERS)
    string(REPLACE "|" ";" spec_list "${spec}")
    list(GET spec_list 0 rel_src)
    list(GET spec_list 1 profile)
    list(GET spec_list 2 entry)
    set(src_path "${SKEE_SHADER_SRC_ROOT}/${rel_src}")
    if(NOT EXISTS "${src_path}")
        message(FATAL_ERROR "Shader source not found: ${src_path}")
    endif()
    get_filename_component(base "${rel_src}" NAME_WE)
    get_filename_component(subdir "${rel_src}" DIRECTORY)
    skee_add_shader("${src_path}" "${profile}" "${entry}"
        "${SKEE_SHADER_OUT_ROOT}/${subdir}/Compiled/${base}.cso")
endforeach()

# Every NiOverride effect is a plain-HLSL pixel shader whose entry point is the
# lowercased file name; the runtime looks up Compiled/Effects/<name>.cso.
file(GLOB SKEE_EFFECT_SOURCES CONFIGURE_DEPENDS "${SKEE_SHADER_SRC_ROOT}/NiOverride/Effects/*.fx")
foreach(fx IN LISTS SKEE_EFFECT_SOURCES)
    get_filename_component(base "${fx}" NAME_WE)
    string(TOLOWER "${base}" lc_base)
    skee_add_shader("${fx}" "ps_5_0" "${lc_base}"
        "${SKEE_SHADER_OUT_ROOT}/NiOverride/Compiled/Effects/${lc_base}.cso")
endforeach()

add_custom_target(skee_shaders ALL DEPENDS ${SKEE_SHADER_OUTPUTS})
add_dependencies(skee64 skee_shaders)

# Install destination follows Skyrim64Path (the game root), matching the
# --install logic in build.ps1/build.sh. When unset, falls back to the
# platform default prefix (C:/Program Files (x86)/skee64 on Windows).
if(DEFINED ENV{Skyrim64Path})
    if(NOT IS_DIRECTORY "$ENV{Skyrim64Path}")
        message(WARNING "Skyrim64Path is set but not an existing directory: '$ENV{Skyrim64Path}' - the install step will fail.")
    endif()
    set(CMAKE_INSTALL_PREFIX "$ENV{Skyrim64Path}" CACHE PATH "Install prefix" FORCE)
endif()

install(TARGETS skee64 RUNTIME DESTINATION "Data/SKSE/Plugins")

# Install each shader module's sources and compiled .cso into the game plugin
# dir, mirroring the --install layout in build.ps1/build.sh:
#   <prefix>/Data/SKSE/Plugins/<Module>/Shaders/**            (sources)
#   <prefix>/Data/SKSE/Plugins/<Module>/Shaders/Compiled/**   (built .cso)
file(GLOB SKEE_SHADER_MODULE_DIRS "${SKEE_SHADER_SRC_ROOT}/*")
foreach(mod_dir IN LISTS SKEE_SHADER_MODULE_DIRS)
    if(IS_DIRECTORY "${mod_dir}")
        get_filename_component(mod_name "${mod_dir}" NAME)
        # Ensure the compiled output dir exists at configure time so the
        # install rule is valid before the first build (the .cso files appear
        # once skee_shaders has run).
        file(MAKE_DIRECTORY "${SKEE_SHADER_OUT_ROOT}/${mod_name}/Compiled")
        install(DIRECTORY "${mod_dir}/"
                DESTINATION "Data/SKSE/Plugins/${mod_name}/Shaders")
        install(DIRECTORY "${SKEE_SHADER_OUT_ROOT}/${mod_name}/Compiled/"
                DESTINATION "Data/SKSE/Plugins/${mod_name}/Shaders/Compiled")
    endif()
endforeach()
