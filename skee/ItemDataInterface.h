#pragma once

struct SKSESerializationInterface;
class Actor;
class ItemAttributeData;

#include "skse64/GameThreads.h"
#include "skse64/GameExtraData.h"

#include "IPluginInterface.h"

#include <vector>
#include <map>
#include <unordered_map>

typedef std::map<SInt32, UInt32> ColorMap;

struct ModifiedItem
{
	ModifiedItem::ModifiedItem()
	{
		pForm = NULL;
		pExtraData = NULL;
		isWorn = false;
	}
	TESForm			* pForm;
	BaseExtraList	* pExtraData;
	bool			isWorn;

	operator bool() const
	{
		return (pForm && pExtraData);
	}

	ItemAttributeData * GetAttributeData(TESObjectREFR * reference, bool makeUnique = true, bool allowNewEntry = true, bool allowSelf = false, UInt32 * idOut = NULL);
};

struct ModifiedItemIdentifier
{
	enum
	{
		kTypeNone = 0,
		kTypeRank = (1 << 0),
		kTypeUID = (1 << 1),
		kTypeSlot = (1 << 2),
		kTypeSelf = (1 << 3),
		kTypeDirect = (1 << 4)
	};

	enum
	{
		kHandSlot_Left = 0,
		kHandSlot_Right
	};

	UInt16			type = kTypeNone;
	UInt16			uid = 0;
	UInt32			ownerForm = 0;
	UInt32			weaponSlot = 0;
	UInt32			slotMask = 0;
	UInt32			rankId = 0;
	TESForm			* form = NULL;
	BaseExtraList	* extraData = NULL;

	void SetRankID(UInt32 _rank)
	{
		type |= kTypeRank;
		rankId = _rank;
	}
	void SetSlotMask(UInt32 _slotMask, UInt32 _weaponSlot = 0)
	{
		type |= kTypeSlot;
		slotMask = _slotMask;
		weaponSlot = _weaponSlot;
	}
	void SetUniqueID(UInt16 _uid, UInt32 _ownerForm)
	{
		type |= kTypeUID;
		uid = _uid;
		ownerForm = _ownerForm;
	}
	void SetDirect(TESForm * _baseForm, BaseExtraList * _extraData)
	{
		type |= kTypeDirect;
		form = _baseForm;
		extraData = _extraData;
	}

	bool IsDirect()
	{
		return (type & kTypeDirect) == kTypeDirect;
	}

	bool IsSelf()
	{
		return (type & kTypeSelf) == kTypeSelf;
	}

	void SetSelf()
	{
		type |= kTypeSelf;
	}
};

class ModifiedItemFinder
{
public:
	ModifiedItemFinder(ModifiedItemIdentifier & identifier) : m_identifier(identifier) { }
	bool Accept(InventoryEntryData* pEntryData);
	ModifiedItem& Found() {
		return m_found;
	};
private:
	ModifiedItemIdentifier m_identifier;
	ModifiedItem	m_found;
};

class ItemAttributeData
{
public:
	ItemAttributeData::~ItemAttributeData()
	{
		if (m_tintData) {
			delete m_tintData;
			m_tintData = NULL;
		}
	}

	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion);

	class TintData
	{
	public:
		TintData::~TintData()
		{
			m_colorMap.clear();
		}

		ColorMap m_colorMap;

		void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
		bool Load(SKSESerializationInterface * intfc, UInt32 kVersion);
	};

	TintData	* m_tintData = NULL;
};


static const UInt32 ITEM_ATTRIBUTE_RANK = 0;
static const UInt32 ITEM_ATTRIBUTE_UID = 1;
static const UInt32 ITEM_ATTRIBUTE_OWNERFORM = 2;
static const UInt32 ITEM_ATTRIBUTE_FORMID = 3;
static const UInt32 ITEM_ATTRIBUTE_DATA = 4;

typedef std::tuple<UInt32, UInt16, UInt32, UInt32, ItemAttributeData*> ItemAttribute;

class ItemDataInterface : public SafeDataHolder<std::vector<ItemAttribute>>, public IPluginInterface
{
public:
	typedef std::vector<ItemAttribute> Data;

	enum
	{
		kCurrentPluginVersion = 1,
		kSerializationVersion1 = 1,
		kSerializationVersion = kSerializationVersion1
	};
	virtual UInt32 GetVersion();

	virtual void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	virtual bool Load(SKSESerializationInterface * intfc, UInt32 kVersion);
	virtual void Revert();

	virtual UInt32 GetItemUniqueID(TESObjectREFR * reference, ModifiedItemIdentifier & identifier, bool makeUnique);
	virtual void SetItemDyeColor(UInt32 uniqueID, SInt32 maskIndex, UInt32 color);
	virtual UInt32 GetItemDyeColor(UInt32 uniqueID, SInt32 maskIndex);
	virtual void ClearItemDyeColor(UInt32 uniqueID, SInt32 maskIndex);
	virtual TESForm * GetFormFromUniqueID(UInt32 uniqueID);
	virtual TESForm * GetOwnerOfUniqueID(UInt32 uniqueID);

	ItemAttributeData * GetExistingData(TESObjectREFR * reference, ModifiedItemIdentifier & identifier);
	ItemAttributeData * CreateData(UInt32 rankId, UInt16 uid, UInt32 ownerId, UInt32 formId);
	ItemAttributeData * GetData(UInt32 rankId);
	bool UpdateUIDByRank(UInt32 rankId, UInt16 uid, UInt32 formId);
	bool UpdateUID(UInt16 oldId, UInt32 oldFormId, UInt16 newId, UInt32 newFormId);
	bool EraseByRank(UInt32 rankId);
	bool EraseByUID(UInt32 uid, UInt32 formId);

	enum
	{
		kInvalidRank = 0
	};

	void UseRankID() { m_nextRank++; }
	UInt32 GetNextRankID() const { return m_nextRank; }
	UInt32	m_nextRank = 1;
};

class DyeMap : public SafeDataHolder<std::unordered_map<UInt32, UInt32>>
{
public:
	typedef std::unordered_map<UInt32, UInt32> Data;

	UInt32 GetDyeColor(TESForm * form);
	bool IsValidDye(TESForm * form);
	void RegisterDyeForm(TESForm * form, UInt32 color);
	void UnregisterDyeForm(TESForm * form);
	void Revert();
};

class NIOVTaskUpdateItemDye : public TaskDelegate
{
public:
	NIOVTaskUpdateItemDye::NIOVTaskUpdateItemDye(Actor * actor, ModifiedItemIdentifier & identifier);
	virtual void Run();
	virtual void Dispose() {
		delete this;
	};

private:
	UInt32 m_formId;
	ModifiedItemIdentifier m_identifier;
};