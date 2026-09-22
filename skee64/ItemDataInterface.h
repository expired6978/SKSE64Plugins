#pragma once

#include "RE/B/BSFixedString.h"
#include "SafeDataHolder.h"
#include "RE/B/BSTEvent.h"
#include "RE/E/ExtraDataList.h"
#include "RE/B/BSContainer.h"
#include "RE/I/InventoryChanges.h"
#include "RE/N/NiSmartPointer.h"
#include "RE/T/TESForm.h"
#include "RE/T/TESObjectARMA.h"
#include "RE/T/TESObjectARMO.h"
#include "RE/T/TESObjectREFR.h"
#include "SKSE/API.h"
#include "SKSE/Interfaces.h"

#include "CDXTextureRenderer.h"
#include "IPluginInterface.h"
#include "StringTable.h"

#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>
#include <cstdint>
#include <RE/A/Actor.h>
#include <RE/N/NiTexture.h>

class ItemAttributeData;
class NIOVTaskUpdateItemDye;
struct LayerTarget;

using NiTexturePtr = RE::NiPointer<RE::NiTexture>;

using LayerFunctor = std::function<void(RE::TESObjectARMO*, RE::TESObjectARMA*, const char*, RE::NiTexturePtr, LayerTarget&)>;

struct ModifiedItem
{
	ModifiedItem()
	{
		pForm = nullptr;
		pExtraData = nullptr;
		isWorn = false;
	}
	RE::TESForm*			pForm;
	RE::ExtraDataList*		pExtraData;
	bool					isWorn;

	operator bool() const
	{
		return (pForm && pExtraData);
	}

	std::shared_ptr<ItemAttributeData> GetAttributeData(RE::TESObjectREFR* reference, bool makeUnique = true, bool allowNewEntry = true, bool allowSelf = false, std::uint32_t* idOut = nullptr);
};

class ModifiedItemFinder : public RE::InventoryChanges::IItemChangeVisitor
{
public:
	ModifiedItemFinder(IItemDataInterface::Identifier& identifier) : m_identifier(identifier) { }
	virtual ~ModifiedItemFinder() = default;

	virtual RE::BSContainer::ForEachResult Visit(RE::InventoryEntryData* a_entryData);
	

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
	void Save(SKSE::SerializationInterface* intfc, std::uint32_t kVersion);
	bool Load(SKSE::SerializationInterface* intfc, std::uint32_t kVersion, const StringIdMap& stringTable);

	using TextureMap = std::map<std::int32_t, StringTableItem>;
	using ColorMap = std::map<std::int32_t, std::uint32_t>;
	using BlendMap = std::map<std::int32_t, StringTableItem>;
	using TypeMap = std::map<std::int32_t, std::uint8_t>;

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

		void Save(SKSE::SerializationInterface* intfc, std::uint32_t kVersion);
		bool Load(SKSE::SerializationInterface* intfc, std::uint32_t kVersion, const StringIdMap& stringTable);
	};

	void SetLayerColor(std::int32_t textureIndex, std::int32_t layerIndex, std::uint32_t color);
	void SetLayerType(std::int32_t textureIndex, std::int32_t layerIndex, std::uint32_t type);
	void SetLayerBlendMode(std::int32_t textureIndex, std::int32_t layerIndex, SKEEFixedString blendMode);
	void SetLayerTexture(std::int32_t textureIndex, std::int32_t layerIndex, SKEEFixedString texture);

	std::uint32_t GetLayerColor(std::int32_t textureIndex, std::int32_t layerIndex);
	std::uint32_t GetLayerType(std::int32_t textureIndex, std::int32_t layerIndex);
	SKEEFixedString GetLayerBlendMode(std::int32_t textureIndex, std::int32_t layerIndex);
	SKEEFixedString GetLayerTexture(std::int32_t textureIndex, std::int32_t layerIndex); 

	void ClearLayerColor(std::int32_t textureIndex, std::int32_t layerIndex);
	void ClearLayerType(std::int32_t textureIndex, std::int32_t layerIndex);
	void ClearLayerBlendMode(std::int32_t textureIndex, std::int32_t layerIndex);
	void ClearLayerTexture(std::int32_t textureIndex, std::int32_t layerIndex);
	void ClearLayer(std::int32_t textureIndex);

	void SetData(SKEEFixedString key, SKEEFixedString value);
	SKEEFixedString GetData(SKEEFixedString key);
	bool HasData(SKEEFixedString key);
	void ClearData(SKEEFixedString key);

	void ForEachLayer(std::function<bool(std::int32_t, TintData&)> functor);
	bool GetLayer(std::int32_t layerIndex, std::function<void(TintData&)> functor);

