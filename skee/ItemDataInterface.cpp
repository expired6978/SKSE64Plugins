#include <algorithm>

#include "skse64/GameRTTI.h"
#include "skse64/GameReferences.h"
#include "skse64/GameObjects.h"
#include "skse64/GameExtraData.h"

#include "skse64/NiNodes.h"

#include "skse64/PluginAPI.h"

#include "skse64/PapyrusEvents.h"

#include "ItemDataInterface.h"
#include "TintMaskInterface.h"
#include "ShaderUtilities.h"
#include "OverrideInterface.h"
#include "NifUtils.h"
#include "Utilities.h"

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

		if ((m_identifier.type & ModifiedItemIdentifier::kTypeRank) == ModifiedItemIdentifier::kTypeRank) {
			if (pExtraDataList->HasType(kExtraData_Rank)) {
				ExtraRank * extraRank = static_cast<ExtraRank*>(pExtraDataList->GetByType(kExtraData_Rank));
				if (m_identifier.rankId == extraRank->rank)
					isMatch = true;
			}
		}
		
		if ((m_identifier.type & ModifiedItemIdentifier::kTypeUID) == ModifiedItemIdentifier::kTypeUID) {
			if (pExtraDataList->HasType(kExtraData_UniqueID)) {
				ExtraUniqueID * extraUID = static_cast<ExtraUniqueID*>(pExtraDataList->GetByType(kExtraData_UniqueID));
				if (m_identifier.uid == extraUID->uniqueId && m_identifier.ownerForm == extraUID->ownerFormId)
					isMatch = true;
			}
		}
		
		if ((m_identifier.type & ModifiedItemIdentifier::kTypeSlot) == ModifiedItemIdentifier::kTypeSlot) {
			if (isWorn) {
				TESObjectARMO * armor = DYNAMIC_CAST(pEntryData->type, TESForm, TESObjectARMO);

				// It's armor and we don't have a slot mask search
				if (armor && m_identifier.slotMask != 0) {
					if ((armor->bipedObject.GetSlotMask() & m_identifier.slotMask) != 0)
						isMatch = true;
				}
				// Is not an armor, we don't have a slot mask search, and it's equipped
				else if (!armor && m_identifier.slotMask == 0) {
					if ((m_identifier.weaponSlot == ModifiedItemIdentifier::kHandSlot_Left && hasWornLeft) || (m_identifier.weaponSlot == ModifiedItemIdentifier::kHandSlot_Right && hasWorn))
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

bool ResolveModifiedIdentifier(TESObjectREFR * reference, ModifiedItemIdentifier & identifier, ModifiedItem & itemData)
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

std::shared_ptr<ItemAttributeData> ItemDataInterface::GetExistingData(TESObjectREFR * reference, ModifiedItemIdentifier & identifier)
{
	ModifiedItem foundData;
	if (ResolveModifiedIdentifier(reference, identifier, foundData)) {
		return foundData.GetAttributeData(reference, false, false, identifier.IsSelf());
	}

	return NULL;
}

NIOVTaskUpdateItemDye::NIOVTaskUpdateItemDye(Actor * actor, ModifiedItemIdentifier & identifier)
{
	m_formId = actor->formID;
	m_identifier = identifier;
}

void NIOVTaskUpdateItemDye::Run()
{
	TESForm * form = LookupFormByID(m_formId);
	Actor * actor = DYNAMIC_CAST(form, TESForm, Actor);
	if (actor) {
		ModifiedItem foundData;
		if (ResolveModifiedIdentifier(actor, m_identifier, foundData)) {
			auto data = foundData.GetAttributeData(actor, false, true, m_identifier.IsSelf());
			if (!data) {
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
							VisitObjects(parent, [&](NiAVObject* object)
							{
								if (object->GetAsBSGeometry()) {
									g_tintMaskInterface.ApplyMasks(actor, isFirstPerson, armor, arma, object, [=]()
									{
										return data;
									});
								}
								return false;
							});
						});
					}
				}
			}
		}
	}
}

UInt32 ItemDataInterface::GetItemUniqueID(TESObjectREFR * reference, ModifiedItemIdentifier & identifier, bool makeUnique)
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
		data->m_tintData[textureIndex].m_colorMap[layerIndex] = color;
	}
}

void ItemDataInterface::SetItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, SKEEFixedString blendMode)
{
	auto data = GetData(uniqueID);
	if (data) {
		data->m_tintData[textureIndex].m_blendMap[layerIndex] = g_stringTable.GetString(blendMode);
	}
}

void ItemDataInterface::SetItemTextureLayerType(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, UInt32 type)
{
	auto data = GetData(uniqueID);
	if (data) {
		data->m_tintData[textureIndex].m_typeMap[layerIndex] = static_cast<UInt8>(type);
	}
}

void ItemDataInterface::SetItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex, SKEEFixedString texture)
{
	auto data = GetData(uniqueID);
	if (data) {
		data->m_tintData[textureIndex].m_textureMap[layerIndex] = g_stringTable.GetString(texture);
	}
}

UInt32 ItemDataInterface::GetItemTextureLayerColor(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	UInt32 color = 0;
	auto data = GetData(uniqueID);
	if (data)
	{
		auto & layerData = data->m_tintData.find(textureIndex);
		if (layerData != data->m_tintData.end())
		{
			auto & it = layerData->second.m_colorMap.find(layerIndex);
			if (it != layerData->second.m_colorMap.end()) {
				return it->second;
			}
		}
		
	}

	return color;
}

