#pragma once

class TESObjectREFR;
class NiNode;
class NiAVObject;
class NiStringsExtraData;
class NiStringExtraData;

#include <set>
#include "skse64\GameTypes.h"
#include "StringTable.h"

class SkeletonExtender
{
public:
	static void Attach(TESObjectREFR * refr, NiNode * skeleton, NiAVObject * objectRoot);
	static NiNode * LoadTemplate(NiNode * parent, const char * path);
	static void AddTransforms(TESObjectREFR * refr, bool isFirstPerson, NiNode * skeleton, NiAVObject * objectRoot);
	static void ReadTransforms(TESObjectREFR * refr, const char * jsonData, bool isFirstPerson, bool isFemale, std::set<SKEEFixedString> & nodes, std::set<SKEEFixedString> & changes);
};