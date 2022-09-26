#include <algorithm>
#include <unordered_set>
#include <iterator>

#include "skse64/GameRTTI.h"
#include "skse64/GameReferences.h"
#include "skse64/GameObjects.h"
#include "skse64/GameExtraData.h"

#include "skse64/NiNodes.h"

#include "skse64/PluginAPI.h"

#include "skse64/PapyrusEvents.h"

#include "ActorUpdateManager.h"
#include "ItemDataInterface.h"
#include "TintMaskInterface.h"
#include "ShaderUtilities.h"
#include "OverrideInterface.h"
#include "NifUtils.h"
#include "Utilities.h"

extern ActorUpdateManager	g_actorUpdateManager;
extern SKSETaskInterface	* g_task;
extern SKSEMessagingInterface * g_messaging;
extern TintMaskInterface	g_tintMaskInterface;
extern ItemDataInterface	g_itemDataInterface;
extern StringTable			g_stringTable;

UInt32 ItemDataInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

bool ModifiedItemFinder::Accept(InventoryEntryData* pEntryData)
{
	if (!pEntryData)
		return true;

	ExtendDataList* pExtendList = pEntryData->extendDataList;
	if (!pExtendList)
		return true;

	SInt32 n = 0;
	BaseExtraList* pExtraDataList = pExtendList->GetNthItem(n);
	while (pExtraDataList)
	{
		bool isMatch = false;
		bool isWorn = false;

		// Check if the item is worn
		bool hasWorn = pExtraDataList->HasType(kExtraData_Worn);
		bool hasWornLeft = pExtraDataList->HasType(kExtraData_WornLeft);
		if (hasWorn || hasWornLeft)
			isWorn = true;

		if ((m_identifier.type & IItemDataInterface::Identifier::kTypeRank) == IItemDataInterface::Identifier::kTypeRank) {
			if (pExtraDataList->HasType(kExtraData_Rank)) {
				ExtraRank * extraRank = static_cast<ExtraRank*>(pExtraDataList->GetByType(kExtraData_Rank));
				if (m_identifier.rankId == extraRank->rank)
					isMatch = true;
			}
		}
		
		if ((m_identifier.type & IItemDataInterface::Identifier::kTypeUID) == IItemDataInterface::Identifier::kTypeUID) {
			if (pExtraDataList->HasType(kExtraData_UniqueID)) {
				ExtraUniqueID * extraUID = static_cast<ExtraUniqueID*>(pExtraDataList->GetByType(kExtraData_UniqueID));
				if (m_identifier.uid == extraUID->uniqueId && m_identifier.ownerForm == extraUID->ownerFormId)
					isMatch = true;
			}
		}
		
		if ((m_identifier.type & IItemDataInterface::Identifier::kTypeSlot) == IItemDataInterface::Identifier::kTypeSlot) {
			if (isWorn) {
				TESObjectARMO * armor = DYNAMIC_CAST(pEntryData->type, TESForm, TESObjectARMO);

				// It's armor and we don't have a slot mask search
				if (armor && m_identifier.slotMask != 0) {
					if ((armor->bipedObject.GetSlotMask() & m_identifier.slotMask) != 0)
						isMatch = true;
				}
				// Is not an armor, we don't have a slot mask search, and it's equipped
				else if (!armor && m_identifier.slotMask == 0) {
					if ((m_identifier.weaponSlot == IItemDataInterface::Identifier::kHandSlot_Left && hasWornLeft) || (m_identifier.weaponSlot == IItemDataInterface::Identifier::kHandSlot_Right && hasWorn))
						isMatch = true;
				}
			}
		}

		if (isMatch) {
			m_found.pForm = pEntryData->type;
			m_found.pExtraData = pExtraDataList;
			m_found.isWorn = isWorn;
			return false;
		}

		n++;
		pExtraDataList = pExtendList->GetNthItem(n);
	}

	return true;
}

bool ResolveModifiedIdentifier(TESObjectREFR * reference, IItemDataInterface::Identifier & identifier, ModifiedItem & itemData)
{
	if (!reference)
		return false;

	if (identifier.IsDirect()) {
		itemData.pForm = identifier.form;
		itemData.pExtraData = identifier.extraData;
		if (reference->baseForm == identifier.form)
			identifier.SetSelf();
		return (itemData.pForm && itemData.pExtraData);
	}

	if (identifier.IsSelf()) {
		itemData.pForm = reference->baseForm;
		itemData.pExtraData = &reference->extraData;
		return (itemData.pForm && itemData.pExtraData);
	}

	ExtraContainerChanges* pContainerChanges = static_cast<ExtraContainerChanges*>(reference->extraData.GetByType(kExtraData_ContainerChanges));
	if (pContainerChanges) {
		ModifiedItemFinder itemFinder(identifier);
		auto data = pContainerChanges->data;
		if (data) {
			auto objList = data->objList;
			if (objList) {
				objList->Visit(itemFinder);
				itemData = itemFinder.Found();
				return (itemData.pForm && itemData.pExtraData);
			}
		}
	}
	return false;
}

