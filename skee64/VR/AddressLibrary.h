#pragma once

#include <REL/Relocation.h>

// VR-only IDs verified against the published 0.294.0 release CSV.
// See tests/VR/address-library-baseline.json for its exact identity. These are not AE IDs.
// Runtime branches using them must remain VR-only; flat wrappers keep their
// existing AE IDs. An ID is not a replacement for ABI/instruction validation.
namespace SKEE::VR::Address
{
    inline constexpr REL::RelocationID SetUniqueID{ 0, 0, 15907 };
    inline constexpr REL::RelocationID TransferUID{ 0, 0, 15909 };
    inline constexpr REL::RelocationID UpdateNPCMorphs{ 0, 0, 24209 };
    inline constexpr REL::RelocationID UpdateNPCMorph{ 0, 0, 24210 };
    inline constexpr REL::RelocationID UpdateHeadState{ 0, 0, 24220 };
    inline constexpr REL::RelocationID DisableHead{ 0, 0, 24221 };
    inline constexpr REL::RelocationID EnableHead{ 0, 0, 24228 };
    inline constexpr REL::RelocationID ApplyMorph{ 0, 0, 26252 };
    inline constexpr REL::RelocationID ApplyMorphByPart{ 0, 0, 26254 };
    inline constexpr REL::RelocationID RegenerateHead{ 0, 0, 26259 };
    inline constexpr REL::RelocationID ApplyRaceMorph{ 0, 0, 26300 };
    inline constexpr REL::RelocationID UpdateModelFace{ 0, 0, 26458 };
    inline constexpr REL::RelocationID UpdateOneMorphCaller{ 0, 0, 26466 };
    inline constexpr REL::RelocationID ChangeHeadPart{ 0, 0, 26468 };
    inline constexpr REL::RelocationID UpdateModelSkin{ 0, 0, 26472 };
    inline constexpr REL::RelocationID UpdateModelHair{ 0, 0, 26473 };
    inline constexpr REL::RelocationID RenderMenu{ 0, 0, 32867 };
    inline constexpr REL::RelocationID SetNiGeometryTexture{ 0, 0, 36012 };
    inline constexpr REL::RelocationID InitializeDisplayObject{ 0, 0, 50896 };
    inline constexpr REL::RelocationID DoubleMorph{ 0, 0, 51526 };
    inline constexpr REL::RelocationID LoadSliders{ 0, 0, 51534 };
    inline constexpr REL::RelocationID AddSlider{ 0, 0, 51580 };
    inline constexpr REL::RelocationID NiStreamCtor{ 0, 0, 68971 };
    inline constexpr REL::RelocationID NiStreamDtor{ 0, 0, 68972 };
    inline constexpr REL::RelocationID NiStreamAddObject{ 0, 0, 68973 };
    inline constexpr REL::RelocationID CreateBSTriShape{ 0, 0, 69284 };
    inline constexpr REL::RelocationID CreateSourceTexture{ 0, 0, 69335 };
    inline constexpr REL::RelocationID CreateBSDynamicTriShape{ 0, 0, 69562 };
    inline constexpr REL::RelocationID NiTriBasedGeomCtor{ 0, 0, 69838 };
    inline constexpr REL::RelocationID BSFadeNodeCtor{ 0, 0, 98844 };
    inline constexpr REL::RelocationID InitializeShader{ 0, 0, 99862 };
    inline constexpr REL::RelocationID CopyFrom{ 0, 0, 100006 };
    inline constexpr REL::RelocationID GetHeadParts{ 0, 0, 13541 };
    inline constexpr REL::RelocationID CreateArmorNode{ 0, 0, 15534 };
    inline constexpr REL::RelocationID PreprocessHead{ 0, 0, 24227 };
    inline constexpr REL::RelocationID UpdateAllMorphsCaller{ 0, 0, 26256 };
    inline constexpr REL::RelocationID RaceMorphCaller{ 0, 0, 26299 };
    inline constexpr REL::RelocationID SetInventoryModel{ 0, 0, 50899 };
    inline constexpr REL::RelocationID NewInventoryModelCaller{ 0, 0, 50900 };
    inline constexpr REL::RelocationID DoubleMorphSecondaryCaller{ 0, 0, 51494 };
    inline constexpr REL::RelocationID SliderCategoryCaller{ 0, 0, 51532 };
    inline constexpr REL::RelocationID FaceGenPartCache{ 0, 0, 51552 };
    inline constexpr REL::RelocationID AllocateGeometry{ 0, 0, 67673 };
    inline constexpr REL::RelocationID FreeGeometry{ 0, 0, 67677 };
    inline constexpr REL::RelocationID DynamicGeometryCopy{ 0, 0, 69556 };
    inline constexpr REL::RelocationID DynamicGeometryLoad{ 0, 0, 69557 };
    inline constexpr REL::RelocationID DynamicGeometryCtor{ 0, 0, 69563 };
    inline constexpr REL::RelocationID DynamicGeometryDtor{ 0, 0, 69565 };
    inline constexpr REL::RelocationID DynamicGeometryResize{ 0, 0, 69569 };
    inline constexpr REL::RelocationID DynamicGeometryDelete{ 0, 0, 69573 };
    inline constexpr REL::RelocationID FxDelegateInvoke{ 0, 0, 80520 };
    inline constexpr REL::RelocationID MenuCursor{ 0, 0, 517043 };
}
