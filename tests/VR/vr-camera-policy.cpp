// SPDX-License-Identifier: GPL-3.0-or-later
#include "RaceSexCameraPolicy.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
    void Check(bool result, const char* message)
    { if (!result) throw std::runtime_error(message); }
    void Near(const RE::NiPoint3& a, const RE::NiPoint3& b)
    { Check(a.GetSquaredDistance(b) < 0.00001F, "Camera coordinate mismatch"); }
}
int main()
{
    try {
        using namespace SKEE::CameraPolicy;
        unsigned flatReads = 0, vrReads = 0;
        Check(Select(true, [&] { ++vrReads; return true; }, [&]() -> bool {
            ++flatReads; throw std::runtime_error("VR evaluated nonexistent flat camera");
        }), "VR dispatch failed");
        Check(vrReads == 1 && flatReads == 0, "Wrong camera path evaluated");
        Check(Select(false, [] { return false; }, [&] { ++flatReads; return true; }), "Flat dispatch failed");
        Check(flatReads == 1, "Flat dispatch was not preserved");
        Check(!Select(true, [] { return false; }, []() -> bool {
            throw std::runtime_error("Unavailable VR path fell back to flat layout");
        }), "Unavailable VR camera was not rejected");

        RE::NiTransform avatarFrame;
        avatarFrame.translate = {100, -70, 20}; avatarFrame.scale = 2;
        avatarFrame.rotate.entry[0][0] = 0; avatarFrame.rotate.entry[0][1] = -1;
        avatarFrame.rotate.entry[1][0] = 1; avatarFrame.rotate.entry[1][1] = 0;
        Check(Valid(avatarFrame), "Valid rotated/scaled avatar frame rejected");
        const RE::NiPoint3 local{10, 30, 80};
        const auto world = avatarFrame.translate + WorldDelta(avatarFrame, local);
        Near(LocalPoint(avatarFrame, world), local);

        // Different rotated/scaled tracking parent, and a nonzero origin.
        RE::NiTransform trackingFrame;
        trackingFrame.translate = {-90, 150, 40}; trackingFrame.scale = 0.5F;
        trackingFrame.rotate.entry[0][0] = -1; trackingFrame.rotate.entry[1][1] = -1;
        const RE::NiPoint3 input{3, -2, 5}, origin{8, 12, -6};
        const auto delta = WorldDelta(avatarFrame, input);
        const auto movedOrigin = origin + LocalDelta(trackingFrame, delta);
        Near(WorldDelta(trackingFrame, movedOrigin-origin), delta);
        Near(LocalPoint(avatarFrame, world+delta), local+input);
        // Applying the input delta must preserve real tracking movement that
        // occurs between enqueue and execution, rather than restore a stale pose.
        const RE::NiPoint3 headMotion{0.5F, 1.F, -0.25F};
        Near(LocalPoint(avatarFrame, world+headMotion+delta),
            local+input+LocalDelta(avatarFrame, headMotion));
        // Shared Face View offset composition and reversal are lossless.
        const RE::NiPoint3 faceOffset{20, -25, 2};
        const auto total = faceOffset + LocalDelta(trackingFrame, delta);
        Near((origin+total)-total, origin);

        // World yaw is about the live eye, not the room or avatar origin.
        RE::NiTransform roomWorld=avatarFrame;
        const RE::NiPoint3 trackedEye{4,8,6}, trackedHand{6,7,5};
        const auto eye=roomWorld.translate+WorldDelta(roomWorld,trackedEye);
        const auto yawed=YawAroundEye(roomWorld,eye,10);
        Check(Valid(yawed), "Yaw produced invalid transform");
        Near(yawed.translate+WorldDelta(yawed,trackedEye),eye);
        const auto hand=roomWorld.translate+WorldDelta(roomWorld,trackedHand);
        const auto yawedHand=yawed.translate+WorldDelta(yawed,trackedHand);
        const auto difference=yawedHand-eye;
        RE::NiTransform yawOnly;
        yawOnly=YawAroundEye(yawOnly,{},10);
        Near(difference,WorldDelta(yawOnly,hand-eye));
        Near(YawAroundEye(yawed,eye,-10).translate,roomWorld.translate);
        Check(Same(YawAroundEye(yawed,eye,-10),roomWorld), "Yaw inverse changed orientation or scale");
        Check(!Same(yawed,roomWorld), "Rotation ownership missed changed yaw");
        for(const float angle : {-60.F,-30.F,-5.F,5.F,30.F,60.F}) {
            const auto turn=YawAroundEye(roomWorld,eye,angle);
            Near(turn.translate+WorldDelta(turn,trackedEye),eye);
            Check(Same(YawAroundEye(turn,eye,-angle),roomWorld), "Full control range inverse failed");
        }
        const auto yawLocal=LocalPoint(trackingFrame,yawed.translate);
        Near(trackingFrame.translate+WorldDelta(trackingFrame,yawLocal),yawed.translate);
        const auto compensation=yawLocal-LocalPoint(trackingFrame,roomWorld.translate);
        const auto faceBase=LocalPoint(trackingFrame,roomWorld.translate)-faceOffset;
        Near((faceBase+faceOffset+compensation)-faceOffset-compensation,faceBase);
        auto competing=yawed; competing.scale+=0.01F;
        Check(!Same(competing,yawed), "Ownership missed changed scale");
        competing=yawed; competing.translate.x+=1;
        Check(!Same(competing,yawed), "Ownership missed changed translation");

        Check(!SafeDelta({2001, 0, 0}), "Oversized move accepted");
        Check(!SafeDelta({std::numeric_limits<float>::infinity(), 0, 0}), "Infinite move accepted");
        Check(!SafeDelta({0, std::numeric_limits<float>::quiet_NaN(), 0}), "NaN move accepted");
        Check(SafeDelta({0, 0, 0}), "Zero move rejected");
        trackingFrame.scale = 0; Check(!Valid(trackingFrame), "Zero scale accepted");
        trackingFrame.scale = -1; Check(!Valid(trackingFrame), "Negative scale accepted");
        trackingFrame.scale = 1; trackingFrame.rotate.entry[0][0] = 2;
        Check(!Valid(trackingFrame), "Nonrotation matrix accepted");
        trackingFrame.rotate.entry[0][0] = std::numeric_limits<float>::quiet_NaN();
        Check(!Valid(trackingFrame), "NaN matrix accepted");
        std::cout << "PASS: VR/flat camera dispatch, coordinate transforms, tracking preservation, offset reversal and invalid inputs\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
