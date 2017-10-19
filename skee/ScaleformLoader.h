#pragma once

#include "skse64/ScaleformState.h"

class NiTexture;
class IMenu;
class GImageInfoBase;

extern bool g_logScaleform;

class GFxImageLoader : public GFxState
{
public:
	virtual GImageInfoBase*	LoadImage(const char * url) = 0;
};

class BSScaleformImageLoader : public GFxImageLoader
{
public:
	virtual ~BSScaleformImageLoader();
	virtual GImageInfoBase*	LoadImage(const char * url);

	MEMBER_FN_PREFIX(BSScaleformImageLoader);
	DEFINE_MEMBER_FN(AddVirtualImage, UInt8, 0, NiTexture ** texture);
	DEFINE_MEMBER_FN(ReleaseVirtualImage, UInt8, 0, NiTexture ** texture);
};

class GFxLoader
{
public:
	UInt32			unk00;					// 00
	GFxStateBag		* stateBag;				// 04
	UInt32			unk08;					// 08
	UInt32			unk0C;					// 0C
	BSScaleformImageLoader	* imageLoader;	// 10

	static GFxLoader * GetSingleton();	



	MEMBER_FN_PREFIX(GFxLoader);
	DEFINE_MEMBER_FN(ctor, GFxLoader *, 0x00EA7830);

	// Note: Probably in subclass
	DEFINE_MEMBER_FN(LoadMovie, bool, 0x00EA7EF0, IMenu* menu, GFxMovieView** viewOut, const char* name, int arg4, float arg5);
};