std::shared_ptr<ItemAttributeData> ModifiedItem::GetAttributeData(TESObjectREFR * reference, bool makeUnique, bool allowNewEntry, bool isSelf, UInt32 * idOut)
{
	ExtraRank * rank = NULL;

	if (pExtraData->HasType(kExtraData_Rank)) {
		rank = static_cast<ExtraRank*>(pExtraData->GetByType(kExtraData_Rank));
	} else if (makeUnique) {
		rank = ExtraRank::Create();
		rank->rank = 0;
		pExtraData->Add(kExtraData_Rank, rank);
	}

	ExtraUniqueID * uniqueId = NULL;
	if (pExtraData->HasType(kExtraData_UniqueID)) {
		uniqueId = static_cast<ExtraUniqueID*>(pExtraData->GetByType(kExtraData_UniqueID));
	} else if (makeUnique && !isSelf) {
		ExtraContainerChanges* pContainerChanges = static_cast<ExtraContainerChanges*>(reference->extraData.GetByType(kExtraData_ContainerChanges));
		if (pContainerChanges) {
			auto containerData = pContainerChanges->data;
			if (containerData) {
				CALL_MEMBER_FN(containerData, SetUniqueID)(pExtraData, NULL, pForm);
				uniqueId = static_cast<ExtraUniqueID*>(pExtraData->GetByType(kExtraData_UniqueID));
			}
		}
	}
	else if (!uniqueId && isSelf) {
		uniqueId = ExtraUniqueID::Create();
		uniqueId->ownerFormId = reference->formID;
		uniqueId->uniqueId = 0;
		pExtraData->Add(kExtraData_UniqueID, uniqueId);
	}

	if (rank) {
		auto data = g_itemDataInterface.GetData(rank->rank);
		if (!data && uniqueId && allowNewEntry) {
			rank->rank = g_itemDataInterface.GetNextRankID();
			data = g_itemDataInterface.CreateData(rank->rank, uniqueId ? uniqueId->uniqueId : 0, uniqueId ? uniqueId->ownerFormId : 0, pForm->formID);
			g_itemDataInterface.UseRankID();
		}
		if (idOut) {
			*idOut = rank->rank;
		}
		return data;
	}

	return NULL;
}

std::shared_ptr<ItemAttributeData> ItemDataInterface::GetExistingData(TESObjectREFR * reference, IItemDataInterface::Identifier & identifier)
{
	ModifiedItem foundData;
	if (ResolveModifiedIdentifier(reference, identifier, foundData)) {
		return foundData.GetAttributeData(reference, false, false, identifier.IsSelf());
	}

	return NULL;
}

NIOVTaskUpdateItemDye::NIOVTaskUpdateItemDye(Actor * actor, IItemDataInterface::Identifier & identifier, UInt32 flags, bool forced, LayerFunctor layerFunctor)
{
	m_formId = actor->formID;
	m_identifier = identifier;
	m_flags = flags;
	m_forced = forced;
	m_layerFunctor = layerFunctor;
}

void NIOVTaskUpdateItemDye::Run()
{
	TESForm * form = LookupFormByID(m_formId);
	Actor * actor = DYNAMIC_CAST(form, TESForm, Actor);
	if (actor) {
		ModifiedItem foundData;
		if (ResolveModifiedIdentifier(actor, m_identifier, foundData)) {
			auto data = foundData.GetAttributeData(actor, false, true, m_identifier.IsSelf());
			if (!data && !m_forced) {
				_DMESSAGE("%s - Failed to acquire item attribute data", __FUNCTION__);
				return;
			}

			// Don't bother with visual update if not wearing it
			if (!foundData.isWorn)
				return;

			TESObjectARMO * armor = DYNAMIC_CAST(foundData.pForm, TESForm, TESObjectARMO);
			if (armor) {
				for (UInt32 i = 0; i < armor->armorAddons.count; i++)
				{
					TESObjectARMA * arma = NULL;
					if (armor->armorAddons.GetNthItem(i, arma)) {
						VisitArmorAddon(actor, armor, arma, [&](bool isFirstPerson, NiAVObject * rootNode, NiAVObject * parent)
						{
							g_tintMaskInterface.ApplyMasks(actor, isFirstPerson, armor, arma, parent, m_flags, data, m_layerFunctor);
						});
					}
				}
			}
		}
	}
}

UInt32 ItemDataInterface::GetItemUniqueID(TESObjectREFR * reference, IItemDataInterface::Identifier & identifier, bool makeUnique)
{
	ModifiedItem foundData;
	if (ResolveModifiedIdentifier(reference, identifier, foundData)) {
		UInt32 id = 0;
		auto data = foundData.GetAttributeData(reference, makeUnique, true, identifier.IsSelf(), &id);
		if (data) {
			return id;
		}
	}

	return 0;
}

void ItemDataInterface::SetItemTextureLayerColor(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, UInt32 color)
{
	auto data = GetData(uniqueID);
	if (data) {
		data->SetLayerColor(textureIndex, layerIndex, color);
	}
}

void ItemDataInterface::Impl_SetItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, SKEEFixedString blendMode)
{
	auto data = GetData(uniqueID);
	if (data) {
		data->SetLayerBlendMode(textureIndex, layerIndex, blendMode);
	}
}

void ItemDataInterface::SetItemTextureLayerType(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, UInt32 type)
{
	auto data = GetData(uniqueID);
	if (data) {
		data->SetLayerType(textureIndex, layerIndex, type);
	}
}

void ItemDataInterface::Impl_SetItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, SKEEFixedString texture)
{
	auto data = GetData(uniqueID);
	if (data) {
		data->SetLayerTexture(textureIndex, layerIndex, texture);
	}
}

UInt32 ItemDataInterface::GetItemTextureLayerColor(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data)
	{
		return data->GetLayerColor(textureIndex, layerIndex);
	}

	return 0;
}

