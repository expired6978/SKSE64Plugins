#include "StringTable.h"

#include "skse64/PluginAPI.h"

extern StringTable g_stringTable;

using namespace Serialization;

void DeleteStringEntry(const SKEEFixedString* string)
{
	g_stringTable.RemoveString(*string);
	delete string;
}

StringTableItem StringTable::GetString(const SKEEFixedString & str)
{
	IScopedCriticalSection locker(&m_lock);

	auto it = m_table.find(str);
	if (it != m_table.end()) {
		return it->second.lock();
	}
	else {
		StringTableItem item = std::shared_ptr<SKEEFixedString>(new SKEEFixedString(str), DeleteStringEntry);
		m_table.emplace(str, item);
		m_tableVector.push_back(item);
		return item;
	}

	return nullptr;
}

void StringTable::RemoveString(const SKEEFixedString & str)
{
	IScopedCriticalSection locker(&m_lock);

	auto it = m_table.find(str);
	if (it != m_table.end())
	{
		for (long int i = m_tableVector.size() - 1; i >= 0; --i)
		{
			if (m_tableVector[i].lock() == it->second.lock())
				m_tableVector.erase(m_tableVector.begin() + i);
		}

		m_table.erase(it);
	}
}

UInt32 StringTable::GetStringID(const StringTableItem & str)
{
	for (long int i = m_tableVector.size() - 1; i >= 0; --i)
	{
		auto item = m_tableVector[i].lock();
		if (item == str)
			return i;
	}

	return -1;
}

void StringTable::Save(const SKSESerializationInterface * intfc, UInt32 kVersion)
{
	intfc->OpenRecord('STTB', kVersion);

	UInt32 totalStrings = m_tableVector.size();
	WriteData<UInt32>(intfc, &totalStrings);

	for (auto & str : m_tableVector)
	{
		auto data = str.lock();
		UInt16 length = 0;
		if (!data) {
			WriteData<UInt16>(intfc, &length);
		}
		else {
			WriteData<SKEEFixedString>(intfc, data.get());
		}
	}
}

bool StringTable::Load(const SKSESerializationInterface * intfc, UInt32 kVersion, StringIdMap & stringTable)
{
	bool error = false;
	UInt32 totalStrings = 0;

	if (kVersion >= kSerializationVersion3)
	{
		if (!ReadData<UInt32>(intfc, &totalStrings))
		{
			_ERROR("%s - Error loading total strings from table", __FUNCTION__);
			error = true;
			return error;
		}

		for (UInt32 i = 0; i < totalStrings; i++)
		{
			SKEEFixedString str;
			if (!ReadData<SKEEFixedString>(intfc, &str)) {
				_ERROR("%s - Error loading string", __FUNCTION__);
				error = true;
				return error;
			}

			StringTableItem item = GetString(str);
			stringTable.emplace(i, item);
		}
	}
	else if (kVersion >= kSerializationVersion2)
	{
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

			SKEEFixedString str(stringName);
			delete[] stringName;

			UInt32 stringId = 0;
			if (!intfc->ReadRecordData(&stringId, sizeof(stringId)))
			{
				_ERROR("%s - Error loading string id", __FUNCTION__);
				error = true;
				return error;
			}

			StringTableItem item = GetString(str);
			stringTable.emplace(stringId, item);
		}
	}

	return error;
}

void StringTable::Revert()
{
	IScopedCriticalSection locker(&m_lock);
	m_table.clear();
	m_tableVector.clear();
}

template <typename T>
bool Serialization::WriteData(const SKSESerializationInterface * intfc, const T * data)
{
	return intfc->WriteRecordData(data, sizeof(T));
}

template <typename T>
bool Serialization::ReadData(const SKSESerializationInterface * intfc, T * data)
{
	return intfc->ReadRecordData(data, sizeof(T)) > 0;
}

template<>
bool Serialization::WriteData<SKEEFixedString>(const SKSESerializationInterface * intfc, const SKEEFixedString * str)
{
	UInt16 len = str->length();
	if (len > SHRT_MAX)
		return false;
	if (!intfc->WriteRecordData(&len, sizeof(len)))
		return false;
	if (len == 0)
		return true;
	if (!intfc->WriteRecordData(str->c_str(), len))
		return false;
	return true;
}

template<>
bool Serialization::ReadData<SKEEFixedString>(const SKSESerializationInterface * intfc, SKEEFixedString * str)
{
	UInt16 len = 0;

	if (!intfc->ReadRecordData(&len, sizeof(len)))
		return false;
	if (len == 0)
		return true;
	if (len > SHRT_MAX)
		return false;

	char * buf = new char[len + 1];
	buf[0] = 0;

	if (!intfc->ReadRecordData(buf, len)) {
		delete[] buf;
		return false;
	}
	buf[len] = 0;

	*str = SKEEFixedString(buf);
	delete[] buf;
	return true;
}