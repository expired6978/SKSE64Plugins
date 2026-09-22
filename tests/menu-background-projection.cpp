#include "../skee64/MenuBackgroundProjection.h"
#include <cassert>
#include <limits>
#include <algorithm>
int main()
{
	using namespace SKEE::MenuAppearance;
	std::array<SurfaceVertex,4> quad{{{0,0,0,0,0},{160,0,0,1,0},{160,90,0,1,1},{0,90,0,0,1}}};
	assert(std::abs(SurfaceAspect(quad)-16.0/9)<1e-9);
	std::reverse(quad.begin(),quad.end());
	assert(std::abs(SurfaceAspect(quad)-16.0/9)<1e-9);
	for (auto& p : quad) { const auto x=p.x; p.x=p.y*3+50; p.y=-x*3-100; p.z=20; p.u=1-p.u; }
	assert(std::abs(SurfaceAspect(quad)-16.0/9)<1e-9); // rotation, uniform scale, flipped U
	for (auto& p : quad) p.v*=0.5;
	assert(std::abs(SurfaceAspect(quad)-8.0/9)<1e-9); // cropped texture coordinates
	auto bad=quad; bad[0].x+=5; assert(SurfaceAspect(bad)==0);
	bad=quad; for(auto& p:bad) p.y+=p.x; assert(SurfaceAspect(bad)==0); // shear
	bad=quad; bad[0].u=std::numeric_limits<double>::quiet_NaN(); assert(SurfaceAspect(bad)==0);
	bad=quad; for(auto& p:bad) p.u=0; assert(SurfaceAspect(bad)==0);
	bad=quad; for(auto& p:bad) p.x=p.y=p.z=0; assert(SurfaceAspect(bad)==0);
}
