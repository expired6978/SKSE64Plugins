#pragma once

#include "skse64/GameThreads.h"
#include "skse64/GameExtraData.h"
#include "skse64/NiTypes.h"

#include "CDXTextureRenderer.h"
#include "IPluginInterface.h"
#include "StringTable.h"


#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>

struct SKSESerializationInterface;
class Actor;
class ItemAttributeData;
class NIOVTaskUpdateItemDye;
struct LayerTarget;
class TESObjectARMO;
class TESObjectARMA;
class NiTexture;
typedef NiPointer<NiTexture> NiTexturePtr;

typedef std::function<void(TESObjectARMO *, TESObjectARMA *, const char*, NiTexturePtr, LayerTarget&)> LayerFunctor;


struct ModifiedItem
{
	ModifiedItem()
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



class ModifiedItemFinder
{
public:
	ModifiedItemFinder(IItemDataInterface::Identifier & identifier) : m_identifier(identifier) { }
	bool Accept(InventoryEntryData* pEntryData);
	ModifiedItem& Found() {
		return m_found;
	};
private:
	IItemDataInterface::Identifier m_identifier;
	ModifiedItem	m_found;
};

class ItemAttributeData
{
public:
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);

	using TextureMap = std::map<SInt32, StringTableItem>;
	using ColorMap = std::map<SInt32, UInt32>;
	using BlendMap = std::map<SInt32, StringTableItem>;
	using TypeMap = std::map<SInt32, UInt8>;

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

	void SetLayerColor(SInt32 textureIndex, SInt32 layerIndex, UInt32 color);
	void SetLayerType(SInt32 textureIndex, SInt32 layerIndex, UInt32 type);
	void SetLayerBlendMode(SInt32 textureIndex, SInt32 layerIndex, SKEEFixedString blendMode);
	void SetLayerTexture(SInt32 textureIndex, SInt32 layerIndex, SKEEFixedString texture);

	UInt32 GetLayerColor(SInt32 textureIndex, SInt32 layerIndex);
	UInt32 GetLayerType(SInt32 textureIndex, SInt32 layerIndex);
	SKEEFixedString GetLayerBlendMode(SInt32 textureIndex, SInt32 layerIndex);
	SKEEFixedString GetLayerTexture(SInt32 textureIndex, SInt32 layerIndex); 

	void ClearLayerColor(SInt32 textureIndex, SInt32 layerIndex);
	void ClearLayerType(SInt32 textureIndex, SInt32 layerIndex);
	void ClearLayerBlendMode(SInt32 textureIndex, SInt32 layerIndex);
	void ClearLayerTexture(SInt32 textureIndex, SInt32 layerIndex);
	void ClearLayer(SInt32 textureIndex);

	void SetData(SKEEFixedString key, SKEEFixedString value);
	SKEEFixedString GetData(SKEEFixedString key);
	bool HasData(SKEEFixedString key);
	void ClearData(SKEEFixedString key);

	void ForEachLayer(std::function<bool(SInt32, TintData&)> functor);
	bool GetLayer(SInt32 layerIndex, std::function<void(TintData&)> functor);

private:
	std::mutex m_lock;
	std::unordered_map<SInt32, TintData> m_tintData;
	std::unordered_map<StringTableItem, StringTableItem> m_data;
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
	, public IItemDataInterface
	, public BSTEventSink <TESUniqueIDChangeEvent>
	, public IAddonAttachmentInterface
{
public:
	typedef std::vector<ItemAttribute> Data;

	enum
	{
		kCurrentPluginVersion = 2,
		kSerializationVersion1 = 1,
		kSerializationVersion2 = 2,
		kSerializationVersion = kSerializationVersion2
	};
	virtual UInt32 GetVersion();


	virtual	EventResult ReceiveEvent(TESUniqueIDChangeEvent * evn, EventDispatcher<TESUniqueIDChangeEvent> * dispatcher) override;

	virtual void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	virtual bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);
	virtual void Revert();