SKEEFixedString ItemDataInterface::Impl_GetItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data)
	{
		return data->GetLayerBlendMode(textureIndex, layerIndex);
	}

	return "";
}

UInt32 ItemDataInterface::GetItemTextureLayerType(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data)
	{
		return data->GetLayerType(textureIndex, layerIndex);
	}

	return -1;
}

bool ItemDataInterface::GetItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, IItemDataInterface::StringVisitor& visitor)
{
	auto data = GetData(uniqueID);
	if (data)
	{
		visitor.Visit(data->GetLayerBlendMode(textureIndex, layerIndex).c_str());
		return true;
	}
	return false;
}

bool ItemDataInterface::GetItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, IItemDataInterface::StringVisitor& visitor)
{
	auto data = GetData(uniqueID);
	if (data)
	{
		visitor.Visit(data->GetLayerTexture(textureIndex, layerIndex).c_str());
		return true;
	}
	return false;
}

SKEEFixedString ItemDataInterface::Impl_GetItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data)
	{
		return data->GetLayerTexture(textureIndex, layerIndex);
	}

	return "";
}

void ItemDataInterface::ClearItemTextureLayerColor(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data) {
		data->ClearLayerColor(textureIndex, layerIndex);
	}
}

void ItemDataInterface::ClearItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data) {
		data->ClearLayerBlendMode(textureIndex, layerIndex);
	}
}

void ItemDataInterface::ClearItemTextureLayerType(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data) {
		data->ClearLayerType(textureIndex, layerIndex);
	}
}

void ItemDataInterface::ClearItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data) {
		data->ClearLayerTexture(textureIndex, layerIndex);
	}
}

void ItemDataInterface::ClearItemTextureLayer(UInt32 uniqueID, SInt32 textureIndex)
{
	auto data = GetData(uniqueID);
	if (data) {
		data->ClearLayer(textureIndex);
	}
}

std::shared_ptr<ItemAttributeData> ItemDataInterface::GetData(UInt32 rankId)
{
	SimpleLocker lock(&m_lock);
	if (rankId == kInvalidRank)
		return nullptr;

	auto it = std::find_if(m_data.begin(), m_data.end(), [&](ItemAttribute& item)
	{
		if (item.rank == rankId)
			return true;
		return false;
	});

	if (it != m_data.end()) {
		return (*it).data;
	}

	return nullptr;
}

TESForm * ItemDataInterface::GetFormFromUniqueID(UInt32 uniqueID)
{
	SimpleLocker lock(&m_lock);
	if (uniqueID == kInvalidRank)
		return nullptr;

	auto it = std::find_if(m_data.begin(), m_data.end(), [&](ItemAttribute& item)
	{
		if (item.rank == uniqueID)
			return true;
		return false;
	});

	if (it != m_data.end()) {
		return LookupFormByID((*it).formId);
	}

	return nullptr;
}

TESForm * ItemDataInterface::GetOwnerOfUniqueID(UInt32 uniqueID)
{
	SimpleLocker lock(&m_lock);
	if (uniqueID == kInvalidRank)
		return NULL;

	auto it = std::find_if(m_data.begin(), m_data.end(), [&](ItemAttribute& item)
	{
		if (item.rank == uniqueID)
			return true;
		return false;
	});

	if (it != m_data.end()) {
		return LookupFormByID((*it).ownerForm);
	}

	return nullptr;
}

bool ItemDataInterface::HasItemData(UInt32 uniqueID, const char* key)
{
	auto data = GetData(uniqueID);
	if (data)
	{
		return data->HasData(key);
	}
	return false;
}

bool ItemDataInterface::GetItemData(UInt32 uniqueID, const char* key, IItemDataInterface::StringVisitor& visitor)
{
	auto data = GetData(uniqueID);
	if (data)
	{
		visitor.Visit(data->GetData(key).c_str());
		return true;
	}
	return false;
}

SKEEFixedString ItemDataInterface::Impl_GetItemData(UInt32 uniqueID, SKEEFixedString key)
{
	auto data = GetData(uniqueID);
	if (data)
	{
		return data->GetData(key);
	}
	return "";
}

void ItemDataInterface::Impl_SetItemData(UInt32 uniqueID, SKEEFixedString key, SKEEFixedString value)
{
	auto data = GetData(uniqueID);
	if (data)
	{
		data->SetData(key, value);
	}
}

void ItemDataInterface::Impl_ClearItemData(UInt32 uniqueID, SKEEFixedString key)
{
	auto data = GetData(uniqueID);
	if (data)
	{
		data->ClearData(key);
	}
}

std::shared_ptr<ItemAttributeData> ItemDataInterface::CreateData(UInt32 rankId, UInt16 uid, UInt32 ownerId, UInt32 formId)
{
	EraseByRank(rankId);
	
	std::shared_ptr<ItemAttributeData> data = std::make_shared<ItemAttributeData>();
	Lock();
	m_data.push_back({ rankId, uid, ownerId, formId, data });
	Release();
	return data;
}

bool ItemDataInterface::UpdateUIDByRank(UInt32 rankId, UInt16 uid, UInt32 formId)
{
	SimpleLocker lock(&m_lock);

	auto it = std::find_if(m_data.begin(), m_data.end(), [&](ItemAttribute& item)
	{
		if (item.rank == rankId) {
			item.uid = uid;
			item.ownerForm = formId;
			return true;
		}

		return false;
	});

	return it != m_data.end();
}

