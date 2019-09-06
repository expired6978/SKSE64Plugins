#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <functional> 
#include <cctype>
#include <locale>

#include "skse64/GameTypes.h"
#include "StringTable.h"

class TESRace;
class BGSHeadPart;
class BSResourceNiBinaryStream;
struct ModInfo;
class TESLevCharacter;
class TESNPC;

namespace std {
	extern inline std::string &ltrim(std::string &s);
	extern inline std::string &rtrim(std::string &s);
	extern inline std::string &trim(std::string &s);
	std::vector<std::string> explode(const std::string& str, const char& ch);
}

namespace BSFileUtil {
	bool ReadLine(BSResourceNiBinaryStream* fin, std::string * str);

	template<typename Container>
	void ReadAll(BSResourceNiBinaryStream* fin, Container & c)
	{
		char ch;
		UInt32 ret = fin->Read(&ch, 1);
		while (ret > 0) {
			c.push_back(ch);
			ret = fin->Read(&ch, 1);
		}
	}
}

namespace FileUtils
{
	void GetAllFiles(LPCTSTR lpFolder, LPCTSTR lpFilePattern, std::vector<SKEEFixedString> & filePaths);
}

TESRace * GetRaceByName(std::string & raceName);
BGSHeadPart * GetHeadPartByName(std::string & headPartName);

ModInfo* GetModInfoByFormID(UInt32 formId, bool allowLight = true);

std::string GetFormIdentifier(TESForm * form);
TESForm * GetFormFromIdentifier(const std::string & formIdentifier);

void ForEachMod(std::function<void(ModInfo *)> functor);

template<int MaxBuf>
class BSResourceTextFile
{
public:
	BSResourceTextFile(BSResourceNiBinaryStream* file) : fin(file) { }

	bool ReadLine(std::string* str)
	{
		UInt32 ret = fin->ReadLine((char*)buf, MaxBuf, '\n');
		if (ret > 0) {
			*str = buf;
			return true;
		}
		return false;
	}

protected:
	BSResourceNiBinaryStream * fin;
	char buf[MaxBuf];
};

void VisitLeveledCharacter(TESLevCharacter * character, std::function<void(TESNPC*)> functor);