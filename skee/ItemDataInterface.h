#pragma once

struct SKSESerializationInterface;
class Actor;
class ItemAttributeData;
class NIOVTaskUpdateItemDye;

#include "skse64/GameThreads.h"
#include "skse64/GameExtraData.h"

#include "CDXTextureRenderer.h"
#include "IPluginInterface.h"
#include "StringTable.h"

#include <vector>
#include <map>
#include <unordered_map>
#include <memory>


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

	std::shared_ptr<ItemAttributeData> GetAttributeData(TESObjectREFR * reference, bool makeUnique = true, bool allowNewEntry = true, bool allowSelf = false, UInt32 * idOut = NULL);
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
	TESForm			* form = nullptr;
	BaseExtraList	* extraData = nullptr;

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
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);

	typedef std::unordered_map<SInt32, StringTableItem> TextureMap;
	typedef std::unordered_map<SInt32, UInt32> ColorMap;
	typedef std::unordered_map<SInt32, StringTableItem> BlendMap;
	typedef std::unordered_map<SInt32, UInt8> TypeMap;

	class TintData
	{
	public:
		enum OverrideFlags
		{
			kNone		= 0,
			kColor		= (1 << 0),
			kTextureMap = (1 << 1),
			kBlendMap	= (1 << 2),
			kTypeMap	= (1 << 3)
		};

		bool empty() const { return m_textureMap.empty() && m_colorMap.empty() && m_blendMap.empty() && m_typeMap.empty(); }

		TextureMap m_textureMap;
		ColorMap m_colorMap;
		BlendMap m_blendMap;
		TypeMap m_typeMap;

		void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
		bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);
	};

	std::unordered_map<SInt32, TintData> m_tintData;
};


struct ItemAttribute
{
	UInt32 rank;
	UInt16 uid;
	UInt32 ownerForm;
	UInt32 formId;
	std::shared_ptr<ItemAttributeData> data;
};

class ItemDataInterface 
	: public SafeDataHolder<std::vector<ItemAttribute>>
	, public IPluginInterface
	, public BSTEventSink <TESUniqueIDChangeEvent>
	, public IAddonAttachmentInterface
{
public:
	typedef std::vector<ItemAttribute> Data;

	enum
	{
		kCurrentPluginVersion = 1,
		kSerializationVersion1 = 1,
		kSerializationVersion2 = 2,
		kSerializationVersion = kSerializationVersion2
	};
	virtual UInt32 GetVersion();


	virtual	EventResult ReceiveEvent(TESUniqueIDChangeEvent * evn, EventDispatcher<TESUniqueIDChangeEvent> * dispatcher) override;

	virtual void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	virtual bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);
	virtual void Revert();

	virtual UInt32 GetItemUniqueID(TESObjectREFR * reference, ModifiedItemIdentifier & identifier, bool makeUnique);
	virtual void SetItemTextureLayerColor(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, UInt32 color);
	virtual void SetItemTextureLayerType(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, UInt32 type);
	virtual void SetItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, SKEEFixedString blendMode);
	virtual void SetItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, SKEEFixedString texture);

	virtual UInt32 GetItemTextureLayerColor(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex);
	virtual UInt32 GetItemTextureLayerType(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex);
	virtual SKEEFixedString GetItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex);
	virtual SKEEFixedString GetItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex);

	virtual void ClearItemTextureLayerColor(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex);
	virtual void ClearItemTextureLayerType(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex);
	virtual void ClearItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex);
	virtual void ClearItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex);
	virtual void ClearItemTextureLayer(UInt32 uniqueID, SInt32 textureIndex);

	virtual TESForm * GetFormFromUniqueID(UInt32 uniqueID);
	virtual TESForm * GetOwnerOfUniqueID(UInt32 uniqueID);

	std::shared_ptr<ItemAttributeData> GetExistingData(TESObjectREFR * reference, ModifiedItemIdentifier & identifier);
	std::shared_ptr<ItemAttributeData> CreateData(UInt32 rankId, UInt16 uid, UInt32 ownerId, UInt32 formId);
	std::shared_ptr<ItemAttributeData> GetData(UInt32 rankId);
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

private:
	std::vector<NIOVTaskUpdateItemDye*> m_loadQueue;

	// Inherited via IAddonAttachmentInterface
	virtual void OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root) override;
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
	NIOVTaskUpdateItemDye(Actor * actor, ModifiedItemIdentifier & identifier, UInt32 flags, bool forced);
	virtual void Run();
	virtual void Dispose() {
		delete this;
	};

	UInt32 GetActor() const { return m_formId; }
	UInt32 GetSlotMask() const { return m_identifier.slotMask; }

private:
	UInt32 m_formId;
	ModifiedItemIdentifier m_identifier;
	UInt32 m_flags;
	bool m_forced;

	friend class ItemDataInterface;
};
