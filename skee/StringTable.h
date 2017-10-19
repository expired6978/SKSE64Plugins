#pragma once

#include "skse64/GameTypes.h"

struct SKSESerializationInterface;

class StringTable
{
public:
	enum
	{
		kSerializationVersion1 = 1,
		kSerializationVersion2 = 2,
		kSerializationVersion = kSerializationVersion2
	};

	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface* intfc, UInt32 kVersion);

	void Clear();

	void StringToId(const BSFixedString & str, const UInt32 & id);
	void IdToString(const UInt32 & id, const BSFixedString & str);

	template<typename T>
	BSFixedString ReadString(SKSESerializationInterface* intfc, UInt32 kVersion)
	{
		BSFixedString str("");
		if (kVersion >= kSerializationVersion2)
		{
			UInt32 stringId = 0;
			if (!intfc->ReadRecordData(&stringId, sizeof(stringId)))
			{
				_ERROR("%s - Error loading string id", __FUNCTION__);
				return BSFixedString("");
			}

			auto & it = m_idToString.find(stringId);
			if (it != m_idToString.end())
			{
				str = it->second;
			}
			else
			{
				_ERROR("%s - Error string lookup failure (%08X)", __FUNCTION__, stringId);
				return BSFixedString("");
			}
		}
		else
		{
			char * stringName = NULL;
			T stringLength = 0;
			if (!intfc->ReadRecordData(&stringLength, sizeof(stringLength)))
			{
				_ERROR("%s - Error loading string length", __FUNCTION__);
				return BSFixedString("");
			}

			stringName = new char[stringLength + 1];
			if (!intfc->ReadRecordData(stringName, stringLength)) {
				_ERROR("%s - Error loading string of length %d", __FUNCTION__, stringLength);
				return BSFixedString("");
			}
			stringName[stringLength] = 0;

			str = stringName;
			delete[] stringName;
		}
		return str;
	}

	template<typename T>
	void WriteString(SKSESerializationInterface* intfc, const BSFixedString & str, UInt32 kVersion)
	{
		if (kVersion >= kSerializationVersion2)
		{
			auto & it = m_stringToId.find(str);
			if (it != m_stringToId.end())
			{
				UInt32 stringId = it->second;
				intfc->WriteRecordData(&stringId, sizeof(stringId));
			}
			else
			{
				_ERROR("%s - Error mapping string %s to id", __FUNCTION__, str.data);
				UInt32 stringId = (std::numeric_limits<UInt32>::max)();
				intfc->WriteRecordData(&stringId, sizeof(stringId));
			}
		}
		else
		{
			T length = strlen(str.data);
			intfc->WriteRecordData(&length, sizeof(length));
			intfc->WriteRecordData(str.data, length);
		}
	}

private:
	std::map<BSFixedString, UInt32> m_stringToId;
	std::map<UInt32, BSFixedString> m_idToString;
};