#include "FileUtils.h"

#include "skse64/GameStreams.h"
#include "skse64/GameData.h"
#include "skse64/GameForms.h"

// trim from start
namespace std
{
	std::string &ltrim(std::string &s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
		return s;
	}

	// trim from end
	std::string &rtrim(std::string &s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
		return s;
	}

	// trim from both ends
	std::string &trim(std::string &s) {
		return ltrim(rtrim(s));
	}

	// return vector of delimited string
	std::vector<std::string> explode(const std::string& str, const char& ch) {
		std::string next;
		std::vector<std::string> result;

		for (std::string::const_iterator it = str.begin(); it != str.end(); it++) {
			if (*it == ch) {
				if (!next.empty()) {
					result.push_back(next);
					next.clear();
				}
			}
			else {
				next += *it;
			}
		}
		if (!next.empty())
			result.push_back(next);
		return result;
	}
}

bool BSReadLine(BSResourceNiBinaryStream* fin, std::string* str)
{
	char buf[1024];
	UInt32 ret = 0;

	ret = fin->ReadLine((char*)buf, 1024, '\n');
	if (ret > 0) {
		buf[ret - 1] = '\0';
		*str = buf;
		return true;
	}
	return false;
}

void BSReadAll(BSResourceNiBinaryStream* fin, std::string* str)
{
	char ch;
	UInt32 ret = fin->Read(&ch, 1);
	while (ret > 0) {
		str->push_back(ch);
		ret = fin->Read(&ch, 1);
	}
}

TESRace * GetRaceByName(std::string & raceName)
{
	DataHandler * dataHandler = DataHandler::GetSingleton();
	if (dataHandler)
	{
		for (UInt32 i = 0; i < dataHandler->races.count; i++)
		{
			TESRace * race = NULL;
			if (dataHandler->races.GetNthItem(i, race)) {
				BSFixedString raceStrName(raceName.c_str());
				if (race->editorId == raceStrName) {
					CALL_MEMBER_FN(&raceStrName, Release)();
					return race;
				}
				CALL_MEMBER_FN(&raceStrName, Release)();
			}
		}
	}

	return NULL;
}

BGSHeadPart * GetHeadPartByName(std::string & headPartName)
{
	DataHandler * dataHandler = DataHandler::GetSingleton();
	if (dataHandler)
	{
		for (UInt32 i = 0; i < dataHandler->headParts.count; i++)
		{
			BGSHeadPart * headPart = NULL;
			if (dataHandler->headParts.GetNthItem(i, headPart)) {
				BSFixedString partName(headPartName.c_str());
				if (headPart->partName == partName) {
					CALL_MEMBER_FN(&partName, Release)();
					return headPart;
				}
				CALL_MEMBER_FN(&partName, Release)();
			}
		}
	}

	return NULL;
}

ModInfo* GetModInfoByFormID(UInt32 formId, bool allowLight)
{
	DataHandler * dataHandler = DataHandler::GetSingleton();

	UInt8 modIndex = formId >> 24;
	UInt16 lightIndex = ((formId >> 12) & 0xFFF);

	ModInfo* modInfo = nullptr;
	if (modIndex == 0xFE && allowLight) {
		if (lightIndex < dataHandler->modList.loadedCCMods.count)
			dataHandler->modList.loadedCCMods.GetNthItem(lightIndex, modInfo);
	} else {
		if(modIndex < 0xFE)
			dataHandler->modList.loadedMods.GetNthItem(modIndex, modInfo);
	}

	return modInfo;
}

std::string GetFormIdentifier(TESForm * form)
{
	char formName[MAX_PATH];
	UInt8 modIndex = form->formID >> 24;
	UInt32 modForm = form->formID & 0xFFFFFF;

	ModInfo* modInfo = nullptr;
	if (modIndex == 0xFE)
	{
		UInt16 lightIndex = (form->formID >> 12) & 0xFFF;
		if (lightIndex < (*g_dataHandler)->modList.loadedCCMods.count)
			modInfo = (*g_dataHandler)->modList.loadedCCMods[lightIndex];
	}
	else
	{
		modInfo = (*g_dataHandler)->modList.loadedMods[modIndex];
	}

	if (modInfo) {
		sprintf_s(formName, "%s|%06X", modInfo->name, modForm);
	}

	return formName;
}

TESForm * GetFormFromIdentifier(const std::string & formIdentifier)
{
	std::size_t pos = formIdentifier.find_first_of('|');
	std::string modName = formIdentifier.substr(0, pos);
	std::string modForm = formIdentifier.substr(pos + 1);

	UInt32 formId = 0;
	sscanf_s(modForm.c_str(), "%X", &formId);

	UInt8 modIndex = (*g_dataHandler)->GetLoadedModIndex(modName.c_str());
	if (modIndex != 0xFF) {
		formId |= ((UInt32)modIndex) << 24;
	}
	else
	{
		UInt16 lightModIndex = (*g_dataHandler)->GetLoadedLightModIndex(modName.c_str());
		if (lightModIndex != 0xFFFF) {
			formId |= 0xFE000000 | (UInt32(lightModIndex) << 12);
		}
	}

	return LookupFormByID(formId);
}

void ForEachMod(std::function<void(ModInfo *)> functor)
{
	for (UInt32 i = 0; i < (*g_dataHandler)->modList.loadedMods.count; i++)
	{
		functor((*g_dataHandler)->modList.loadedMods[i]);
	}

	for (UInt32 i = 0; i < (*g_dataHandler)->modList.loadedCCMods.count; i++)
	{
		functor((*g_dataHandler)->modList.loadedCCMods[i]);
	}
}