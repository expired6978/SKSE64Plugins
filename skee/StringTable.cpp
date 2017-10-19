#include "StringTable.h"

#include "skse64/PluginAPI.h"


void StringTable::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	intfc->OpenRecord('STTB', kVersion);

	UInt32 totalStrings = m_stringToId.size();
	intfc->WriteRecordData(&totalStrings, sizeof(totalStrings));

	for (auto & str : m_stringToId)
	{
		UInt16 length = strlen(str.first.data);
		UInt32 strId = str.second;
		intfc->WriteRecordData(&length, sizeof(length));
		intfc->WriteRecordData(str.first.data, length);
		intfc->WriteRecordData(&strId, sizeof(strId));
	}
}

bool StringTable::Load(SKSESerializationInterface* intfc, UInt32 kVersion)
{
	bool error = false;
	UInt32 totalStrings = 0;

	if (!intfc->ReadRecordData(&totalStrings, sizeof(totalStrings)))
	{
		_ERROR("%s - Error loading total strings from table", __FUNCTION__);
		error = true;
		return error;
	}

	for (UInt32 i = 0; i < totalStrings; i++)
	{
		char * stringName = NULL;
		UInt16 stringLength = 0;
		if (!intfc->ReadRecordData(&stringLength, sizeof(stringLength)))
		{
			_ERROR("%s - Error loading string length", __FUNCTION__);
			error = true;
			return error;
		}

		stringName = new char[stringLength + 1];

		if (stringLength > 0 && !intfc->ReadRecordData(stringName, stringLength)) {
			_ERROR("%s - Error loading string of length %d", __FUNCTION__, stringLength);
			error = true;
			return error;
		}

		stringName[stringLength] = 0;

		BSFixedString str(stringName);
		delete[] stringName;

		UInt32 stringId = 0;
		if (!intfc->ReadRecordData(&stringId, sizeof(stringId)))
		{
			_ERROR("%s - Error loading string id", __FUNCTION__);
			error = true;
			return error;
		}

		m_idToString.insert_or_assign(stringId, str);
	}

	return error;
}

void StringTable::StringToId(const BSFixedString & str, const UInt32 & id)
{
	m_stringToId.insert_or_assign(str, id);
}

void StringTable::IdToString(const UInt32 & id, const BSFixedString & str)
{
	m_idToString.insert_or_assign(id, str);
}

void StringTable::Clear()
{
	m_stringToId.clear();
	m_idToString.clear();
}
