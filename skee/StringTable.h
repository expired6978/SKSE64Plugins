#pragma once

#include "skse64/GameTypes.h"
#include "skse64/PluginAPI.h"
#include "common/ICriticalSection.h"
#include "Utilities.h"
#include <unordered_map>
#include <memory>
#include <vector>

struct SKSESerializationInterface;

class SKEEFixedString
{
public:
	SKEEFixedString() : m_internal() { m_hash = utils::hash_lower(m_internal.c_str(), m_internal.size()); }
	SKEEFixedString(const char * str) : m_internal(str) { m_hash = utils::hash_lower(m_internal.c_str(), m_internal.size()); }
	SKEEFixedString(const std::string & str) : m_internal(str) { m_hash = utils::hash_lower(m_internal.c_str(), m_internal.size()); }
	SKEEFixedString(const BSFixedString & str) : m_internal(str.c_str()) { m_hash = utils::hash_lower(m_internal.c_str(), m_internal.size()); }

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
	operator BSFixedString() const { return BSFixedString(m_internal.c_str()); }
	BSFixedString AsBSFixedString() const { return operator BSFixedString(); }

	const char * c_str() const { return operator const char *(); }
	operator const char *() const { return m_internal.c_str(); }

	size_t GetHash() const
	{
		return m_hash;
	}

protected:
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
	bool WriteData(const SKSESerializationInterface * intfc, const T * data);

	template <typename T>
	bool ReadData(const SKSESerializationInterface * intfc, T * data);

	template<> bool WriteData<SKEEFixedString>(const SKSESerializationInterface * intfc, const SKEEFixedString * str);
	template<> bool ReadData<SKEEFixedString>(const SKSESerializationInterface * intfc, SKEEFixedString * str);
}

typedef std::unordered_map<UInt32, StringTableItem> StringIdMap;

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

	void Save(const SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(const SKSESerializationInterface * intfc, UInt32 kVersion, StringIdMap & stringTable);
	void Revert();

	StringTableItem GetString(const SKEEFixedString & str);

	UInt32 GetStringID(const StringTableItem & str);

	void RemoveString(const SKEEFixedString & str);

	static StringTableItem ReadString(const SKSESerializationInterface * intfc, const StringIdMap & stringTable)
	{
		UInt32 stringId;
		if (!Serialization::ReadData<UInt32>(intfc, &stringId))
		{
			_ERROR("%s - Error loading string id", __FUNCTION__);
			return nullptr;
		}

		auto it = stringTable.find(stringId);
		if (it == stringTable.end())
		{
			_ERROR("%s - Error loading string from table", __FUNCTION__);
			return nullptr;
		}

		return it->second;
	}

	void WriteString(const SKSESerializationInterface * intfc, const StringTableItem & str)
	{
		UInt32 stringId = GetStringID(str);
		Serialization::WriteData<UInt32>(intfc, &stringId);
	}

	void PrintDiagnostics();

private:
	std::unordered_map<SKEEFixedString, WeakTableItem>	m_table;
	std::vector<WeakTableItem>							m_tableVector;
	mutable ICriticalSection							m_lock;
};