bool ItemDataInterface::UpdateUID(UInt16 oldId, UInt32 oldFormId, UInt16 newId, UInt32 newFormId)
{
	SimpleLocker lock(&m_lock);

	auto it = std::find_if(m_data.begin(), m_data.end(), [&](ItemAttribute& item)
	{
		if (item.uid == oldId && item.ownerForm == oldFormId) {
			item.uid = newId;
			item.ownerForm = newFormId;
			return true;
		}

		return false;
	});

	return it != m_data.end();
}

bool ItemDataInterface::EraseByRank(UInt32 rankId)
{
	SimpleLocker lock(&m_lock);

	auto it = std::find_if(m_data.begin(), m_data.end(), [&](ItemAttribute& item)
	{
		if (item.rank == rankId) {
			return true;
		}

		return false;
	});

	if (it != m_data.end()) {
		m_data.erase(it);
		return true;
	}

	return false;
}

bool ItemDataInterface::EraseByUID(UInt32 uid, UInt32 formId)
{
	SimpleLocker lock(&m_lock);

	auto it = std::find_if(m_data.begin(), m_data.end(), [&](ItemAttribute& item)
	{
		if (item.uid == uid && item.ownerForm == formId) {
			return true;
		}

		return false;
	});

	if (it != m_data.end()) {
		auto eventDispatcher = static_cast<EventDispatcher<SKSEModCallbackEvent>*>(g_messaging->GetEventDispatcher(SKSEMessagingInterface::kDispatcher_ModEvent));
		if (eventDispatcher) {
			TESForm * form = LookupFormByID((*it).formId);
			SKSEModCallbackEvent evn("NiOverride_Internal_EraseUID", "", (*it).uid, form);
			eventDispatcher->SendEvent(&evn);
		}

		m_data.erase(it);
		return true;
	}

	return false;
}

void ItemDataInterface::UpdateInventoryItemDye(UInt32 rankId, TESObjectARMO * armor, NiAVObject * rootNode)
{
	g_task->AddTask(new NIOVTaskDeferredMask((*g_thePlayer), false, armor, nullptr, rootNode, rankId ? g_itemDataInterface.GetData(rankId) : nullptr));
}

void ItemDataInterface::ForEachItemAttribute(std::function<void(const ItemAttribute&)> functor)
{
	SimpleLocker lock(&m_lock);
	for (auto& attribute : m_data)
	{
		functor(attribute);
	}
}

void ItemDataInterface::OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root)
{
	UInt32 armorMask = armor->bipedObject.GetSlotMask();
	Actor * actor = DYNAMIC_CAST(refr, TESForm, Actor);
	if (actor) {
		IItemDataInterface::Identifier identifier;
		identifier.SetSlotMask(armorMask);
		g_task->AddTask(new NIOVTaskDeferredMask(refr, isFirstPerson, armor, addon, object, g_itemDataInterface.GetExistingData(actor, identifier)));
	}
}

void ItemDataInterface::Revert()
{
	SimpleLocker lock(&m_lock);
	std::vector<ItemAttribute> copy = m_data;
	m_data.clear();

	// We need to trigger a dye update after wiping data to reset
	for (auto & itemAttribute : copy)
	{
		TESForm * ownerForm = LookupFormByID(itemAttribute.ownerForm);
		TESForm * itemForm = LookupFormByID(itemAttribute.formId);

		Actor * actor = DYNAMIC_CAST(ownerForm, TESForm, Actor);
		TESObjectARMO * armor = DYNAMIC_CAST(itemForm, TESForm, TESObjectARMO);
		if (actor && armor) {
			IItemDataInterface::Identifier identifier;
			identifier.SetRankID(itemAttribute.rank);
			identifier.SetUniqueID(itemAttribute.uid, itemAttribute.ownerForm);
			identifier.SetSlotMask(armor->bipedObject.GetSlotMask());
			m_loadQueue.push_back(new NIOVTaskUpdateItemDye(actor, identifier, TintMaskInterface::kUpdate_All, true));
		}
	}
	m_nextRank = 1;
}

void DyeMap::Revert()
{
	SimpleLocker lock(&m_lock);
	m_data.clear();
}

bool DyeMap::IsValidDye(TESForm * form)
{
	SimpleLocker lock(&m_lock);

	auto it = m_data.find(form->formID);
	if (it != m_data.end()) {
		return true;
	}

	return false;
}

UInt32 DyeMap::GetDyeColor(TESForm * form)
{
	SimpleLocker lock(&m_lock);

	auto it = m_data.find(form->formID);
	if (it != m_data.end()) {
		return it->second;
	}

	return 0;
}

void DyeMap::RegisterDyeForm(TESForm * form, UInt32 color)
{
	SimpleLocker lock(&m_lock);
	m_data[form->formID] = color;
}

void DyeMap::UnregisterDyeForm(TESForm * form)
{
	SimpleLocker lock(&m_lock);
	auto it = m_data.find(form->formID);
	if (it != m_data.end())
		m_data.erase(it);
}

