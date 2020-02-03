#include <vector>

#include "ItemDataInterface.h"

#include "skse64/PluginAPI.h"
#include "skse64/ScaleformMovie.h"

#include "skse64/GameForms.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameReferences.h"
#include "skse64/GameExtraData.h"

#include "TintMaskInterface.h"
#include "ScaleformFunctions.h"

extern SKSETaskInterface	* g_task;
extern ItemDataInterface	g_itemDataInterface;
extern TintMaskInterface	g_tintMaskInterface;
extern DyeMap			g_dyeMap;

class DyeableItemCollector
{
public:
	typedef std::vector<ModifiedItemIdentifier> FoundItems;

	DyeableItemCollector() {}

	bool Accept(InventoryEntryData* pEntryData)
	{
		if (!pEntryData)
			return true;

		if (pEntryData->countDelta < 1)
			return true;

		ExtendDataList* pExtendList = pEntryData->extendDataList;
		if (!pExtendList)
			return true;

		SInt32 n = 0;
		BaseExtraList* pExtraDataList = pExtendList->GetNthItem(n);
		while (pExtraDataList)
		{
			// Only armor right now
			if (TESObjectARMO * armor = DYNAMIC_CAST(pEntryData->type, TESForm, TESObjectARMO)) {
				ModifiedItemIdentifier itemData;
				if (ExtraRank * extraRank = static_cast<ExtraRank*>(pExtraDataList->GetByType(kExtraData_Rank)))
				{
					itemData.type |= ModifiedItemIdentifier::kTypeRank;
					itemData.rankId = extraRank->rank;
				}
				if (ExtraUniqueID * extraUID = static_cast<ExtraUniqueID*>(pExtraDataList->GetByType(kExtraData_UniqueID)))
				{
					itemData.type |= ModifiedItemIdentifier::kTypeUID;
					itemData.uid = extraUID->uniqueId;
					itemData.ownerForm = extraUID->ownerFormId;
				}
				if (pExtraDataList->HasType(kExtraData_Worn) || pExtraDataList->HasType(kExtraData_WornLeft))
				{
					itemData.type |= ModifiedItemIdentifier::kTypeSlot;
					itemData.slotMask = armor->bipedObject.GetSlotMask();
				}

				if (itemData.type != ModifiedItemIdentifier::kTypeNone && g_tintMaskInterface.IsDyeable(armor)) {
					itemData.form = pEntryData->type;
					itemData.extraData = pExtraDataList;
					m_found.push_back(itemData);
				}
			}

			n++;
			pExtraDataList = pExtendList->GetNthItem(n);
		}

		return true;
	}

	FoundItems& Found()
	{
		return m_found;
	}
private:
	FoundItems	m_found;
};

class DyeItemCollector
{
public:
	struct FoundData
	{
		TESForm * form;
		SInt32	count;
		std::vector<UInt32> colors;
	};
	typedef std::vector<FoundData> FoundItems;

	DyeItemCollector() {}

	bool Accept(InventoryEntryData* pEntryData)
	{
		if (!pEntryData)
			return true;

		if (pEntryData->countDelta < 1)
			return true;

		if (pEntryData->type->formType == AlchemyItem::kTypeID) {
			AlchemyItem * potion = DYNAMIC_CAST(pEntryData->type, TESForm, AlchemyItem);
			if (potion) {
				FoundData found;
				found.form = NULL;
				found.count = 0;
				AlchemyItem::EffectItem * effect = NULL;
				for (UInt32 i = 0; i < potion->effectItemList.count; i++) {
					if (potion->effectItemList.GetNthItem(i, effect)) {
						if (g_dyeMap.IsValidDye(effect->mgef)) {
							found.form = pEntryData->type;
							found.count = pEntryData->countDelta;
							found.colors.push_back(g_dyeMap.GetDyeColor(effect->mgef));
						}
					}
				}

				if (g_dyeMap.IsValidDye(potion)) {
					found.form = potion;
					found.count = pEntryData->countDelta;
					found.colors.clear();
					found.colors.push_back(g_dyeMap.GetDyeColor(potion));
				}

				if (found.form)
					m_found.push_back(found);
			}
		} else if (g_dyeMap.IsValidDye(pEntryData->type)) {
			FoundData found;
			found.form = pEntryData->type;
			found.count = pEntryData->countDelta;
			found.colors.push_back(g_dyeMap.GetDyeColor(found.form));
			m_found.push_back(found);
		}
		return true;
	}

