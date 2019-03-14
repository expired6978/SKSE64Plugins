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

ItemAttributeData * ModifiedItem::GetAttributeData(TESObjectREFR * reference, bool makeUnique, bool allowNewEntry, bool isSelf, UInt32 * idOut)
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

ItemAttributeData * ItemDataInterface::GetExistingData(TESObjectREFR * reference, ModifiedItemIdentifier & identifier)
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
			ColorMap * overrides = NULL;
			ItemAttributeData * data = foundData.GetAttributeData(actor, false, true, m_identifier.IsSelf());
			if (!data) {
				_DMESSAGE("%s - Failed to acquire item attribute data", __FUNCTION__);
				return;
			}

			if (data->m_tintData)
				overrides = &data->m_tintData->m_colorMap;

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
									g_tintMaskInterface.ApplyMasks(actor, isFirstPerson, armor, arma, object, [&](ColorMap* colorMap)
									{
										if (overrides)
											*colorMap = *overrides;
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
		ItemAttributeData * data = foundData.GetAttributeData(reference, makeUnique, true, identifier.IsSelf(), &id);
		if (data) {
			return id;
		}
	}

	return 0;
}

void ItemDataInterface::SetItemDyeColor(UInt32 uniqueID, SInt32 maskIndex, UInt32 color)
{
	ItemAttributeData * data = GetData(uniqueID);
	if (data) {
		auto tintData = data->m_tintData;
		if (!tintData) {
			tintData = new ItemAttributeData::TintData;
			data->m_tintData = tintData;
		}

		tintData->m_colorMap[maskIndex] = color;
	}
}

UInt32 ItemDataInterface::GetItemDyeColor(UInt32 uniqueID, SInt32 maskIndex)
{
	UInt32 color = 0;
	ItemAttributeData * data = GetData(uniqueID);
	if (data) {
		auto tintData = data->m_tintData;
		if (tintData) {
			auto & it = tintData->m_colorMap.find(maskIndex);
			color = it->second;
		}
	}

	return color;
}

void ItemDataInterface::ClearItemDyeColor(UInt32 uniqueID, SInt32 maskIndex)
{
	ItemAttributeData * data = GetData(uniqueID);
	if (data) {
		auto tintData = data->m_tintData;
		if (tintData) {
			auto & it = tintData->m_colorMap.find(maskIndex);
			if (it != tintData->m_colorMap.end())
				tintData->m_colorMap.erase(it);
		}
	}
}

ItemAttributeData * ItemDataInterface::GetData(UInt32 rankId)
{
	SimpleLocker lock(&m_lock);
	if (rankId == kInvalidRank)
		return NULL;

	auto it = std::find_if(m_data.begin(), m_data.end(), [&](ItemAttribute& item)
	{
		if (std::get<ITEM_ATTRIBUTE_RANK>(item) == rankId)
			return true;
		return false;
	});

	if (it != m_data.end()) {
		return std::get<ITEM_ATTRIBUTE_DATA>(*it);
	}

	return NULL;
}

TESForm * ItemDataInterface::GetFormFromUniqueID(UInt32 uniqueID)
{
	SimpleLocker lock(&m_lock);
	if (uniqueID == kInvalidRank)
		return NULL;

	auto it = std::find_if(m_data.begin(), m_data.end(), [&](ItemAttribute& item)
	{
		if (std::get<ITEM_ATTRIBUTE_RANK>(item) == uniqueID)
			return true;
		return false;
	});

	if (it != m_data.end()) {
		TESForm * form = LookupFormByID(std::get<ITEM_ATTRIBUTE_FORMID>(*it));
		return form;
	}

	return NULL;
}

TESForm * ItemDataInterface::GetOwnerOfUniqueID(UInt32 uniqueID)
{
	SimpleLocker lock(&m_lock);
	if (uniqueID == kInvalidRank)
		return NULL;

	auto it = std::find_if(m_data.begin(), m_data.end(), [&](ItemAttribute& item)
	{
		if (std::get<ITEM_ATTRIBUTE_RANK>(item) == uniqueID)
			return true;
		return false;
	});

	if (it != m_data.end()) {
		TESForm * form = LookupFormByID(std::get<ITEM_ATTRIBUTE_OWNERFORM>(*it));
		return form;
	}

	return NULL;
}

