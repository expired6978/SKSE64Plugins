#pragma once
#include <array>
#include <cmath>

namespace SKEE::VR
{
    struct PolarFrame { std::array<float,3> radial, right, up; };
    // Coordinate-system independent horizontal heading. Positive azimuth is
    // left (world-up cross forward), not NiMatrix3's clockwise MakeZRotation.
    inline PolarFrame MakePolarFrame(float x, float y, float azimuth, float elevation)
    {
        constexpr float radians=0.01745329251994329577F;
        const float a=azimuth*radians,e=elevation*radians;
        const float fx=x*std::cos(a)-y*std::sin(a),fy=x*std::sin(a)+y*std::cos(a);
        return {{fx*std::cos(e),fy*std::cos(e),std::sin(e)},
                {fy,-fx,0}, {-fx*std::sin(e),-fy*std::sin(e),std::cos(e)}};
    }
    struct PolarPlacement { PolarFrame frame; std::array<float,3> offset; };
    inline PolarPlacement MakeRaisedPolarPlacement(float x, float y, float azimuth,
                                                   float elevation, float distance, float height,
                                                   bool forceVertical = true)
    {
        const auto original = MakePolarFrame(x, y, azimuth, elevation);
        const std::array<float,3> offset{original.radial[0]*distance,
                                       original.radial[1]*distance,
                                       original.radial[2]*distance+height};
        // Placement still uses elevation/height; only surface pitch is locked.
        // Keep yaw toward the viewer and world-up vertical, without roll.
        if (forceVertical) return {MakePolarFrame(x,y,azimuth,0),offset};
        // A vertical offset must also tilt the surface toward its fixed viewer.
        // Preserve the exact established transform when the new setting is zero.
        if (height == 0) return {original, offset};
        constexpr float degrees=57.295779513082320876F;
        const auto raisedElevation=std::atan2(offset[2], std::hypot(offset[0],offset[1]))*degrees;
        return {MakePolarFrame(x,y,azimuth,raisedElevation),offset};
    }
}
