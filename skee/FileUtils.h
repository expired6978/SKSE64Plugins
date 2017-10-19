#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <functional> 
#include <cctype>
#include <locale>

#include "skse64/GameTypes.h"

class TESRace;
class BGSHeadPart;
class BSResourceNiBinaryStream;

namespace std {
	extern inline std::string &ltrim(std::string &s);
	extern inline std::string &rtrim(std::string &s);
	extern inline std::string &trim(std::string &s);
	std::vector<std::string> explode(const std::string& str, const char& ch);
}


bool BSReadLine(BSResourceNiBinaryStream* fin, std::string* str);
void BSReadAll(BSResourceNiBinaryStream* fin, std::string* str);

TESRace * GetRaceByName(std::string & raceName);
BGSHeadPart * GetHeadPartByName(std::string & headPartName);