ItemAttributeData * ItemDataInterface::CreateData(UInt32 rankId, UInt16 uid, UInt32 ownerId, UInt32 formId)
{
	EraseByRank(rankId);
	
	ItemAttributeData * data = new ItemAttributeData;
	Lock();
	m_data.push_back(std::make_tuple(rankId, uid, ownerId, formId, data));
	Release();
	return data;
}

bool ItemDataInterface::UpdateUIDByRank(UInt32 rankId, UInt16 uid, UInt32 formId)
{
	SimpleLocker lock(&m_lock);

	auto it = std::find_if(m_data.begin(), m_data.end(), [&](ItemAttribute& item)
	{
		if (std::get<ITEM_ATTRIBUTE_RANK>(item) == rankId) {
			std::get<ITEM_ATTRIBUTE_UID>(item) = uid;
			std::get<ITEM_ATTRIBUTE_OWNERFORM>(item) = formId;
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
		if (std::get<ITEM_ATTRIBUTE_UID>(item) == oldId && std::get<ITEM_ATTRIBUTE_OWNERFORM>(item) == oldFormId) {
			std::get<ITEM_ATTRIBUTE_UID>(item) = newId;
			std::get<ITEM_ATTRIBUTE_OWNERFORM>(item) = newFormId;
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
		if (std::get<ITEM_ATTRIBUTE_RANK>(item) == rankId) {
			return true;
		}

		return false;
	});

	if (it != m_data.end()) {
		ItemAttributeData * data = std::get<ITEM_ATTRIBUTE_DATA>(*it);
		if (data)
			delete data;

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
		if (std::get<ITEM_ATTRIBUTE_UID>(item) == uid && std::get<ITEM_ATTRIBUTE_OWNERFORM>(item) == formId) {
			return true;
		}

		return false;
	});

	if (it != m_data.end()) {
		ItemAttributeData * data = std::get<ITEM_ATTRIBUTE_DATA>(*it);
		if (data) {
			auto eventDispatcher = static_cast<EventDispatcher<SKSEModCallbackEvent>*>(g_messaging->GetEventDispatcher(SKSEMessagingInterface::kDispatcher_ModEvent));
			if (eventDispatcher) {
				TESForm * form = LookupFormByID(std::get<ITEM_ATTRIBUTE_FORMID>(*it));
				SKSEModCallbackEvent evn("NiOverride_Internal_EraseUID", "", std::get<ITEM_ATTRIBUTE_UID>(*it), form);
				eventDispatcher->SendEvent(&evn);
			}
			delete data;
		}

		m_data.erase(it);
		return true;
	}

	return false;
}

void ItemDataInterface::Revert()
{
	SimpleLocker lock(&m_lock);
	for (auto item : m_data)
	{
		ItemAttributeData * data = std::get<ITEM_ATTRIBUTE_DATA>(item);
		if (data)
			delete data;
	}
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
	UInt32 numTints = m_colorMap.size();
	intfc->WriteRecordData(&numTints, sizeof(numTints));

	for (auto tint : m_colorMap)
	{
		SInt32 maskIndex = tint.first;
		UInt32 maskColor = tint.second;

		intfc->WriteRecordData(&maskIndex, sizeof(maskIndex));
		intfc->WriteRecordData(&maskColor, sizeof(maskColor));
	}
}

bool ItemAttributeData::TintData::Load(SKSESerializationInterface * intfc, UInt32 kVersion)
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
		SInt32 maskIndex;
		if (!intfc->ReadRecordData(&maskIndex, sizeof(maskIndex)))
		{
			_ERROR("%s - Error loading mask index", __FUNCTION__);
			error = true;
			return error;
		}

		UInt32 maskColor;
		if (!intfc->ReadRecordData(&maskColor, sizeof(maskColor)))
		{
			_ERROR("%s - Error loading mask color", __FUNCTION__);
			error = true;
			return error;
		}

		m_colorMap[maskIndex] = maskColor;
	}

	return error;
}

