#pragma once
namespace RE { class GFxMovie; class GFxValue; class NiPoint3; class NiMatrix3; }
namespace SKEE::FaceView
{
    void Configure(bool enabled, float distance, float eyeHeight = 5);
    bool Supported();
    unsigned Current();
    bool Request(unsigned view);
    void Restore();
    void Register(RE::GFxMovie* movie, RE::GFxValue* root);
    // CameraEditor coordinates match GetPlayerPosition: avatar-parent space.
    // Movement is queued on the game thread, never applied to the tracked HMD.
    bool GetCameraTransform(RE::NiPoint3& position, RE::NiMatrix3& rotation);
    bool RequestCameraPosition(const RE::NiPoint3& position);
}