	virtual UInt32 GetItemUniqueID(TESObjectREFR * reference, IItemDataInterface::Identifier & identifier, bool makeUnique) override;
	virtual void SetItemTextureLayerColor(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, UInt32 color) override;
	virtual void SetItemTextureLayerType(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, UInt32 type) override;
	virtual void SetItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, const char* blendMode) override { Impl_SetItemTextureLayerBlendMode(uniqueID, textureIndex, layerIndex, blendMode); };
	virtual void SetItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, const char* texture) override { Impl_SetItemTextureLayerTexture(uniqueID, textureIndex, layerIndex, texture); };

	virtual UInt32 GetItemTextureLayerColor(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex) override;
	virtual UInt32 GetItemTextureLayerType(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex) override;
	virtual bool GetItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, IItemDataInterface::StringVisitor& visitor) override;
	virtual bool GetItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, IItemDataInterface::StringVisitor& visitor) override;

	virtual void ClearItemTextureLayerColor(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex) override;
	virtual void ClearItemTextureLayerType(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex) override;
	virtual void ClearItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex) override;
	virtual void ClearItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex) override;
	virtual void ClearItemTextureLayer(UInt32 uniqueID, SInt32 textureIndex) override;

	virtual TESForm * GetFormFromUniqueID(UInt32 uniqueID) override;
	virtual TESForm * GetOwnerOfUniqueID(UInt32 uniqueID) override;

	virtual bool HasItemData(UInt32 uniqueID, const char* key) override;
	virtual bool GetItemData(UInt32 uniqueID, const char* key, IItemDataInterface::StringVisitor& visitor) override;
	virtual void SetItemData(UInt32 uniqueID, const char* key, const char* value) override { Impl_SetItemData(uniqueID, key, value); }
	virtual void ClearItemData(UInt32 uniqueID, const char* key) override { Impl_ClearItemData(uniqueID, key); }

	std::shared_ptr<ItemAttributeData> GetExistingData(TESObjectREFR * reference, IItemDataInterface::Identifier & identifier);
	std::shared_ptr<ItemAttributeData> CreateData(UInt32 rankId, UInt16 uid, UInt32 ownerId, UInt32 formId);
	std::shared_ptr<ItemAttributeData> GetData(UInt32 rankId);
	bool UpdateUIDByRank(UInt32 rankId, UInt16 uid, UInt32 formId);
	bool UpdateUID(UInt16 oldId, UInt32 oldFormId, UInt16 newId, UInt32 newFormId);
	bool EraseByRank(UInt32 rankId);
	bool EraseByUID(UInt32 uid, UInt32 formId);

	void UpdateInventoryItemDye(UInt32 rankId, TESObjectARMO * armor, NiAVObject * rootNode);

	void ForEachItemAttribute(std::function<void(const ItemAttribute&)> functor);

	enum
	{
		kInvalidRank = 0
	};

	void UseRankID() { m_nextRank++; }
	UInt32 GetNextRankID() const { return m_nextRank; }

	void Impl_SetItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, SKEEFixedString blendMode);
	void Impl_SetItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, SKEEFixedString texture);
	SKEEFixedString Impl_GetItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex);
	SKEEFixedString Impl_GetItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex);
	SKEEFixedString Impl_GetItemData(UInt32 uniqueID, SKEEFixedString key);
	void Impl_SetItemData(UInt32 uniqueID, SKEEFixedString key, SKEEFixedString value);
	void Impl_ClearItemData(UInt32 uniqueID, SKEEFixedString key);

private:
	UInt32	m_nextRank = 1;
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
	NIOVTaskUpdateItemDye(Actor * actor, IItemDataInterface::Identifier & identifier, UInt32 flags, bool forced, LayerFunctor layerFunctor = LayerFunctor());
	virtual void Run();
	virtual void Dispose() {
		delete this;
	};

	UInt32 GetActor() const { return m_formId; }
	UInt32 GetSlotMask() const { return m_identifier.slotMask; }
	UInt32 GetRankID() const { return m_identifier.rankId; }

private:
	UInt32 m_formId;
	IItemDataInterface::Identifier m_identifier;
	UInt32 m_flags;
	bool m_forced;
	LayerFunctor m_layerFunctor;

	friend class ItemDataInterface;
};
