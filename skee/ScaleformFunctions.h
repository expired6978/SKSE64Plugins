#pragma once

#include "skse64/ScaleformCallbacks.h"
#include "skse64/GameThreads.h"
#include "skse64/GameTypes.h"

class GFxValue;
class GFxMovieView;

void RegisterNumber(GFxValue * dst, const char * name, double value);
void RegisterString(GFxValue * dst, GFxMovieView * view, const char * name, const char * str);
void RegisterBool(GFxValue * dst, const char * name, bool value);

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