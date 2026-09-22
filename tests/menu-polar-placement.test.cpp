#include "../skee64/MenuPolarPlacementPolicy.h"
#include <cassert>
#include <cstdio>
using V=std::array<float,3>;
float Dot(const V& a,const V& b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
bool Near(float a,float b) { return std::abs(a-b)<0.00001F; }
int main()
{
    auto f=SKEE::VR::MakePolarFrame(0,1,30,0);
    assert(Near(f.radial[0],-0.5F)); // left of +Y is -X, not +X
    assert(Near(f.radial[1],std::sqrt(0.75F)));
    for (float heading : {0.F,30.F,90.F,160.F}) for(float azimuth:{-85.F,0.F,38.F,85.F}) for(float elevation:{-60.F,0.F,25.F,60.F}) {
        const float h=heading*0.01745329251994329577F;
        f=SKEE::VR::MakePolarFrame(std::cos(h),std::sin(h),azimuth,elevation);
        assert(Near(Dot(f.radial,f.radial),1)); assert(Near(Dot(f.right,f.right),1)); assert(Near(Dot(f.up,f.up),1));
        assert(Near(Dot(f.radial,f.right),0)); assert(Near(Dot(f.radial,f.up),0)); assert(Near(Dot(f.up,f.right),0));
        // Menu U/V axes are orthogonal to the viewer-to-centre radial vector.
        // Radius changes magnitude, not angles; uniform scaling preserves aspect.
        assert(Near(Dot(f.radial,f.right)*100,0));
    }
    for(float elevation:{-60.F,0.F,60.F}) for(float distance:{30.F,100.F,300.F}) for(float height:{-150.F,-10.F,0.F,10.F,150.F}) {
        const auto original=SKEE::VR::MakePolarFrame(0,1,30,elevation);
        const auto p=SKEE::VR::MakeRaisedPolarPlacement(0,1,30,elevation,distance,height,false);
        assert(Near(p.offset[0],original.radial[0]*distance));
        assert(Near(p.offset[1],original.radial[1]*distance));
        assert(Near(p.offset[2],original.radial[2]*distance+height));
        auto direction=p.offset;
        const auto length=std::sqrt(Dot(direction,direction));
        for(auto& c:direction) c/=length;
        assert(Near(Dot(direction,p.frame.radial),1));
        assert(Near(Dot(direction,p.frame.right),0));
        assert(Near(Dot(direction,p.frame.up),0));
        if(height==0) { assert(p.frame.radial==original.radial); assert(p.frame.up==original.up); }
    }
    for(float heading:{0.F,30.F,90.F,160.F}) for(float azimuth:{-85.F,0.F,38.F,85.F})
    for(float elevation:{-60.F,0.F,60.F}) for(float distance:{30.F,100.F,300.F})
    for(float height:{-150.F,-10.F,0.F,10.F,150.F}) {
        const float h=heading*0.01745329251994329577F;
        const float x=std::cos(h),y=std::sin(h);
        const auto tilted=SKEE::VR::MakeRaisedPolarPlacement(x,y,azimuth,elevation,distance,height,false);
        const auto defaults=SKEE::VR::MakeRaisedPolarPlacement(x,y,azimuth,elevation,distance,height);
        const auto upright=SKEE::VR::MakeRaisedPolarPlacement(x,y,azimuth,elevation,distance,height,true);
        assert(defaults.offset==upright.offset);
        assert(defaults.frame.radial==upright.frame.radial);
        assert(defaults.frame.right==upright.frame.right);
        assert(defaults.frame.up==upright.frame.up);
        assert(upright.offset==tilted.offset); // orientation must not move the menu
        assert((upright.frame.up==V{0,0,1}));
        assert(Near(upright.frame.radial[2],0));
        assert(Near(upright.frame.right[2],0));
        assert(Near(Dot(upright.frame.radial,upright.frame.radial),1));
        assert(Near(Dot(upright.frame.right,upright.frame.right),1));
        assert(Near(Dot(upright.frame.radial,upright.frame.right),0));
        auto horizontal=upright.offset; horizontal[2]=0;
        const auto length=std::sqrt(Dot(horizontal,horizontal));
        for(auto& c:horizontal) c/=length;
        assert(Near(Dot(horizontal,upright.frame.radial),1)); // still faces viewer in yaw
        assert(Near(Dot(horizontal,upright.frame.right),0));
        assert(upright.frame.right==tilted.frame.right);
    }
    std::puts("Polar placement: upright default, opt-in tilt, unchanged offsets and horizontal facing passed.");
}
