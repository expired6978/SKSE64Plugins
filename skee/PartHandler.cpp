#include "PartHandler.h"
#include "FileUtils.h"

#include "skse64/GameReferences.h"
#include "skse64/GameObjects.h"
#include "skse64/GameForms.h"
#include "skse64/GameRTTI.h"

#include "skse64/GameStreams.h"

void PartSet::AddPart(UInt32 key, BGSHeadPart* part)
{
	iterator it = find(key);
	if(it != end()) {
		it->second.partList.push_back(part);
	} else {
		PartEntry partEntry;
		partEntry.partList.push_back(part);
		insert(std::make_pair(key, partEntry));
	}
}

void PartSet::SetDefaultPart(UInt32 key, BGSHeadPart* part)
{
	iterator it = find(key);
	if(it != end()) {
		it->second.defaultPart = part;
	} else {
		PartEntry partEntry;
		partEntry.defaultPart = part;
		insert(std::make_pair(key, partEntry));
	}
}

void PartSet::Visit(Visitor & visitor)
{
	for (iterator it = begin(); it != end(); ++it)
	{
		for(HeadPartList::iterator pit = it->second.partList.begin(); pit != it->second.partList.end(); ++pit)
		{
			if(visitor.Accept(it->first, *pit))
				return;
		}
	}
}

HeadPartList * PartSet::GetPartList(UInt32 key)
{
	iterator it = find(key);
	if(it != end()) {
		return &it->second.partList;
	}

	return NULL;
}

BGSHeadPart * PartSet::GetDefaultPart(UInt32 key)
{
	iterator it = find(key);
	if(it != end()) {
		return it->second.defaultPart;
	}

	return NULL;
}

BGSHeadPart * PartSet::GetPartByIndex(HeadPartList * headPartList, UInt32 index)
{
	if(index < headPartList->size())
		return headPartList->at(index);

	return NULL;
}

SInt32 PartSet::GetPartIndex(HeadPartList * headPartList, BGSHeadPart * headPart)
{
	SInt32 partIndex = -1;
	for(UInt32 p = 0; p < headPartList->size(); p++)
	{
		BGSHeadPart * partMatch = headPartList->at(p);
		if(partMatch->partName == headPart->partName) {
			partIndex = p;
			break;
		}
	}

	return partIndex;
}

void PartSet::Revert()
{
	for (iterator it = begin(); it != end(); ++it)
	{
		it->second.partList.clear();
	}

	clear();
}


void ReadPartReplacements(std::string fixedPath, std::string modPath, std::string fileName)
{
	std::string fullPath = fixedPath + modPath + fileName;
	BSResourceNiBinaryStream file(fullPath.c_str());
	if (!file.IsValid()) {
		return;
	}

	UInt32 lineCount = 0;
	UInt8 gender = 0;
	std::string str = "";
	while (BSReadLine(&file, &str))
	{
		lineCount++;
		str = std::trim(str);
		if (str.length() == 0)
			continue;
		if (str.at(0) == '#')
			continue;

		if (str.at(0) == '[')
		{
			str.erase(0, 1);
			if (_strnicmp(str.c_str(), "Male", 4) == 0)
				gender = 0;
			if (_strnicmp(str.c_str(), "Female", 6) == 0)
				gender = 1;
			continue;
		}

		std::vector<std::string> side = std::explode(str, '=');
		if (side.size() < 2) {
			_ERROR("%s Error - Line (%d) race from %s has no left-hand side.", __FUNCTION__, lineCount, fullPath.c_str());
			continue;
		}

		std::string lSide = std::trim(side[0]);
		std::string rSide = std::trim(side[1]);

		BGSHeadPart * facePart = GetHeadPartByName(rSide);
		TESRace * race = GetRaceByName(lSide);
		if (!race) {
			_WARNING("%s Warning - Line (%d) race %s from %s is not a valid race.", __FUNCTION__, lineCount, lSide.c_str(), fullPath.c_str());
			continue;
		}

		if (!facePart) {
			_WARNING("%s Warning - Line (%d) head part %s from %s is not a valid head part.", __FUNCTION__, lineCount, rSide.c_str(), fullPath.c_str());
			continue;
		}

		auto charGenData = race->chargenData[gender];
		if (!charGenData) {
			_ERROR("%s Error - Line (%d) race %s from %s has no CharGen data.", __FUNCTION__, lineCount, lSide.c_str(), fullPath.c_str());
			continue;
		}

		if (charGenData->headParts) {
			for (UInt32 i = 0; i < charGenData->headParts->count; i++) {
				BGSHeadPart * headPart = NULL;
				if (charGenData->headParts->GetNthItem(i, headPart)) {
					if (headPart->type == facePart->type) {
						charGenData->headParts->entries[i] = facePart;
					}
				}
			}
		}
	}
}