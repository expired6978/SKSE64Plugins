#pragma once

#include "RE/B/BSFixedString.h"
#include "SKSE/Interfaces.h"
#include "Utilities.h"
#include "SerializationPolicy.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <cstdint>
#include <mutex>

class SKEEFixedString
{
public:
	SKEEFixedString() : m_internal() { m_hash = utils::hash_lower(m_internal.c_str(), m_internal.size()); }
	SKEEFixedString(const char * str) : m_internal(str) { m_hash = utils::hash_lower(m_internal.c_str(), m_internal.size()); }
	SKEEFixedString(const std::string & str) : m_internal(str) { m_hash = utils::hash_lower(m_internal.c_str(), m_internal.size()); }
	SKEEFixedString(const RE::BSFixedString & str) : m_internal(str.c_str()) { m_hash = utils::hash_lower(m_internal.c_str(), m_internal.size()); }

	bool operator<(const SKEEFixedString& x) const
	{
		return _stricmp(m_internal.c_str(), x.m_internal.c_str()) < 0;
	}

	bool operator==(const SKEEFixedString& x) const
	{
		if (m_internal.size() != x.m_internal.size())
			return false;

		if (_stricmp(m_internal.c_str(), x.m_internal.c_str()) == 0)
			return true;

		return false;
	}
	
	size_t length() const { return m_internal.size(); }

	std::string AsString() const { return m_internal; }
	operator RE::BSFixedString() const { return RE::BSFixedString(m_internal.c_str()); }
	RE::BSFixedString AsBSFixedString() const { return operator RE::BSFixedString(); }

	const char * c_str() const { return m_internal.c_str(); }

	size_t GetHash() const
	{
		return m_hash;
	}

private:
	std::string		m_internal;
	size_t			m_hash;
};

typedef std::shared_ptr<SKEEFixedString> StringTableItem;
typedef std::weak_ptr<SKEEFixedString> WeakTableItem;

namespace std {
	template <> struct hash<SKEEFixedString>
	{
		size_t operator()(const SKEEFixedString & x) const
		{
			return x.GetHash();
		}
	};
	template <> struct hash<StringTableItem>
	{
		size_t operator()(const StringTableItem & x) const
		{
			return x->GetHash();
		}
	};
}

namespace Serialization
{
	template <typename T>
	bool WriteData(const SKSE::SerializationInterface* intfc, const T* data)
	{
		return Detail::WriteScalar(intfc, data);
	}

	template <typename T>
	bool ReadData(const SKSE::SerializationInterface* intfc, T* data)
	{
		return Detail::ReadScalar(intfc, data);
	}

	template<> bool WriteData<SKEEFixedString>(const SKSE::SerializationInterface* intfc, const SKEEFixedString* str);
	template<> bool ReadData<SKEEFixedString>(const SKSE::SerializationInterface* intfc, SKEEFixedString* str);
	template<> bool WriteData<RE::BSFixedString>(const SKSE::SerializationInterface* intfc, const RE::BSFixedString* str);
	template<> bool ReadData<RE::BSFixedString>(const SKSE::SerializationInterface* intfc, RE::BSFixedString* str);
}

typedef std::unordered_map<std::uint32_t, StringTableItem> StringIdMap;

class StringTable
{
public:
	enum
	{
		kSerializationVersion1 = 1,
		kSerializationVersion2 = 2,
		kSerializationVersion3 = 3,
		kSerializationVersion = kSerializationVersion3
	};

	void Save(const SKSE::SerializationInterface * intfc, std::uint32_t kVersion);
	bool Load(const SKSE::SerializationInterface * intfc, std::uint32_t kVersion, StringIdMap & stringTable);
	void Revert();

	StringTableItem GetString(const SKEEFixedString & str);

	std::uint32_t GetStringID(const StringTableItem & str);

	void RemoveString(const SKEEFixedString & str);

	static StringTableItem ReadString(const SKSE::SerializationInterface * intfc, const StringIdMap & stringTable)
	{
		std::uint32_t stringId;
		if (!Serialization::ReadData<std::uint32_t>(intfc, &stringId))
		{
			SKSE::log::error("{} - Error loading string id", __FUNCTION__);
			return nullptr;
		}

		auto it = stringTable.find(stringId);
		if (it == stringTable.end())
		{
			SKSE::log::error("{} - Error loading string from table", __FUNCTION__);
			return nullptr;
		}

		return it->second;
	}

	void WriteString(const SKSE::SerializationInterface * intfc, const StringTableItem & str)
	{
		std::uint32_t stringId = GetStringID(str);
		Serialization::WriteData<std::uint32_t>(intfc, &stringId);
	}

	void PrintDiagnostics();

private:
	std::unordered_map<SKEEFixedString, WeakTableItem>	m_table;
	std::vector<WeakTableItem>							m_tableVector;
	mutable std::recursive_mutex									m_lock;
};

extern StringTable g_stringTable;

