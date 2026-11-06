#include "StringTable.h"

#include <SKSE/Interfaces.h>

#include "Utilities.h"

#include <cstdint>

extern StringTable g_stringTable;

using namespace Serialization;

void StringTable::PrintDiagnostics()
{
	Console_Print("StringTable Diagnostics:");
	std::lock_guard<std::recursive_mutex> locker(m_lock);
	Console_Print("\t%llu string entries", m_table.size());
	size_t byteSize = 0;
	for (auto& item : m_table)
	{
		byteSize += item.first.length();
		SKSE::log::info("String: {}", item.first.c_str());
	}
	Console_Print("\t%llu total bytes", byteSize);
}

void DeleteStringEntry(const SKEEFixedString* string)
{
	g_stringTable.RemoveString(*string);
	delete string;
}

StringTableItem StringTable::GetString(const SKEEFixedString & str)
{
	std::lock_guard<std::recursive_mutex> locker(m_lock);
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
	std::lock_guard<std::recursive_mutex> locker(m_lock);
	auto it = m_table.find(str);
	if (it != m_table.end())
	{
		for (auto i = m_tableVector.begin(); i != m_tableVector.end(); ++i)
		{
			if (i->lock() == it->second.lock()) {
				i = m_tableVector.erase(i);
				break;
			}
		}

		m_table.erase(it);
	}
}

std::uint32_t StringTable::GetStringID(const StringTableItem & str)
{
	std::lock_guard<std::recursive_mutex> locker(m_lock);
	std::uint32_t i = 0;
	for (auto it = m_tableVector.begin(); it != m_tableVector.end(); ++it, ++i)
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

	std::lock_guard<std::recursive_mutex> locker(m_lock);
	std::uint32_t totalStrings = m_tableVector.size();
	WriteData<std::uint32_t>(intfc, &totalStrings);

	for (auto & str : m_tableVector)
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
	std::lock_guard<std::recursive_mutex> locker(m_lock);
	m_table.clear();
	m_tableVector.clear();
}

template<>
bool Serialization::WriteData<SKEEFixedString>(const SKSE::SerializationInterface * intfc, const SKEEFixedString * str)
{
	return str && Detail::WriteString(intfc, str->c_str(), str->length());
}

template<>
bool Serialization::ReadData<SKEEFixedString>(const SKSE::SerializationInterface * intfc, SKEEFixedString * str)
{
	std::string decoded;
	if (!str || !Detail::ReadString(intfc, decoded)) return false;
	*str = SKEEFixedString(decoded);
	return true;
}

template<>
bool Serialization::WriteData<RE::BSFixedString>(const SKSE::SerializationInterface* intfc, const RE::BSFixedString* str)
{
	if (!str) return false;
	const auto* text = str->c_str();
	return Detail::WriteString(intfc, text, text ? str->length() : 0);
}

template<>
bool Serialization::ReadData<RE::BSFixedString>(const SKSE::SerializationInterface* intfc, RE::BSFixedString* str)
{
	std::string decoded;
	if (!str || !Detail::ReadString(intfc, decoded)) return false;
	*str = RE::BSFixedString(decoded.c_str());
	return true;
}
