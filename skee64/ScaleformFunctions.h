#pragma once

#include "skse64/ScaleformCallbacks.h"
#include "skse64/GameThreads.h"
#include "skse64/GameTypes.h"

class GFxValue;
class GFxMovieView;

class SKSEScaleform_GetDyeableItems : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_GetDyeItems : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_SetItemDyeColor : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_SetItemDyeColors : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};