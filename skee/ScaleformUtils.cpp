#include "ScaleformUtils.h"
#include "skse64/ScaleformMovie.h"
#include "skse64/ScaleformValue.h"

void RegisterNumber(GFxValue * dst, const char * name, double value)
{
	GFxValue	fxValue;
	fxValue.SetNumber(value);
	dst->SetMember(name, &fxValue);
}

void RegisterBool(GFxValue * dst, const char * name, bool value)
{
	GFxValue	fxValue;
	fxValue.SetBool(value);
	dst->SetMember(name, &fxValue);
}

void RegisterUnmanagedString(GFxValue * dst, const char * name, const char * str)
{
	GFxValue	fxValue;
	fxValue.SetString(str);
	dst->SetMember(name, &fxValue);
}

void RegisterString(GFxValue * dst, GFxMovieView * view, const char * name, const char * str)
{
	GFxValue	fxValue;
	view->CreateString(&fxValue, str);
	dst->SetMember(name, &fxValue);
}