void ItemAttributeData::TintData::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	struct DataMap
	{
		UInt32* color = nullptr;
		StringTableItem* texture = nullptr;
		StringTableItem* blendMode = nullptr;
		UInt8* type = nullptr;
	};

	std::unordered_map<SInt32, DataMap> layerMap;
	for (auto & element : m_colorMap)
	{
		layerMap[element.first].color = &element.second;
	}
	for (auto & element : m_textureMap)
	{
		layerMap[element.first].texture = &element.second;
	}
	for (auto & element : m_blendMap)
	{
		layerMap[element.first].blendMode = &element.second;
	}
	for (auto & element : m_typeMap)
	{
		layerMap[element.first].type = &element.second;
	}

	UInt32 numTints = layerMap.size();
	intfc->WriteRecordData(&numTints, sizeof(numTints));

	for (auto tint : layerMap)
	{
		SInt32 maskIndex = tint.first;
		auto & layerData = tint.second;

		intfc->WriteRecordData(&maskIndex, sizeof(maskIndex));
		UInt8 overrideFlags = 0;
		if (layerData.color) overrideFlags |= OverrideFlags::kColor;
		if (layerData.texture) overrideFlags |= OverrideFlags::kTextureMap;
		if (layerData.blendMode) overrideFlags |= OverrideFlags::kBlendMap;
		if (layerData.type) overrideFlags |= OverrideFlags::kTypeMap;

		intfc->WriteRecordData(&overrideFlags, sizeof(overrideFlags));

		if (layerData.color) {
			intfc->WriteRecordData(layerData.color, sizeof(*layerData.color));
		}
		if (layerData.texture) {
			g_stringTable.WriteString(intfc, *layerData.texture);
		}
		if (layerData.blendMode) {
			intfc->WriteRecordData(layerData.blendMode, sizeof(*layerData.blendMode));
		}
		if (layerData.type) {
			intfc->WriteRecordData(layerData.type, sizeof(*layerData.type));
		}
	}
}

bool ItemAttributeData::TintData::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	bool error = false;

	UInt32 tintCount;
	if (!intfc->ReadRecordData(&tintCount, sizeof(tintCount)))
	{
		_ERROR("%s - Error loading tint count", __FUNCTION__);
		error = true;
		return error;
	}

	for (UInt32 i = 0; i < tintCount; i++)
	{
		SInt32 maskIndex = 0;
		if (!intfc->ReadRecordData(&maskIndex, sizeof(maskIndex)))
		{
			_ERROR("%s - Error loading mask index", __FUNCTION__);
			error = true;
			return error;
		}

		UInt8 overrideFlags = 0;
		if (kVersion >= ItemDataInterface::kSerializationVersion2)
		{
			if (!intfc->ReadRecordData(&overrideFlags, sizeof(overrideFlags)))
			{
				_ERROR("%s - Error loading override flags", __FUNCTION__);
				error = true;
				return error;
			}
		}

		if (kVersion == ItemDataInterface::kSerializationVersion1)
		{
			UInt32 maskColor = 0;
			if (!intfc->ReadRecordData(&maskColor, sizeof(maskColor)))
			{
				_ERROR("%s - Error loading mask color", __FUNCTION__);
				error = true;
				return error;
			}
		}

		if (kVersion >= ItemDataInterface::kSerializationVersion2)
		{
			if (overrideFlags & OverrideFlags::kColor)
			{
				UInt32 maskColor = 0;
				if (!intfc->ReadRecordData(&maskColor, sizeof(maskColor)))
				{
					_ERROR("%s - Error loading mask color", __FUNCTION__);
					error = true;
					return error;
				}

				m_colorMap[maskIndex] = maskColor;
			}
			if (overrideFlags & OverrideFlags::kTextureMap)
			{
				m_textureMap[maskIndex] = StringTable::ReadString(intfc, stringTable);
			}
			if (overrideFlags & OverrideFlags::kBlendMap)
			{
				m_blendMap[maskIndex] = StringTable::ReadString(intfc, stringTable);
			}
			if (overrideFlags & OverrideFlags::kTypeMap)
			{
				UInt8 textureType = 0;
				if (!intfc->ReadRecordData(&textureType, sizeof(textureType)))
				{
					_ERROR("%s - Error loading blend mode", __FUNCTION__);
					error = true;
					return error;
				}
				m_typeMap[maskIndex] = textureType;
			}
		}
	}

	return error;
}

void ItemAttributeData::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	utils::scoped_lock<> locker(m_lock);
	UInt32 numSubrecords = m_tintData.size() + m_data.size();

	intfc->WriteRecordData(&numSubrecords, sizeof(numSubrecords));

	for (auto& layer : m_tintData)
	{
		intfc->OpenRecord('TINT', kVersion);

		SInt32 layerIndex = layer.first;
		intfc->WriteRecordData(&layerIndex, sizeof(layerIndex));

		layer.second.Save(intfc, kVersion);
	}
	for (auto& kvp : m_data)
	{
		intfc->OpenRecord('IKVP', kVersion);
		g_stringTable.WriteString(intfc, kvp.first);
		g_stringTable.WriteString(intfc, kvp.second);
	}
}

bool ItemAttributeData::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	UInt32 type, length, version;
	bool error = false;

	// Handle count
	UInt32 numSubrecords = 0;
	if (!intfc->ReadRecordData(&numSubrecords, sizeof(numSubrecords)))
	{
		_ERROR("%s - Error loading number of attribute sub records", __FUNCTION__);
		error = true;
		return error;
	}

	for (UInt32 i = 0; i < numSubrecords; i++)
	{
		if (intfc->GetNextRecordInfo(&type, &version, &length))
		{
			switch (type)
			{
				case 'TINT':
					{
						SInt32 layerIndex = 0;
						if (version >= ItemDataInterface::kSerializationVersion2)
						{
							if (!intfc->ReadRecordData(&layerIndex, sizeof(layerIndex)))
							{
								_ERROR("%s - Error loading layer count", __FUNCTION__);
								continue;
							}
						}

						utils::scoped_lock<> locker(m_lock);
						if (m_tintData[layerIndex].Load(intfc, version, stringTable))
						{
							error = true;
							return error;
						}
					}
					break;
				case 'IKVP':
				{
					auto key = StringTable::ReadString(intfc, stringTable);
					auto value = StringTable::ReadString(intfc, stringTable);
					if (key && value)
					{
						utils::scoped_lock<> locker(m_lock);
						m_data.emplace(key, value);
					}
					break;
				}
				default:
					_ERROR("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, &type);
					error = true;
					break;
			}
		}
	}

	return error;
}