private:
	std::mutex m_lock;
	std::unordered_map<std::int32_t, TintData> m_tintData;
	std::unordered_map<StringTableItem, StringTableItem> m_data;
};

struct ItemAttribute
{
	std::uint32_t rank;
	std::uint16_t uid;
	std::uint32_t ownerForm;
	std::uint32_t formId;
	std::shared_ptr<ItemAttributeData> data;
};

class ItemDataInterface
	: public IItemDataInterface
	, public SafeDataHolder<std::vector<ItemAttribute>>
	, public RE::BSTEventSink<RE::TESUniqueIDChangeEvent>
	, public IAddonAttachmentInterface
{
public:
	using Data = std::vector<ItemAttribute>;

	virtual std::uint32_t GetVersion();

	virtual RE::BSEventNotifyControl ProcessEvent(const RE::TESUniqueIDChangeEvent* evn, RE::BSTEventSource<RE::TESUniqueIDChangeEvent>* dispatcher) override;

	void Save(SKSE::SerializationInterface* intfc, std::uint32_t kVersion);
	bool Load(SKSE::SerializationInterface* intfc, std::uint32_t kVersion, const StringIdMap& stringTable);
	virtual void Revert();

	virtual std::uint32_t GetItemUniqueID(RE::TESObjectREFR* reference, IItemDataInterface::Identifier& identifier, bool makeUnique) override;
	virtual void SetItemTextureLayerColor(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex, std::uint32_t color) override;
	virtual void SetItemTextureLayerType(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex, std::uint32_t type) override;
	virtual void SetItemTextureLayerBlendMode(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex, const char* blendMode) override { Impl_SetItemTextureLayerBlendMode(uniqueID, textureIndex, layerIndex, blendMode); };
	virtual void SetItemTextureLayerTexture(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex, const char* texture) override { Impl_SetItemTextureLayerTexture(uniqueID, textureIndex, layerIndex, texture); };

	virtual std::uint32_t GetItemTextureLayerColor(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex) override;
	virtual std::uint32_t GetItemTextureLayerType(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex) override;
	virtual bool GetItemTextureLayerBlendMode(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex, IItemDataInterface::StringVisitor& visitor) override;
	virtual bool GetItemTextureLayerTexture(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex, IItemDataInterface::StringVisitor& visitor) override;

	virtual void ClearItemTextureLayerColor(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex) override;
	virtual void ClearItemTextureLayerType(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex) override;
	virtual void ClearItemTextureLayerBlendMode(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex) override;
	virtual void ClearItemTextureLayerTexture(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex) override;
	virtual void ClearItemTextureLayer(std::uint32_t uniqueID, std::int32_t textureIndex) override;

	virtual RE::TESForm* GetFormFromUniqueID(std::uint32_t uniqueID) override;
	virtual RE::TESForm* GetOwnerOfUniqueID(std::uint32_t uniqueID) override;

	virtual bool HasItemData(std::uint32_t uniqueID, const char* key) override;
	virtual bool GetItemData(std::uint32_t uniqueID, const char* key, IItemDataInterface::StringVisitor& visitor) override;
	virtual void SetItemData(std::uint32_t uniqueID, const char* key, const char* value) override { Impl_SetItemData(uniqueID, key, value); }
	virtual void ClearItemData(std::uint32_t uniqueID, const char* key) override { Impl_ClearItemData(uniqueID, key); }

	std::shared_ptr<ItemAttributeData> GetExistingData(RE::TESObjectREFR* reference, IItemDataInterface::Identifier& identifier);
	std::shared_ptr<ItemAttributeData> CreateData(std::uint32_t rankId, std::uint16_t uid, std::uint32_t ownerId, std::uint32_t formId);
	std::shared_ptr<ItemAttributeData> GetData(std::uint32_t rankId);
	bool UpdateUIDByRank(std::uint32_t rankId, std::uint16_t uid, std::uint32_t formId);
	bool UpdateUID(std::uint16_t oldId, std::uint32_t oldFormId, std::uint16_t newId, std::uint32_t newFormId);
	bool EraseByRank(std::uint32_t rankId);
	bool EraseByUID(std::uint32_t uid, std::uint32_t formId);

	void UpdateInventoryItemDye(std::uint32_t rankId, RE::TESObjectARMO* armor, RE::NiAVObject* rootNode);

	void ForEachItemAttribute(std::function<void(const ItemAttribute&)> functor);

	enum
	{
		kInvalidRank = 0
	};

	void UseRankID() { m_nextRank++; }
	std::uint32_t GetNextRankID() const { return m_nextRank; }

	void Impl_SetItemTextureLayerBlendMode(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex, SKEEFixedString blendMode);
	void Impl_SetItemTextureLayerTexture(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex, SKEEFixedString texture);
	SKEEFixedString Impl_GetItemTextureLayerBlendMode(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex);
	SKEEFixedString Impl_GetItemTextureLayerTexture(std::uint32_t uniqueID, std::int32_t textureIndex, std::int32_t layerIndex);
	SKEEFixedString Impl_GetItemData(std::uint32_t uniqueID, SKEEFixedString key);
	void Impl_SetItemData(std::uint32_t uniqueID, SKEEFixedString key, SKEEFixedString value);
	void Impl_ClearItemData(std::uint32_t uniqueID, SKEEFixedString key);

private:
	std::uint32_t	m_nextRank = 1;
	std::vector<NIOVTaskUpdateItemDye*> m_loadQueue;

	// Inherited via IAddonAttachmentInterface
	virtual void OnAttach(RE::TESObjectREFR* refr, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::NiAVObject* object, bool isFirstPerson, RE::NiNode* skeleton, RE::NiNode* root) override;
};

