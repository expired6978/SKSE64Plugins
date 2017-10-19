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
