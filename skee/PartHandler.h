#pragma once

#include <vector>
#include <unordered_map>

#include "FileUtils.h"

class BGSHeadPart;
class SliderArray;
class RaceMenuSlider;

typedef std::vector<BGSHeadPart*> HeadPartList;

class PartEntry
{
public:
	BGSHeadPart * defaultPart;
	HeadPartList partList;
};

class PartSet : public std::unordered_map<UInt32, PartEntry>
{
public:
	class Visitor
	{
	public:
		virtual bool Accept(UInt32 key, BGSHeadPart * part) { return false; };
	};

	void LoadSliders(SliderArray * sliderArray, RaceMenuSlider * slider);

	void AddPart(UInt32 key, BGSHeadPart* part);
	void Visit(Visitor & visitor);

	HeadPartList * GetPartList(UInt32 key);
	BGSHeadPart * GetDefaultPart(UInt32 key);
	void SetDefaultPart(UInt32 key, BGSHeadPart* part);
	SInt32 GetPartIndex(HeadPartList * headPartList, BGSHeadPart * headPart);
	BGSHeadPart * GetPartByIndex(HeadPartList * headPartList, UInt32 index);

	void Revert();
};


void ReadPartReplacements(std::string fixedPath, std::string modPath, std::string fileName);