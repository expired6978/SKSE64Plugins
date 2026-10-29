#include "StringTable.h"

#include <SKSE/Interfaces.h>

#include "Utilities.h"

#include <cstdint>

using namespace Serialization;

struct StringTable::Storage
{
	std::unordered_map<SKEEFixedString, WeakTableItem> m_table;
	std::vector<WeakTableItem> m_tableVector;
	std::recursive_mutex m_lock;

	void RemoveString(const SKEEFixedString& str, const SKEEFixedString* expected = nullptr);
};

StringTable::StringTable() : m_storage(std::make_shared<Storage>()) {}

void StringTable::PrintDiagnostics()
{
	Console_Print("StringTable Diagnostics:");
	std::lock_guard<std::recursive_mutex> locker(m_storage->m_lock);
	Console_Print("\t%llu string entries", m_storage->m_table.size());
	size_t byteSize = 0;
	for (auto& item : m_storage->m_table)
	{
		byteSize += item.first.length();
		SKSE::log::info("String: {}", item.first.c_str());
	}
	Console_Print("\t%llu total bytes", byteSize);
}

StringTableItem StringTable::GetString(const SKEEFixedString & str)
{
	auto storage = m_storage;
	std::lock_guard<std::recursive_mutex> locker(storage->m_lock);
	auto it = storage->m_table.find(str);
	if (it != storage->m_table.end()) {
		if (auto existing = it->second.lock())
			return existing;
		// The old last-owner callback may still be waiting for this mutex.
		// Remove its expired record before publishing a live replacement.
		storage->RemoveString(str);
	}
	{
		StringTableItem item(new SKEEFixedString(str), [storage](const SKEEFixedString* string) mutable {
			storage->RemoveString(*string, string);
			delete string;
			// Weak observers may retain the deleter after the string dies.
			// They must not keep an otherwise unused intern table alive.
			storage.reset();
		});
		storage->m_table.emplace(str, item);
		storage->m_tableVector.push_back(item);
		return item;
	}
}

void StringTable::RemoveString(const SKEEFixedString & str)
{
	m_storage->RemoveString(str);
}

void StringTable::Storage::RemoveString(const SKEEFixedString & str, const SKEEFixedString* expected)
{
	std::lock_guard<std::recursive_mutex> locker(m_lock);
	auto it = m_table.find(str);
	if (it != m_table.end())
	{
		// Revert may have removed an old item and created a replacement.
		// A late old deleter must not erase a live new item with equal text.
		auto current = it->second.lock();
		if (expected && current && current.get() != expected)
			return;
		for (auto i = m_tableVector.begin(); i != m_tableVector.end(); ++i)
		{
			if (!i->owner_before(it->second) && !it->second.owner_before(*i)) {
				i = m_tableVector.erase(i);
				break;
			}
		}

		m_table.erase(it);
	}
}

std::uint32_t StringTable::GetStringID(const StringTableItem & str)
{
	std::lock_guard<std::recursive_mutex> locker(m_storage->m_lock);
	std::uint32_t i = 0;
	for (auto it = m_storage->m_tableVector.begin(); it != m_storage->m_tableVector.end(); ++it, ++i)
	{
		auto item = it->lock();
		if (item == str)
			return i;
	}

	return -1;
}

void StringTable::Save(const SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	if (!intfc->OpenRecord('STTB', kVersion)) {
		SKSE::log::error("{} - Failed to open record", __FUNCTION__);
	}

	std::lock_guard<std::recursive_mutex> locker(m_storage->m_lock);
	std::uint32_t totalStrings = m_storage->m_tableVector.size();
	WriteData<std::uint32_t>(intfc, &totalStrings);

	for (auto & str : m_storage->m_tableVector)
	{
		auto data = str.lock();
		std::uint16_t length = 0;
		if (!data) {
			WriteData<std::uint16_t>(intfc, &length);
		}
		else {
			WriteData<SKEEFixedString>(intfc, data.get());
		}
	}
}

bool StringTable::Load(const SKSE::SerializationInterface * intfc, std::uint32_t kVersion, StringIdMap & stringTable)
{
	bool error = false;
	std::uint32_t totalStrings = 0;

	if (kVersion >= kSerializationVersion3)
	{
		if (!ReadData<std::uint32_t>(intfc, &totalStrings))
		{
			SKSE::log::error("{} - Error loading total strings from table", __FUNCTION__);
			error = true;
			return error;
		}

		for (std::uint32_t i = 0; i < totalStrings; i++)
		{
			SKEEFixedString str;
			if (!ReadData<SKEEFixedString>(intfc, &str)) {
				SKSE::log::error("{} - Error loading string", __FUNCTION__);
				error = true;
				return error;
			}

			StringTableItem item = GetString(str);
			stringTable.emplace(i, item);
		}
	}
	else if (kVersion >= kSerializationVersion1)
	{
		if (!intfc->ReadRecordData(&totalStrings, sizeof(totalStrings)))
		{
			SKSE::log::error("{} - Error loading total strings from table", __FUNCTION__);
			error = true;
			return error;
		}

		for (std::uint32_t i = 0; i < totalStrings; i++)
		{
			char * stringName = NULL;
			std::uint16_t stringLength = 0;
			if (!intfc->ReadRecordData(&stringLength, sizeof(stringLength)))
			{
				SKSE::log::error("{} - Error loading string length", __FUNCTION__);
				error = true;
				return error;
			}

			stringName = new char[stringLength + 1];

			if (stringLength > 0 && !intfc->ReadRecordData(stringName, stringLength)) {
				SKSE::log::error("{} - Error loading string of length {}", __FUNCTION__, stringLength);
				error = true;
				return error;
			}

			stringName[stringLength] = 0;

			SKEEFixedString str(stringName);
			delete[] stringName;

			std::uint32_t stringId = 0;
			if (!intfc->ReadRecordData(&stringId, sizeof(stringId)))
			{
				SKSE::log::error("{} - Error loading string id", __FUNCTION__);
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
	std::lock_guard<std::recursive_mutex> locker(m_storage->m_lock);
	m_storage->m_table.clear();
	m_storage->m_tableVector.clear();
}

template <typename T>
bool Serialization::WriteData(const SKSE::SerializationInterface * intfc, const T * data)
{
	return intfc->WriteRecordData(data, sizeof(T));
}

template <typename T>
bool Serialization::ReadData(const SKSE::SerializationInterface * intfc, T * data)
{
	return intfc->ReadRecordData(data, sizeof(T)) > 0;
}

template<>
bool Serialization::WriteData<SKEEFixedString>(const SKSE::SerializationInterface * intfc, const SKEEFixedString * str)
{
	std::uint16_t len = str->length();
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
bool Serialization::ReadData<SKEEFixedString>(const SKSE::SerializationInterface * intfc, SKEEFixedString * str)
{
	std::uint16_t len = 0;

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
