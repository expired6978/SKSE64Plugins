// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cmath>
#include <utility>
#include "RE/N/NiTransform.h"

namespace SKEE::CameraPolicy
{
    // Lazy dispatch is essential: VR must never evaluate the flat menu's
    // nonexistent embedded camera, even as a proposed fallback.
    template <class VR, class Flat>
    auto Select(bool vr, VR&& tracked, Flat&& embedded)
    {
        if (vr) return std::forward<VR>(tracked)();
        return std::forward<Flat>(embedded)();
    }
    inline bool Finite(const RE::NiPoint3& p)
    { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
    inline bool Valid(const RE::NiTransform& frame)
    {
        if (!Finite(frame.translate) || !std::isfinite(frame.scale) || frame.scale <= 0.0001F) return false;
        // These transforms are rotations plus uniform scale. Reject malformed
        // matrices rather than treating a transpose as an arbitrary inverse.
        for (unsigned i = 0; i < 3; ++i) for (unsigned j = 0; j < 3; ++j) {
            float dot = 0;
            for (unsigned k = 0; k < 3; ++k) dot += frame.rotate.entry[k][i] * frame.rotate.entry[k][j];
            if (!std::isfinite(dot) || std::abs(dot - (i == j ? 1.F : 0.F)) > 0.001F) return false;
        }
        return true;
    }
    inline RE::NiPoint3 LocalPoint(const RE::NiTransform& frame, const RE::NiPoint3& world)
    { return frame.rotate.Transpose() * (world - frame.translate) / frame.scale; }
    inline RE::NiPoint3 LocalDelta(const RE::NiTransform& frame, const RE::NiPoint3& world)
    { return frame.rotate.Transpose() * world / frame.scale; }
    inline RE::NiPoint3 WorldDelta(const RE::NiTransform& frame, const RE::NiPoint3& local)
    { return frame.rotate * (local * frame.scale); }
    inline bool SafeDelta(const RE::NiPoint3& p)
    { return Finite(p) && p.Length() <= 2000.F; }
    inline bool Same(const RE::NiTransform& a, const RE::NiTransform& b)
    {
        if (!Valid(a) || !Valid(b) || a.translate.GetSquaredDistance(b.translate) >= 0.0001F ||
            std::abs(a.scale-b.scale) > 0.0001F) return false;
        for (unsigned i=0;i<3;++i) for (unsigned j=0;j<3;++j)
            if (std::abs(a.rotate.entry[i][j]-b.rotate.entry[i][j]) > 0.0001F) return false;
        return true;
    }
    inline RE::NiTransform YawAroundEye(const RE::NiTransform& origin, const RE::NiPoint3& eye, float degrees)
    {
        const float angle=degrees*0.017453292519943295F, c=std::cos(angle), s=std::sin(angle);
        RE::NiMatrix3 yaw;
        yaw.entry[0][0]=c; yaw.entry[0][1]=-s;
        yaw.entry[1][0]=s; yaw.entry[1][1]=c;
        auto result=origin;
        result.rotate=yaw*origin.rotate;
        result.translate=eye+yaw*(origin.translate-eye);
        return result;
    }
}