	FoundItems& Found()
	{
		return m_found;
	}
private:
	FoundItems	m_found;
};

void SKSEScaleform_GetDyeableItems::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);

	UInt32		formidArg = 0;
	TESForm		* formArg = NULL;

	if (args->numArgs >= 1) {
		formidArg = (UInt32)args->args[0].GetNumber();
		if (formidArg > 0)
			formArg = LookupFormByID(formidArg);
	}

	Actor * actor = DYNAMIC_CAST(formArg, TESForm, Actor);
	if (!actor) {
		_MESSAGE("%s - Invalid form type (%X)", __FUNCTION__, formidArg);
		return;
	}
	
	ExtraContainerChanges * extraContainer = static_cast<ExtraContainerChanges*>(actor->extraData.GetByType(kExtraData_ContainerChanges));
	if (extraContainer) {
		DyeableItemCollector::FoundItems foundData;
		if (extraContainer->data && extraContainer->data->objList) {
			DyeableItemCollector dyeFinder;
			extraContainer->data->objList->Visit(dyeFinder);
			foundData = dyeFinder.Found();

			if (!foundData.empty()) {
				args->movie->CreateArray(args->result);

				for (auto & item : foundData) {
					GFxValue itm;
					args->movie->CreateObject(&itm);
					RegisterNumber(&itm, "type", item.type);
					RegisterNumber(&itm, "uid", item.uid);
					RegisterNumber(&itm, "owner", item.ownerForm);
					RegisterNumber(&itm, "rankId", item.rankId);
					RegisterNumber(&itm, "slotMask", item.slotMask);
					RegisterNumber(&itm, "weaponSlot", item.weaponSlot);

					const char * itemName = NULL;
					if (item.form && item.extraData) {
						itemName = item.extraData->GetDisplayName(item.form);
						if (!itemName) {
							TESFullName* pFullName = DYNAMIC_CAST(item.form, TESForm, TESFullName);
							if (pFullName)
								itemName = pFullName->name.data;
						}

						if (itemName)
							RegisterString(&itm, args->movie, "name", itemName);
					}

					std::shared_ptr<ItemAttributeData> itemData;
					if ((item.type & ModifiedItemIdentifier::kTypeRank) == ModifiedItemIdentifier::kTypeRank)
						itemData = g_itemDataInterface.GetData(item.rankId);

					// This is an approx color lookup, its possible multiple shapes may have differing color templates but use the same override
					// we can only show one color so last shape wins
					if (item.form->formType == TESObjectARMO::kTypeID)
					{
						std::map<SInt32, UInt32> colorMap;
						g_tintMaskInterface.GetTemplateColorMap(actor, static_cast<TESObjectARMO*>(item.form), colorMap);

						GFxValue baseArray;
						args->movie->CreateArray(&baseArray);
						for (UInt32 i = 0; i < 15; i++) {
							GFxValue colorValue;
							colorValue.SetNumber(colorMap[i]);
							baseArray.PushBack(&colorValue);
						}

						itm.SetMember("base", &baseArray);
					}

					GFxValue colorArray;
					args->movie->CreateArray(&colorArray);
					for (UInt32 i = 0; i < 15; i++) {
						UInt32 color = 0;
						if (itemData) {
							const auto & layerData = itemData->m_tintData.find(0);
							if (layerData != itemData->m_tintData.end())
							{
								const auto & it = layerData->second.m_colorMap.find(i);
								if (it != layerData->second.m_colorMap.end())
									color = it->second;
							}
						}

						GFxValue colorValue;
						colorValue.SetNumber(color);
						colorArray.PushBack(&colorValue);
					}

					itm.SetMember("colors", &colorArray);
					args->result->PushBack(&itm);
				}
			}
		}
	}
}