EventResult ItemDataInterface::ReceiveEvent(TESUniqueIDChangeEvent * evn, EventDispatcher<TESUniqueIDChangeEvent>* dispatcher)
{
	if (evn->oldOwnerFormId != 0) {
		g_itemDataInterface.UpdateUID(evn->oldUniqueId, evn->oldOwnerFormId, evn->newUniqueId, evn->newOwnerFormId);
	}
	if (evn->newOwnerFormId == 0) {
		g_itemDataInterface.EraseByUID(evn->oldUniqueId, evn->oldOwnerFormId);
	}
	return EventResult::kEvent_Continue;
}

void ItemDataInterface::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	intfc->OpenRecord('ITEE', kVersion);

	UInt32 nextRank = m_nextRank;
	intfc->WriteRecordData(&nextRank, sizeof(nextRank));

	UInt32 numItems = m_data.size();
	intfc->WriteRecordData(&numItems, sizeof(numItems));

	for (auto & attribute : m_data) {
		intfc->OpenRecord('IDAT', kVersion);

		UInt32 rankId = attribute.rank;
		UInt16 uid = attribute.uid;

		UInt32 ownerFormId = attribute.ownerForm;
		UInt32 itemFormId = attribute.formId;
		
		intfc->WriteRecordData(&rankId, sizeof(rankId));
		intfc->WriteRecordData(&uid, sizeof(uid));
		intfc->WriteRecordData(&ownerFormId, sizeof(ownerFormId));
		intfc->WriteRecordData(&itemFormId, sizeof(itemFormId));

		const std::shared_ptr<ItemAttributeData> & data = attribute.data;
		if (data) {
			data->Save(intfc, kVersion);
		}
	}
}