void ItemAttributeData::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	if (m_tintData) {
		UInt32 numSubrecords = 1;
		intfc->WriteRecordData(&numSubrecords, sizeof(numSubrecords));

		intfc->OpenRecord('TINT', kVersion);
		m_tintData->Save(intfc, kVersion);
	}
	else {
		UInt32 numSubrecords = 0;
		intfc->WriteRecordData(&numSubrecords, sizeof(numSubrecords));
	}
}

bool ItemAttributeData::Load(SKSESerializationInterface * intfc, UInt32 kVersion)
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
						ItemAttributeData::TintData * tintData = new ItemAttributeData::TintData;
						if (tintData->Load(intfc, kVersion))
						{
							delete tintData;
							tintData = NULL;
							error = true;
							return error;
						}
						m_tintData = tintData;
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

void ItemDataInterface::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	intfc->OpenRecord('ITEE', kVersion);

	UInt32 nextRank = m_nextRank;
	intfc->WriteRecordData(&nextRank, sizeof(nextRank));

	UInt32 numItems = m_data.size();
	intfc->WriteRecordData(&numItems, sizeof(numItems));

	for (auto attribute : m_data) {
		intfc->OpenRecord('IDAT', kVersion);

		UInt32 rankId = std::get<ITEM_ATTRIBUTE_RANK>(attribute);
		UInt16 uid = std::get<ITEM_ATTRIBUTE_UID>(attribute);

		UInt32 ownerFormId = std::get<ITEM_ATTRIBUTE_OWNERFORM>(attribute);
		UInt32 itemFormId = std::get<ITEM_ATTRIBUTE_FORMID>(attribute);

		TESForm * ownerForm = LookupFormByID(ownerFormId);
		TESForm * itemForm = LookupFormByID(itemFormId);

		UInt64 ownerHandle = VirtualMachine::GetHandle(ownerForm, TESForm::kTypeID);
		UInt64 itemHandle = VirtualMachine::GetHandle(itemForm, TESForm::kTypeID);
		
		intfc->WriteRecordData(&rankId, sizeof(rankId));
		intfc->WriteRecordData(&uid, sizeof(uid));
		intfc->WriteRecordData(&ownerHandle, sizeof(ownerHandle));
		intfc->WriteRecordData(&itemHandle, sizeof(itemHandle));

		ItemAttributeData * data = std::get<ITEM_ATTRIBUTE_DATA>(attribute);
		if (data) {
			data->Save(intfc, kVersion);
		}
	}
}

bool ItemDataInterface::Load(SKSESerializationInterface * intfc, UInt32 kVersion)
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

					UInt64 ownerHandle = 0;
					if (!intfc->ReadRecordData(&ownerHandle, sizeof(ownerHandle)))
					{
						_ERROR("%s - Error loading owner handle", __FUNCTION__);
						error = true;
						return error;
					}

					UInt64 itemHandle = 0;
					if (!intfc->ReadRecordData(&itemHandle, sizeof(itemHandle)))
					{
						_ERROR("%s - Error loading item handle", __FUNCTION__);
						error = true;
						return error;
					}

					UInt64 newOwnerHandle = 0;
					if (!ResolveAnyHandle(intfc, ownerHandle, &newOwnerHandle)) {
						return error;
					}

					UInt64 newItemHandle = 0;
					if (!intfc->ResolveHandle(itemHandle, &newItemHandle)) {
						return error;
					}

					ItemAttributeData * data = new ItemAttributeData;
					if (data->Load(intfc, kVersion)) {
						_ERROR("%s - Failed to load item data for owner %016llX item %016llX", __FUNCTION__, ownerHandle, itemHandle);
						delete data;
						data = NULL;
						error = true;
						return error;
					}

					if (rankId != kInvalidRank) {
						UInt32 ownerFormId = newOwnerHandle & 0xFFFFFFFF;
						UInt32 itemFormId = newItemHandle & 0xFFFFFFFF;

						Lock();
						m_data.push_back(std::make_tuple(rankId, uid, ownerFormId, itemFormId, data));
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