class DyeMap : public SafeDataHolder<std::unordered_map<std::uint32_t, std::uint32_t>>
{
public:
	using Data = std::unordered_map<std::uint32_t, std::uint32_t>;

	std::uint32_t GetDyeColor(RE::TESForm* form);
	bool IsValidDye(RE::TESForm* form);
	void RegisterDyeForm(RE::TESForm* form, std::uint32_t color);
	void UnregisterDyeForm(RE::TESForm* form);
	void Revert();
};

class NIOVTaskUpdateItemDye : public SKEETaskDelegate
{
public:
	NIOVTaskUpdateItemDye(RE::Actor* actor, IItemDataInterface::Identifier& identifier, std::uint32_t flags, bool forced, LayerFunctor layerFunctor = LayerFunctor());
	virtual void Run() override;
	virtual void Dispose() override {
		delete this;
	};

	std::uint32_t GetActor() const { return m_formId; }
	std::uint32_t GetSlotMask() const { return m_identifier.slotMask; }
	std::uint32_t GetRankID() const { return m_identifier.rankId; }

private:
	std::uint32_t m_formId;
	IItemDataInterface::Identifier m_identifier;
	std::uint32_t m_flags;
	bool m_forced;
	LayerFunctor m_layerFunctor;

	friend class ItemDataInterface;
};