SKEEFixedString ItemDataInterface::GetItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data)
	{
		auto & layerData = data->m_tintData.find(textureIndex);
		if (layerData != data->m_tintData.end())
		{
			auto & it = layerData->second.m_blendMap.find(layerIndex);
			if (it != layerData->second.m_blendMap.end()) {
				return it->second ? *it->second : "";
			}
		}

	}

	return "";
}

UInt32 ItemDataInterface::GetItemTextureLayerType(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data)
	{
		auto & layerData = data->m_tintData.find(textureIndex);
		if (layerData != data->m_tintData.end())
		{
			auto & it = layerData->second.m_typeMap.find(layerIndex);
			if (it != layerData->second.m_typeMap.end()) {
				return it->second;
			}
		}

	}

	return -1;
}

SKEEFixedString ItemDataInterface::GetItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data)
	{
		auto & layerData = data->m_tintData.find(textureIndex);
		if (layerData != data->m_tintData.end())
		{
			auto & it = layerData->second.m_textureMap.find(layerIndex);
			if (it != layerData->second.m_textureMap.end()) {
				return it->second ? *it->second : "";
			}
		}
	}

	return "";
}

void ItemDataInterface::ClearItemTextureLayerColor(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data) {
		auto & layerData = data->m_tintData.find(textureIndex);
		if (layerData != data->m_tintData.end())
		{
			auto & it = layerData->second.m_colorMap.find(layerIndex);
			if (it != layerData->second.m_colorMap.end()) {
				layerData->second.m_colorMap.erase(it);
			}

			// The whole layer is now empty as a result, erase the parent
			if (layerData->second.empty()) {
				data->m_tintData.erase(layerData);
			}
		}
	}
}

void ItemDataInterface::ClearItemTextureLayerBlendMode(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data) {
		auto & layerData = data->m_tintData.find(textureIndex);
		if (layerData != data->m_tintData.end())
		{
			auto & it = layerData->second.m_blendMap.find(layerIndex);
			if (it != layerData->second.m_blendMap.end()) {
				layerData->second.m_blendMap.erase(it);
			}

			// The whole layer is now empty as a result, erase the parent
			if (layerData->second.empty()) {
				data->m_tintData.erase(layerData);
			}
		}
	}
}

void ItemDataInterface::ClearItemTextureLayerType(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data) {
		auto & layerData = data->m_tintData.find(textureIndex);
		if (layerData != data->m_tintData.end())
		{
			auto & it = layerData->second.m_typeMap.find(layerIndex);
			if (it != layerData->second.m_typeMap.end()) {
				layerData->second.m_typeMap.erase(it);
			}

			// The whole layer is now empty as a result, erase the parent
			if (layerData->second.empty()) {
				data->m_tintData.erase(layerData);
			}
		}
	}
}

void ItemDataInterface::ClearItemTextureLayerTexture(UInt32 uniqueID, SInt32 textureIndex, SInt32 layerIndex)
{
	auto data = GetData(uniqueID);
	if (data) {
		auto & layerData = data->m_tintData.find(textureIndex);
		if (layerData != data->m_tintData.end())
		{
			auto & it = layerData->second.m_textureMap.find(layerIndex);
			if (it != layerData->second.m_textureMap.end()) {
				layerData->second.m_textureMap.erase(it);
			}

			// The whole layer is now empty as a result, erase the parent
			if (layerData->second.empty()) {
				data->m_tintData.erase(layerData);
			}
		}
	}
}

void ItemDataInterface::ClearItemTextureLayer(UInt32 uniqueID, SInt32 textureIndex)
{
	auto data = GetData(uniqueID);
	if (data) {
		auto & layerData = data->m_tintData.find(textureIndex);
		if (layerData != data->m_tintData.end()) {
			data->m_tintData.erase(layerData);
		}
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

void ItemDataInterface::Revert()
{
	SimpleLocker lock(&m_lock);
	m_data.clear();
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

	auto & it = m_data.find(form->formID);
	if (it != m_data.end()) {
		return true;
	}

	return false;
}

UInt32 DyeMap::GetDyeColor(TESForm * form)
{
	SimpleLocker lock(&m_lock);

	auto & it = m_data.find(form->formID);
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
	auto & it = m_data.find(form->formID);
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
	if (!m_tintData.empty()) {
		UInt32 numSubrecords = m_tintData.size();
		intfc->WriteRecordData(&numSubrecords, sizeof(numSubrecords));

		for (auto & layer : m_tintData)
		{
			intfc->OpenRecord('TINT', kVersion);

			SInt32 layerIndex = layer.first;
			intfc->WriteRecordData(&layerIndex, sizeof(layerIndex));

			layer.second.Save(intfc, kVersion);
		}
	}
	else {
		UInt32 numSubrecords = 0;
		intfc->WriteRecordData(&numSubrecords, sizeof(numSubrecords));
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

						if (m_tintData[layerIndex].Load(intfc, version, stringTable))
						{
							error = true;
							return error;
						}
					}
					break;
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
						if (!intfc->ResolveFormId(itemHandle, &newItemForm)) {
							_WARNING("%s - item %08X could not be found, skipping", __FUNCTION__, itemHandle);
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

						TESForm * ownerForm = LookupFormByID(ownerFormId);
						TESForm * itemForm = LookupFormByID(itemFormId);

						Actor * actor = DYNAMIC_CAST(ownerForm, TESForm, Actor);
						TESObjectARMO * armor = DYNAMIC_CAST(itemForm, TESForm, TESObjectARMO);
						if (actor && armor) {
							ModifiedItemIdentifier identifier;
							identifier.SetSlotMask(armor->bipedObject.GetSlotMask());
							g_task->AddTask(new NIOVTaskUpdateItemDye(actor, identifier));
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

	return error;
}