void SKSEScaleform_GetDyeItems::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 1);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);

	UInt32		formidArg = 0;
	TESForm		* formArg = NULL;

	if (args->numArgs >= 1) {
		formidArg = (UInt32)args->args[0].GetNumber();
		if (formidArg > 0)
			formArg = LookupFormByID(formidArg);
	}

	TESObjectREFR * reference = DYNAMIC_CAST(formArg, TESForm, Actor);
	if (!reference) {
		_MESSAGE("%s - Invalid form type (%X)", __FUNCTION__, formidArg);
		return;
	}

	ExtraContainerChanges * extraContainer = static_cast<ExtraContainerChanges*>(reference->extraData.GetByType(kExtraData_ContainerChanges));
	if (extraContainer) {
		DyeItemCollector::FoundItems foundData;
		if (extraContainer->data && extraContainer->data->objList) {
			DyeItemCollector dyeFinder;
			extraContainer->data->objList->Visit(dyeFinder);
			foundData = dyeFinder.Found();

			if (!foundData.empty()) {
				args->movie->CreateArray(args->result);

				for (auto & item : foundData) {
					GFxValue itm;
					args->movie->CreateObject(&itm);
					RegisterNumber(&itm, "formId", item.form->formID);
					RegisterNumber(&itm, "count", item.count);

					GFxValue colorArray;
					args->movie->CreateArray(&colorArray);
					for (auto color : item.colors) {
						GFxValue itemColor;
						itemColor.SetNumber(color);
						colorArray.PushBack(&itemColor);
					}
					itm.SetMember("colors", &colorArray);

					const char * itemName = NULL;
					if (item.form) {
						TESFullName* pFullName = DYNAMIC_CAST(item.form, TESForm, TESFullName);
						if (pFullName)
							RegisterString(&itm, args->movie, "name", pFullName->name.data);
					}

					args->result->PushBack(&itm);
				}
			}
		}
	}
}

void SKSEScaleform_SetItemDyeColor::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 2);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Object);
	ASSERT(args->args[2].GetType() == GFxValue::kType_Number);

	UInt32		formidArg = 0;
	TESForm		* formArg = NULL;
	UInt32		maskIndex = args->args[2].GetNumber();
	UInt32		color = 0;
	bool		clear = false;

	if (args->numArgs >= 3) {
		if (args->args[3].GetType() == GFxValue::kType_Undefined || args->args[3].GetType() == GFxValue::kType_Null)
			clear = true;
		else
			color = args->args[3].GetNumber();
	} else {
		clear = true;
	}

	if (args->numArgs >= 1) {
		formidArg = (UInt32)args->args[0].GetNumber();
		if (formidArg > 0)
			formArg = LookupFormByID(formidArg);
	}

	Actor * actor = DYNAMIC_CAST(formArg, TESForm, Actor);
	if (!actor) {
		_MESSAGE("%s - Invalid form type (%X)", __FUNCTION__, formidArg);
		return;
	}

	ModifiedItemIdentifier identifier;
	GFxValue param[6];

	if (args->args[1].HasMember("type")) {
		args->args[1].GetMember("type", &param[0]);
		identifier.type = param[0].GetNumber();
	}
	if (args->args[1].HasMember("uid")) {
		args->args[1].GetMember("uid", &param[1]);
		identifier.uid = param[1].GetNumber();
	}
	if (args->args[1].HasMember("owner")) {
		args->args[1].GetMember("owner", &param[2]);
		identifier.ownerForm = param[2].GetNumber();
	}
	if (args->args[1].HasMember("rankId")) {
		args->args[1].GetMember("rankId", &param[3]);
		identifier.rankId = param[3].GetNumber();
	}
	if (args->args[1].HasMember("slotMask")) {
		args->args[1].GetMember("slotMask", &param[4]);
		identifier.slotMask = param[4].GetNumber();
	}
	if (args->args[1].HasMember("weaponSlot")) {
		args->args[1].GetMember("weaponSlot", &param[5]);
		identifier.weaponSlot = param[5].GetNumber();
	}
	std::map<SInt32, UInt32> slotTextureIndexMap;
	UInt32 uniqueId = g_itemDataInterface.GetItemUniqueID(actor, identifier, true);
	TESForm* dyedItem = g_itemDataInterface.GetFormFromUniqueID(uniqueId);
	if (dyedItem && dyedItem->formType == TESObjectARMO::kTypeID)
	{
		g_tintMaskInterface.GetSlotTextureIndexMap(actor, static_cast<TESObjectARMO*>(dyedItem), slotTextureIndexMap);
	}

	if (clear)
		g_itemDataInterface.ClearItemTextureLayerColor(uniqueId, slotTextureIndexMap[maskIndex], maskIndex);
	else
		g_itemDataInterface.SetItemTextureLayerColor(uniqueId, slotTextureIndexMap[maskIndex], maskIndex, color);

	g_task->AddTask(new NIOVTaskUpdateItemDye(actor, identifier, TintMaskInterface::kUpdate_All, false));
}