bool ItemDataInterface::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	UInt32 type, length, version;
	bool error = false;

	// Resolve formIds from any prior queues (if there are entries here its because Revert queued them)
	for (auto & task : m_loadQueue)
	{
		// If the task was queued from a previous save we need to resolve the actor's formId first
		UInt32 newItemForm = 0;
		if (!ResolveAnyForm(intfc, task->m_formId, &newItemForm)) {
			_WARNING("%s - actor %08X could not be found, skipping dye update", __FUNCTION__, task->m_formId);
			continue;
		}
		task->m_formId = newItemForm;
	}

	UInt32 nextRank;
	if (!intfc->ReadRecordData(&nextRank, sizeof(nextRank)))
	{
		_ERROR("%s - Error loading next rank ID", __FUNCTION__);
		error = true;
		return error;
	}

	m_nextRank = nextRank;

	UInt32 numItems;
	if (!intfc->ReadRecordData(&numItems, sizeof(numItems)))
	{
		_ERROR("%s - Error loading number of items", __FUNCTION__);
		error = true;
		return error;
	}


	for (UInt32 i = 0; i < numItems; i++)
	{
		if (intfc->GetNextRecordInfo(&type, &version, &length))
		{
			switch (type)
			{
				case 'IDAT':
				{
					UInt32 rankId;
					if (!intfc->ReadRecordData(&rankId, sizeof(rankId)))
					{
						_ERROR("%s - Error loading Rank ID", __FUNCTION__);
						error = true;
						return error;
					}

					UInt16 uid;
					if (!intfc->ReadRecordData(&uid, sizeof(uid)))
					{
						_ERROR("%s - Error loading Unique ID", __FUNCTION__);
						error = true;
						return error;
					}
					
					UInt32 ownerForm = 0, itemForm = 0;
					UInt64 ownerHandle = 0, itemHandle = 0, newOwnerHandle = 0, newItemHandle = 0;
					if (version >= kSerializationVersion2)
					{
						if (!intfc->ReadRecordData(&ownerForm, sizeof(ownerForm)))
						{
							_ERROR("%s - Error loading owner form", __FUNCTION__);
							error = true;
							return error;
						}
						if (!intfc->ReadRecordData(&itemForm, sizeof(itemForm)))
						{
							_ERROR("%s - Error loading item form", __FUNCTION__);
							error = true;
							return error;
						}

						UInt32 newOwnerForm = 0;
						if (!ResolveAnyForm(intfc, ownerForm, &newOwnerForm)) {
							_WARNING("%s - owner %08X could not be found, skipping", __FUNCTION__, ownerForm);
							continue;
						}
						newOwnerHandle = newOwnerForm;

						UInt32 newItemForm = 0;
						if (!ResolveAnyForm(intfc, itemForm, &newItemForm)) {
							_WARNING("%s - item %08X could not be found, skipping", __FUNCTION__, itemForm);
							continue;
						}
						newItemHandle = newItemForm;
					}
					else if (version >= kSerializationVersion1)
					{
						if (!intfc->ReadRecordData(&ownerHandle, sizeof(ownerHandle)))
						{
							_ERROR("%s - Error loading owner handle", __FUNCTION__);
							error = true;
							return error;
						}
						if (!intfc->ReadRecordData(&itemHandle, sizeof(itemHandle)))
						{
							_ERROR("%s - Error loading item handle", __FUNCTION__);
							error = true;
							return error;
						}
						if (!ResolveAnyHandle(intfc, ownerHandle, &newOwnerHandle)) {
							_WARNING("%s - owner handle %016llX could not be found, skipping", __FUNCTION__, ownerHandle);
							continue;
						}
						if (!intfc->ResolveHandle(itemHandle, &newItemHandle)) {
							_WARNING("%s - item handle %016llX could not be found, skipping", __FUNCTION__, itemHandle);
							continue;
						}
					}
					

					std::shared_ptr<ItemAttributeData> data = std::make_shared<ItemAttributeData>();
					if (data->Load(intfc, version, stringTable)) {
						_ERROR("%s - Failed to load item data for owner %016llX item %016llX", __FUNCTION__, ownerHandle, itemHandle);
						error = true;
						return error;
					}

					if (rankId != kInvalidRank) {
						UInt32 ownerFormId = newOwnerHandle & 0xFFFFFFFF;
						UInt32 itemFormId = newItemHandle & 0xFFFFFFFF;

						Lock();
						m_data.push_back({rankId, uid, ownerFormId, itemFormId, data});
						Release();

						TESForm* ownerForm = LookupFormByID(ownerFormId);
						Actor* actor = DYNAMIC_CAST(ownerForm, TESForm, Actor);
						TESForm * itemForm = LookupFormByID(itemFormId);
						if (actor)
						{
							IItemDataInterface::Identifier identifier;
							identifier.SetRankID(rankId);
							identifier.SetUniqueID(uid, ownerFormId);

							TESObjectARMO* armor = DYNAMIC_CAST(itemForm, TESForm, TESObjectARMO);
							if (armor) {
								identifier.SetSlotMask(armor->bipedObject.GetSlotMask());
							}

							m_loadQueue.push_back(new NIOVTaskUpdateItemDye(actor, identifier, TintMaskInterface::kUpdate_All, false));
						}
					}
				}
				break;
				default:
					_ERROR("Error loading unexpected chunk type %08X (%.4s)", type, &type);
					error = true;
					break;
			}
		}
	}

	// Sort task list
	std::sort(m_loadQueue.begin(), m_loadQueue.end(), [](NIOVTaskUpdateItemDye * a, NIOVTaskUpdateItemDye * b)
	{
		if (a->GetActor() == b->GetActor())
		{
			return a->GetRankID() < b->GetRankID();
		}
		return a->GetActor() < b->GetActor();
	});

	std::unordered_set<NIOVTaskUpdateItemDye*> uniqueTasks;
	std::unique_copy(m_loadQueue.begin(), m_loadQueue.end(), std::inserter(uniqueTasks, uniqueTasks.end()), [](NIOVTaskUpdateItemDye * a, NIOVTaskUpdateItemDye * b)
	{
		return a->GetActor() == b->GetActor() && a->GetRankID() == b->GetRankID();
	});

	// Push the loads onto the task queue
	for (auto & task : m_loadQueue)
	{
		auto foundTask = uniqueTasks.find(task);
		if (foundTask != uniqueTasks.end())
		{
			g_actorUpdateManager.AddDyeUpdate_Internal(task);
		}
		else
		{
			task->Dispose();
		}
	}

	m_loadQueue.clear();
	return error;
}

void ItemAttributeData::SetLayerColor(SInt32 textureIndex, SInt32 layerIndex, UInt32 color)
{
	utils::scoped_lock<> locker(m_lock);
	m_tintData[textureIndex].m_colorMap[layerIndex] = color;
}

void ItemAttributeData::SetLayerType(SInt32 textureIndex, SInt32 layerIndex, UInt32 type)
{
	utils::scoped_lock<> locker(m_lock);
	m_tintData[textureIndex].m_typeMap[layerIndex] = static_cast<UInt8>(type);
}

void ItemAttributeData::SetLayerBlendMode(SInt32 textureIndex, SInt32 layerIndex, SKEEFixedString blendMode)
{
	utils::scoped_lock<> locker(m_lock);
	m_tintData[textureIndex].m_blendMap[layerIndex] = g_stringTable.GetString(blendMode);
}

void ItemAttributeData::SetLayerTexture(SInt32 textureIndex, SInt32 layerIndex, SKEEFixedString texture)
{
	utils::scoped_lock<> locker(m_lock);
	m_tintData[textureIndex].m_textureMap[layerIndex] = g_stringTable.GetString(texture);
}

UInt32 ItemAttributeData::GetLayerColor(SInt32 textureIndex, SInt32 layerIndex)
{
	utils::scoped_lock<> locker(m_lock);
	auto layerData = m_tintData.find(textureIndex);
	if (layerData != m_tintData.end())
	{
		auto it = layerData->second.m_colorMap.find(layerIndex);
		if (it != layerData->second.m_colorMap.end()) {
			return it->second;
		}
	}
	return 0;
}

UInt32 ItemAttributeData::GetLayerType(SInt32 textureIndex, SInt32 layerIndex)
{
	utils::scoped_lock<> locker(m_lock);
	auto layerData = m_tintData.find(textureIndex);
	if (layerData != m_tintData.end())
	{
		auto it = layerData->second.m_typeMap.find(layerIndex);
		if (it != layerData->second.m_typeMap.end()) {
			return it->second;
		}
	}
	return -1;
}

