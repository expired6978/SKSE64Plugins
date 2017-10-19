#pragma once

class GFxValue;
class GFxMovieView;

void RegisterNumber(GFxValue * dst, const char * name, double value);
void RegisterBool(GFxValue * dst, const char * name, bool value);
void RegisterUnmanagedString(GFxValue * dst, const char * name, const char * str);
void RegisterString(GFxValue * dst, GFxMovieView * view, const char * name, const char * str);