void SKSEScaleform_SetItemDyeColors::Invoke(Args * args)
{
	ASSERT(args->numArgs >= 2);
	ASSERT(args->args[0].GetType() == GFxValue::kType_Number);
	ASSERT(args->args[1].GetType() == GFxValue::kType_Object);
	ASSERT(args->args[2].GetType() == GFxValue::kType_Array);

	UInt32		formidArg = 0;
	TESForm		* formArg = NULL;

	if (args->numArgs >= 1) {
		formidArg = (UInt32)args->args[0].GetNumber();
		if (formidArg > 0)
			formArg = LookupFormByID(formidArg);
	}

	Actor * actor = DYNAMIC_CAST(formArg, TESForm, Actor);
	if (!actor) {
		_MESSAGE("%s - Invalid form type (%X)", __FUNCTION__, formidArg);
		return;
	}

	ModifiedItemIdentifier identifier;
	GFxValue param[6];

	if (args->args[1].HasMember("type")) {
		args->args[1].GetMember("type", &param[0]);
		identifier.type = param[0].GetNumber();
	}
	if (args->args[1].HasMember("uid")) {
		args->args[1].GetMember("uid", &param[1]);
		identifier.uid = param[1].GetNumber();
	}
	if (args->args[1].HasMember("owner")) {
		args->args[1].GetMember("owner", &param[2]);
		identifier.ownerForm = param[2].GetNumber();
	}
	if (args->args[1].HasMember("rankId")) {
		args->args[1].GetMember("rankId", &param[3]);
		identifier.rankId = param[3].GetNumber();
	}
	if (args->args[1].HasMember("slotMask")) {
		args->args[1].GetMember("slotMask", &param[4]);
		identifier.slotMask = param[4].GetNumber();
	}
	if (args->args[1].HasMember("weaponSlot")) {
		args->args[1].GetMember("weaponSlot", &param[5]);
		identifier.weaponSlot = param[5].GetNumber();
	}

	std::map<SInt32, UInt32> slotTextureIndexMap;
	
	UInt32 uniqueId = g_itemDataInterface.GetItemUniqueID(actor, identifier, true);
	TESForm* dyedItem = g_itemDataInterface.GetFormFromUniqueID(uniqueId);
	if (dyedItem && dyedItem->formType == TESObjectARMO::kTypeID)
	{
		g_tintMaskInterface.GetSlotTextureIndexMap(actor, static_cast<TESObjectARMO*>(dyedItem), slotTextureIndexMap);
	}

	UInt32 size = args->args[2].GetArraySize();
	for (UInt32 i = 0; i < size; ++i)
	{
		GFxValue element;
		args->args[2].GetElement(i, &element);

		if (element.GetType() == GFxValue::kType_Undefined || element.GetType() == GFxValue::kType_Null) {
			g_itemDataInterface.ClearItemTextureLayerColor(uniqueId, slotTextureIndexMap[i], i);
		}
		else {
			UInt32 color = element.GetNumber();
			g_itemDataInterface.SetItemTextureLayerColor(uniqueId, slotTextureIndexMap[i], i, color);
		}
	}

	g_task->AddTask(new NIOVTaskUpdateItemDye(actor, identifier, TintMaskInterface::kUpdate_All, false));
}