SKEEFixedString ItemAttributeData::GetLayerBlendMode(SInt32 textureIndex, SInt32 layerIndex)
{
	utils::scoped_lock<> locker(m_lock);
	auto layerData = m_tintData.find(textureIndex);
	if (layerData != m_tintData.end())
	{
		auto it = layerData->second.m_blendMap.find(layerIndex);
		if (it != layerData->second.m_blendMap.end()) {
			return it->second ? *it->second : SKEEFixedString("");
		}
	}
	return SKEEFixedString("");
}

SKEEFixedString ItemAttributeData::GetLayerTexture(SInt32 textureIndex, SInt32 layerIndex)
{
	utils::scoped_lock<> locker(m_lock);
	auto layerData = m_tintData.find(textureIndex);
	if (layerData != m_tintData.end())
	{
		auto it = layerData->second.m_textureMap.find(layerIndex);
		if (it != layerData->second.m_textureMap.end()) {
			return it->second ? *it->second : SKEEFixedString("");
		}
	}
	return SKEEFixedString("");
}

void ItemAttributeData::ClearLayerColor(SInt32 textureIndex, SInt32 layerIndex)
{
	utils::scoped_lock<> locker(m_lock);
	auto layerData = m_tintData.find(textureIndex);
	if (layerData != m_tintData.end())
	{
		auto it = layerData->second.m_colorMap.find(layerIndex);
		if (it != layerData->second.m_colorMap.end()) {
			layerData->second.m_colorMap.erase(it);
		}

		// The whole layer is now empty as a result, erase the parent
		if (layerData->second.empty()) {
			m_tintData.erase(layerData);
		}
	}
}

void ItemAttributeData::ClearLayerType(SInt32 textureIndex, SInt32 layerIndex)
{
	utils::scoped_lock<> locker(m_lock);
	auto layerData = m_tintData.find(textureIndex);
	if (layerData != m_tintData.end())
	{
		auto it = layerData->second.m_typeMap.find(layerIndex);
		if (it != layerData->second.m_typeMap.end()) {
			layerData->second.m_typeMap.erase(it);
		}

		// The whole layer is now empty as a result, erase the parent
		if (layerData->second.empty()) {
			m_tintData.erase(layerData);
		}
	}
}

void ItemAttributeData::ClearLayerBlendMode(SInt32 textureIndex, SInt32 layerIndex)
{
	utils::scoped_lock<> locker(m_lock);
	auto layerData = m_tintData.find(textureIndex);
	if (layerData != m_tintData.end())
	{
		auto it = layerData->second.m_blendMap.find(layerIndex);
		if (it != layerData->second.m_blendMap.end()) {
			layerData->second.m_blendMap.erase(it);
		}

		// The whole layer is now empty as a result, erase the parent
		if (layerData->second.empty()) {
			m_tintData.erase(layerData);
		}
	}
}

void ItemAttributeData::ClearLayerTexture(SInt32 textureIndex, SInt32 layerIndex)
{
	utils::scoped_lock<> locker(m_lock);
	auto layerData = m_tintData.find(textureIndex);
	if (layerData != m_tintData.end())
	{
		auto it = layerData->second.m_textureMap.find(layerIndex);
		if (it != layerData->second.m_textureMap.end()) {
			layerData->second.m_textureMap.erase(it);
		}

		// The whole layer is now empty as a result, erase the parent
		if (layerData->second.empty()) {
			m_tintData.erase(layerData);
		}
	}
}

void ItemAttributeData::ClearLayer(SInt32 textureIndex)
{
	utils::scoped_lock<> locker(m_lock);
	auto layerData = m_tintData.find(textureIndex);
	if (layerData != m_tintData.end()) {
		m_tintData.erase(layerData);
	}
}

void ItemAttributeData::SetData(SKEEFixedString key, SKEEFixedString value)
{
	utils::scoped_lock<> locker(m_lock);
	m_data[g_stringTable.GetString(key)] = g_stringTable.GetString(value);
}

SKEEFixedString ItemAttributeData::GetData(SKEEFixedString key)
{
	utils::scoped_lock<> locker(m_lock);
	auto it = m_data.find(g_stringTable.GetString(key));
	if (it != m_data.end())
	{
		return *it->second;
	}
	return "";
}

bool ItemAttributeData::HasData(SKEEFixedString key)
{
	utils::scoped_lock<> locker(m_lock);
	auto it = m_data.find(g_stringTable.GetString(key));
	if (it != m_data.end())
	{
		return true;
	}
	return false;
}

void ItemAttributeData::ClearData(SKEEFixedString key)
{
	utils::scoped_lock<> locker(m_lock);
	auto it = m_data.find(g_stringTable.GetString(key));
	if (it != m_data.end())
	{
		m_data.erase(it);
	}
}

void ItemAttributeData::ForEachLayer(std::function<bool(SInt32, TintData&)> functor)
{
	utils::scoped_lock<> locker(m_lock);
	for (auto it : m_tintData)
	{
		if (functor(it.first, it.second))
		{
			break;
		}
	}
}

bool ItemAttributeData::GetLayer(SInt32 layerIndex, std::function<void(TintData&)> functor)
{
	utils::scoped_lock<> locker(m_lock);
	auto it = m_tintData.find(layerIndex);
	if (it != m_tintData.end())
	{
		functor(it->second);
		return true;
	}
	return false;
}
