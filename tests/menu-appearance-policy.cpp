#include "../skee64/MenuAppearancePolicy.h"
#include <array>
#include <cassert>
int main()
{
	using namespace SKEE::MenuAppearance;
	assert(ParseColor("") == -1);
	assert(ParseColor("#000000") == 0);
	assert(ParseColor(" Ff00aB ") == 0xFF00AB);
	assert(ParseColor("#ffffff") == 0xFFFFFF);
	for (auto invalid : {"red", "0x112233", "12345", "1234567", "#GG0000"}) assert(ParseColor(invalid) == -1);
	std::array<std::uint8_t,33> png{137,80,78,71,13,10,26,10,0,0,0,13,'I','H','D','R',0,0,0,1,0,0,0,1};
	assert(BoundedPNG(png.data(),png.size()));
	assert(!BoundedPNG(png.data(),32));
	assert(!BoundedPNG(png.data(),16*1024*1024+1));
	png[18]=16;png[19]=0;assert(BoundedPNG(png.data(),png.size())); // 4096
	png[19]=1;assert(!BoundedPNG(png.data(),png.size()));
	png[18]=0;png[19]=0;assert(!BoundedPNG(png.data(),png.size())); // zero
}
