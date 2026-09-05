#include <RE/G/GameSettingCollection.h>
#include "NiTransformInterface.h"
#include "SKEETasks.h"
#include "ShaderUtilities.h"
#include "SkeletonExtender.h"
#include "ActorUpdateManager.h"
#include "StringTable.h"
#include "NifUtils.h"
#include "Utilities.h"



#include "RE/N/NiExtraData.h"

#include <algorithm>
#include <cstdint>
#include <numbers>


extern const SKSE::TaskInterface* g_task;
extern StringTable					g_stringTable;
extern bool							g_enableEquippableTransforms;
extern std::uint16_t						g_scaleMode;
extern SkeletonExtenderInterface	g_skeletonExtenderInterface;
extern ActorUpdateManager			g_actorUpdateManager;

skee_u32 NiTransformInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void NodeTransformKeys::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	std::uint32_t numNodes = this->size();
	intfc->WriteRecordData(&numNodes, sizeof(numNodes));

	for (NodeTransformKeys::iterator it = this->begin(); it != this->end(); ++it)
	{
		if (!intfc->OpenRecord('NOTM', kVersion)) {
			SKSE::log::error("{} - Failed to open record", __FUNCTION__);
		}

		// Key
		WriteKey<StringTableItem>(intfc, it->first, kVersion);

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool NodeTransformKeys::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	std::uint32_t type, length, version;
	bool error = false;

	// Handle count
	std::uint32_t numRegs = 0;
	if (!intfc->ReadRecordData(&numRegs, sizeof(numRegs)))
	{
		SKSE::log::error("{} - Error loading override registration count", __FUNCTION__);
		error = true;
		return error;
	}

	for (std::uint32_t i = 0; i < numRegs; i++)
	{
		if (intfc->GetNextRecordInfo(type, version, length))
		{
			switch (type)
			{
				case 'NOTM':
				{
					StringTableItem key;
					if (ReadKey<StringTableItem>(intfc, key, kVersion, stringTable)) {
						SKSE::log::error("{} - Error loading node entry key", __FUNCTION__);
						error = true;
						return error;
					}

					// operator[] not working for some odd reason
					bool loadError = false;
					NodeTransformKeys::iterator iter = this->find(key); // Find existing first
					if (iter != this->end()) {
						error = iter->second.Load(intfc, version, stringTable);
					}
					else { // No existing, create
						OverrideRegistration<StringTableItem> set;
						error = set.Load(intfc, version, stringTable);
						emplace(key, set);
					}
					if (loadError)
					{
						SKSE::log::error("{} - Error loading node overrides", __FUNCTION__);
						error = true;
						return error;
					}
					break;
				}
				default:
				{
					SKSE::log::error("{} - Error loading unexpected chunk type {:08X} ({:.4})", __FUNCTION__, type, std::string(reinterpret_cast<char*>(&type), 4));
					error = true;
					return error;
				}
			}
		}
	}

	return error;
}

void NodeTransformRegistrationMapHolder::Save(SKSE::SerializationInterface* intfc, std::uint32_t kVersion)
{
	for (NodeTransformRegistrationMapHolder::RegMap::iterator it = m_data.begin(); it != m_data.end(); ++it) {
		if (!intfc->OpenRecord('ACTM', kVersion)) {
			SKSE::log::error("{} - Failed to open record", __FUNCTION__);
		}

		// Key
		std::uint64_t handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		SKSE::log::info("{} - Saving Handle %016llX", __FUNCTION__, handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool NodeTransformRegistrationMapHolder::Load(SKSE::SerializationInterface* intfc, std::uint32_t kVersion, std::uint32_t * outFormId, const StringIdMap & stringTable)
{
	bool error = false;

	std::uint64_t handle = 0;
	// Key
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		SKSE::log::error("{} - Error loading reg key", __FUNCTION__);
		error = true;
		return error;
	}

	MultiRegistration<MultiRegistration<NodeTransformKeys, 2>,2> reg;
	if (reg.Load(intfc, kVersion, stringTable))
	{
		SKSE::log::error("{} - Error loading transform gender registrations", __FUNCTION__);
		error = true;
		return error;
	}

	std::uint64_t newHandle = 0;
	if (!ResolveAnyHandle(intfc, handle, &newHandle)) {
		*outFormId = 0;
		return error;
	}

	std::uint32_t formId = newHandle & 0xFFFFFFFFF;

	RE::TESForm* form = RE::TESForm::LookupByID(formId);
	if (!form || form->IsNot(RE::FormType::ActorCharacter)) {
		*outFormId = 0;
		return error;
	}

	if (reg.empty()) {
		*outFormId = 0;
		return error;
	}

	*outFormId = formId;

	Lock();
	m_data[formId] = reg;
	Release();

#ifdef _DEBUG
	SKSE::log::info("{} - Loaded FormId {:08X}", __FUNCTION__, formId);
#endif

	//SetHandleProperties(newHandle, false);
	return error;
}

class NIOVTaskUpdateReference : public SKSE::detail::TaskDelegate
{
public:
	NIOVTaskUpdateReference(std::uint32_t formId, NiTransformInterface * xFormInterface)
	{
		m_formId = formId;
		m_interface = xFormInterface;
	}
	virtual void Run()
	{
		m_interface->SetTransforms(m_formId, true);
	}
	virtual void Dispose()
	{
		delete this;
	}

	std::uint32_t m_formId;
	NiTransformInterface * m_interface;
};

void NiTransformInterface::VisitStrings(std::function<void(SKEEFixedString)> functor)
{
	for (auto & i1 : transformData.m_data) {
		for (std::uint8_t gender = 0; gender <= 1; gender++) {
			for (std::uint8_t fp = 0; fp <= 1; fp++) {
				for (auto & i2 : i1.second[gender][fp]) {
					functor(*i2.first);
					for (auto & i3 : i2.second) {
						functor(*i3.first);
						for (auto & i4 : i3.second) {
							if (i4.type == OverrideVariant::kType_String) {
								functor(*i4.str);
							}
						}
					}
				}
			}
		}
	}
}

void NiTransformInterface::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	transformData.Save(intfc, kVersion);
}
bool NiTransformInterface::Load(SKSE::SerializationInterface* intfc, std::uint32_t kVersion, const StringIdMap & stringTable)
{
	std::uint32_t formId = 0;
	if (!transformData.Load(intfc, kVersion, &formId, stringTable))
	{
		RemoveInvalidTransforms(formId);
		RemoveNamedTransforms(formId, "internal");

		g_actorUpdateManager.AddTransformUpdate(formId);
	}

	return false;
}

bool NiTransformInterface::Impl_AddNodeTransform(RE::TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, OverrideVariant & value)
{
	std::lock_guard lock(transformData.m_lock);

	transformData.m_data[refr->formID][isFemale ? 1 : 0][firstPerson ? 1 : 0][g_stringTable.GetString(node)][g_stringTable.GetString(name)].erase(value);
	transformData.m_data[refr->formID][isFemale ? 1 : 0][firstPerson ? 1 : 0][g_stringTable.GetString(node)][g_stringTable.GetString(name)].insert(value);
	return true;
}

bool NiTransformInterface::Impl_RemoveNodeTransform(RE::TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name)
{
	std::lock_guard lock(transformData.m_lock);

	std::uint8_t gender = isFemale ? 1 : 0;
	std::uint8_t fp = firstPerson ? 1 : 0;

	auto it = transformData.m_data.find(refr->formID);
	if (it != transformData.m_data.end())
	{
		auto ait = it->second[gender][fp].find(g_stringTable.GetString(node));
		if (ait != it->second[gender][fp].end())
		{
			auto oit = ait->second.find(g_stringTable.GetString(name));
			if (oit != ait->second.end())
			{
				ait->second.erase(oit);
				return true;
			}
		}
	}

	return false;
}

void NiTransformInterface::RemoveInvalidTransforms(std::uint32_t formId)
{
	auto it = transformData.m_data.find(formId);
	if (it != transformData.m_data.end())
	{
		for (std::uint8_t gender = 0; gender <= 1; gender++)
		{
			for (std::uint8_t fp = 0; fp <= 1; fp++)
			{
				for (auto & ait : it->second[gender][fp])
				{
					for (auto it = ait.second.begin(); it != ait.second.end();)
					{
						std::string strKey(it->first->c_str());
						SKEEFixedString ext(strKey.substr(strKey.find_last_of(".") + 1).c_str());
						if (ext == SKEEFixedString("esp") || ext == SKEEFixedString("esm") || ext == SKEEFixedString("esl"))
						{
							it = ait.second.erase(it);
						}
						else
							++it;
					}
				}
			}
		}
	}
}

void NiTransformInterface::RemoveNamedTransforms(std::uint32_t formId, SKEEFixedString name)
{
	std::lock_guard lock(transformData.m_lock);

	auto it = transformData.m_data.find(formId);
	if (it != transformData.m_data.end())
	{
		for (std::uint8_t gender = 0; gender <= 1; gender++)
		{
			for (std::uint8_t fp = 0; fp <= 1; fp++)
			{
				for (auto& ait : it->second[gender][fp])
				{
					auto oit = ait.second.find(g_stringTable.GetString(name));
					if (oit != ait.second.end())
					{
						ait.second.erase(oit);
					}
				}
			}
		}
	}
}

void NiTransformInterface::Revert()
{
	// Revert all transforms to their base data
	for (auto & it : transformData.m_data) {
		SetTransforms(it.first, false, true);
	}

	std::lock_guard lock(transformData.m_lock);
	transformData.m_data.clear();
}

void NiTransformInterface::Impl_RemoveAllReferenceTransforms(RE::TESObjectREFR * refr)
{
	std::lock_guard lock(transformData.m_lock);

	auto it = transformData.m_data.find(refr->formID);
	if (it != transformData.m_data.end())
	{
		transformData.m_data.erase(it);
	}
}

bool NiTransformInterface::Impl_RemoveNodeTransformComponent(RE::TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, std::uint16_t key, std::uint16_t index)
{
	std::lock_guard lock(transformData.m_lock);

	std::uint8_t gender = isFemale ? 1 : 0;
	std::uint8_t fp = firstPerson ? 1 : 0;
	auto it = transformData.m_data.find(refr->formID);
	if (it != transformData.m_data.end())
	{
		auto ait = it->second[gender][fp].find(g_stringTable.GetString(node));
		if (ait != it->second[gender][fp].end())
		{
			auto oit = ait->second.find(g_stringTable.GetString(name));
			if (oit != ait->second.end())
			{
				OverrideVariant ovr;
				ovr.key = key;
				ovr.index = index;
				auto ost = oit->second.find(ovr);
				if (ost != oit->second.end())
				{
					oit->second.erase(ost);
					return true;
				}
			}
		}
	}

	return false;
}

void NiTransformInterface::Impl_VisitNodes(RE::TESObjectREFR * refr, bool firstPerson, bool isFemale, std::function<bool(SKEEFixedString key, OverrideRegistration<StringTableItem> * value)> functor)
{
	std::lock_guard lock(transformData.m_lock);

	std::uint8_t gender = isFemale ? 1 : 0;
	std::uint8_t fp = firstPerson ? 1 : 0;

	auto it = transformData.m_data.find(refr->formID); // Find ActorHandle
	if (it != transformData.m_data.end())
	{
		for (auto node : it->second[gender][fp]) {
			if (functor(*node.first, &node.second))
				break;
		}
	}
}

bool NiTransformInterface::Impl_VisitNodeTransforms(RE::TESObjectREFR * refr, bool firstPerson, bool isFemale, RE::BSFixedString node, std::function<bool(OverrideRegistration<StringTableItem>*)> each_key, std::function<void(RE::NiNode*, RE::NiAVObject*, RE::NiTransform*)> finalize)
{
	std::lock_guard lock(transformData.m_lock);

	bool ret = false;
	std::uint8_t gender = isFemale ? 1 : 0;
	std::uint8_t fp = firstPerson ? 1 : 0;
	auto it = transformData.m_data.find(refr->formID); // Find ActorHandle
	if (it != transformData.m_data.end())
	{
		RE::NiPointer<RE::NiNode> root{refr->Get3D(fp) ? refr->Get3D(fp)->AsNode() : nullptr};
		if (root) {
			RE::BSFixedString skeleton = GetRootModelPath(refr, firstPerson, isFemale);
			RE::NiPointer<RE::NiAVObject> foundNode{root->GetObjectByName(node)};
			if (foundNode) {
				RE::NiTransform * baseTransform = transformCache.GetBaseTransform(skeleton, node, true);
				if (!baseTransform) {
					// Look at extensions
					VisitObjects(root.get(), [&](RE::NiAVObject * root)
					{
						RE::NiPointer<RE::NiExtraData> extraData{root->GetExtraData("EXTN")};
						if (extraData) {
							RE::NiStringsExtraData * extraSkeletons = netimmerse_cast<RE::NiStringsExtraData*>(extraData.get());
							if (extraSkeletons && (extraSkeletons->size % 3) == 0) {
								for (std::uint32_t i = 0; i < extraSkeletons->size; i+= 3) {
									SKEEFixedString extnSkeleton = extraSkeletons->value[i+2];
									baseTransform = transformCache.GetBaseTransform(extnSkeleton, node, false);
									if (baseTransform)
										return true;
								}
							}
						}

						return false;
					});
				}

				if (baseTransform) {
					auto nodeIt = it->second[gender][fp].find(g_stringTable.GetString(node));
					if (nodeIt != it->second[gender][fp].end())
						if (each_key(&nodeIt->second))
							ret = true;

					if (finalize)
						finalize(root.get(), foundNode.get(), baseTransform);
				}
			}
		}
	}

	return ret;
}

void NiTransformInterface::Impl_UpdateNodeTransforms(RE::TESObjectREFR * ref, bool firstPerson, bool isFemale, SKEEFixedString node)
{
	RE::BSFixedString target("");
	RE::NiTransform transformResult;
	Impl_VisitNodeTransforms(ref, firstPerson, isFemale, node, 
	[&](OverrideRegistration<StringTableItem>* keys)
	{
		for (auto dit = keys->begin(); dit != keys->end(); ++dit) {// Loop Keys
			RE::NiTransform localTransform;
			Impl_GetOverrideTransform(&dit->second, OverrideVariant::kParam_NodeTransformPosition, &localTransform);
			Impl_GetOverrideTransform(&dit->second, OverrideVariant::kParam_NodeTransformScale, &localTransform);
			Impl_GetOverrideTransform(&dit->second, OverrideVariant::kParam_NodeTransformRotation, &localTransform);
			transformResult = localTransform * transformResult;

			OverrideVariant value;
			value.key = OverrideVariant::kParam_NodeDestination;
			auto it = dit->second.find(value);
			if (it != dit->second.end()) {
				target = it->str ? it->str->AsBSFixedString() : RE::BSFixedString("");
			}
		}
		return false;
	}, 
	[&](RE::NiNode * root, RE::NiAVObject * foundNode, RE::NiTransform * baseTransform)
	{
		// Process Node Movement
		bool noTarget = target == RE::BSFixedString("");
		if (!noTarget) {
			RE::NiAVObject * targetNode = root->GetObjectByName(target);
			if (targetNode) {
				RE::NiNode * parentNode = targetNode ? targetNode->AsNode() : nullptr;
				if (parentNode) {
					if (g_task)
						SKEE_AddTask(g_task, new NIOVTaskMoveNode(RE::NiPointer<RE::NiNode>(parentNode), RE::NiPointer<RE::NiAVObject>(foundNode)));
				}
			}
		}

		// Process Transform
		foundNode->local = (*baseTransform) * transformResult;
		if (g_task)
			SKEE_AddTask(g_task, new NIOVTaskUpdateWorldData(RE::NiPointer<RE::NiAVObject>(foundNode)));
	});
}

OverrideVariant NiTransformInterface::Impl_GetOverrideNodeValue(RE::TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, std::uint16_t key, std::int8_t index)
{
	OverrideVariant foundValue;
	Impl_VisitNodeTransforms(refr, firstPerson, isFemale, node,
		[&](OverrideRegistration<StringTableItem>* keys)
	{
		if (name == SKEEFixedString("")) {
			return true;
		}
		else {
			auto it = keys->find(g_stringTable.GetString(name));
			if (it != keys->end()) {
				OverrideVariant searchValue;
				searchValue.key = key;
				searchValue.index = index;
				auto sit = it->second.find(searchValue);
				if (sit != it->second.end())
					foundValue = *sit;
				return true;
			}
		}

		return false;
	},
	std::function<void(RE::NiNode*,RE::NiAVObject*,RE::NiTransform*)>());
	return foundValue;
}

bool NiTransformInterface::Impl_GetOverrideNodeTransform(RE::TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, std::uint16_t key, RE::NiTransform * result)
{
	return Impl_VisitNodeTransforms(refr, firstPerson, isFemale, node,
	[&](OverrideRegistration<StringTableItem>* keys)
	{
		if (name == SKEEFixedString("")) {
			return true;
		} else {
			auto it = keys->find(g_stringTable.GetString(name));
			if (it != keys->end()) {
				Impl_GetOverrideTransform(&it->second, key, result);
				return true;
			}
		}
		
		return false;
	},
	[&](RE::NiNode * root, RE::NiAVObject * foundNode, RE::NiTransform * baseTransform)
	{
		if (name == SKEEFixedString(""))
			*result = *baseTransform;
	});
}

void NiTransformInterface::Impl_UpdateNodeAllTransforms(RE::TESObjectREFR * refr)
{
	SetTransforms(refr->formID);
}

void NiTransformInterface::SetTransforms(std::uint32_t formId, bool immediate, bool reset)
{
	std::lock_guard lock(transformData.m_lock);

	RE::TESForm* form = RE::TESForm::LookupByID(formId);
	if (!form || form->IsNot(RE::FormType::ActorCharacter))
	{
		return;
	}

	RE::TESObjectREFR * refr = static_cast<RE::TESObjectREFR*>(form);
	if (!refr) {
		return;
	}

	std::uint8_t gender = 0;
	RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if (actorBase)
		gender = static_cast<std::uint8_t>(actorBase->GetSex());

	auto it = transformData.m_data.find(formId); // Find ActorHandle
	if (it != transformData.m_data.end())
	{
		std::unordered_map<RE::NiAVObject*, RE::NiNode*> nodeMovement;
		RE::NiPointer<RE::NiNode> lastNode = NULL;
		for (std::uint8_t i = 0; i <= 1; i++)
		{
			RE::NiPointer<RE::NiNode> root{refr->Get3D(i) ? refr->Get3D(i)->AsNode() : nullptr};
			if (root == lastNode) // First and Third are the same, skip
				continue;

			SKEEFixedString skeleton = GetRootModelPath(refr, i >= 1 ? true : false, gender >= 1 ? true : false);
			if (root)
			{
				// Gather up skeleton extensions
				std::vector<SKEEFixedString> additionalSkeletons;
				std::set<SKEEFixedString> modified, changed;
				VisitObjects(root.get(), [&](RE::NiAVObject * root)
				{
					RE::NiPointer<RE::NiExtraData> extraData{root->GetExtraData("EXTN")};
					if (extraData) {
						RE::NiStringsExtraData * extraSkeletons = netimmerse_cast<RE::NiStringsExtraData*>(extraData.get());
						if (extraSkeletons && (extraSkeletons->size % 3) == 0) {
							for (std::uint32_t i = 0; i < extraSkeletons->size; i += 3) {
								SKEEFixedString extnSkeleton = extraSkeletons->value[i+2];
								additionalSkeletons.push_back(extnSkeleton);
							}
						}
					}
					if (g_enableEquippableTransforms)
					{
						RE::NiStringExtraData * stringData = netimmerse_cast<RE::NiStringExtraData*>(root->GetExtraData("SDTA"));
						if (stringData)
						{
							g_skeletonExtenderInterface.ReadTransforms(refr, stringData->value, i >= 1 ? true : false, gender >= 1 ? true : false, modified, changed);
						}
						RE::NiFloatExtraData * floatData = netimmerse_cast<RE::NiFloatExtraData*>(root->GetExtraData("HH_OFFSET"));
						if (floatData)
						{
							char buffer[32 + std::numeric_limits<float>::digits];
							sprintf_s(buffer, sizeof(buffer), "[{\"name\":\"NPC\",\"pos\":[0,0,%f]}]", floatData->value);
							g_skeletonExtenderInterface.ReadTransforms(refr, buffer, i >= 1 ? true : false, gender >= 1 ? true : false, modified, changed);
						}
					}

					return false;
				});

				if (g_enableEquippableTransforms)
				{
					RE::NiStringsExtraData * globalData = netimmerse_cast<RE::NiStringsExtraData*>(FindExtraData(root.get(), "BNDT"));
					if (globalData)
					{
						if (modified.size() > 0)
						{
							std::vector<RE::BSFixedString> newNodes;
							for (auto & node : modified)
							{
								newNodes.push_back(node);
							}

							NiStringsExtraDataHelper::Replace(globalData, newNodes);
						}
					}
				}

				for (auto ait = it->second[gender][i].begin(); ait != it->second[gender][i].end(); ++ait) // Loop Nodes
				{
					RE::NiTransform * baseTransform = transformCache.GetBaseTransform(skeleton, *ait->first, true);
					if (!baseTransform) { // Not found in base skeleton, search additional skeletons
						for (auto & secondaryPath : additionalSkeletons) {
							baseTransform = transformCache.GetBaseTransform(secondaryPath, *ait->first, false);
							if (baseTransform)
								break;
						}
					}

					if (baseTransform)
					{
						RE::BSFixedString target("");
						float fScaleValue = 1.0;
						RE::NiTransform combinedTransform;
						if (!reset) {
							std::uint16_t scaleMode = g_scaleMode;
							std::map<StringTableItem, OverrideSet*> scaleModes;
							for (auto dit = ait->second.begin(); dit != ait->second.end(); ++dit) {
								scaleModes.emplace(dit->first, &dit->second);
							}
							if (!scaleModes.empty())
							{
								OverrideSet* overrideSet = scaleModes.rbegin()->second;
								OverrideVariant value;
								value.key = OverrideVariant::kParam_NodeTransformScaleMode;
								auto it = overrideSet->find(value);
								if (it != overrideSet->end()) {
									scaleMode = it->data.i;
								}
							}

							for (auto dit = ait->second.begin(); dit != ait->second.end(); ++dit) {// Loop Keys
								RE::NiTransform localTransform;
								Impl_GetOverrideTransform(&dit->second, OverrideVariant::kParam_NodeTransformPosition, &localTransform);
								Impl_GetOverrideTransform(&dit->second, OverrideVariant::kParam_NodeTransformScale, &localTransform);
								Impl_GetOverrideTransform(&dit->second, OverrideVariant::kParam_NodeTransformRotation, &localTransform);
								combinedTransform = combinedTransform * localTransform;

								if (scaleMode == 1 || scaleMode == 2)
								{
									fScaleValue += localTransform.scale;
								}
								if (scaleMode == 3 && localTransform.scale > fScaleValue)
								{
									fScaleValue = localTransform.scale;
								}

								// Find node movement
								OverrideVariant value;
								value.key = OverrideVariant::kParam_NodeDestination;
								auto it = dit->second.find(value);
								if (it != dit->second.end()) {
									target = RE::BSFixedString(it->str ? it->str->c_str() : "");
								}
							}
							if (scaleMode == 1)
							{
								combinedTransform.scale = fScaleValue / (float)(ait->second.size() + 1);
							}
							if (scaleMode == 2 || scaleMode == 3)
							{
								combinedTransform.scale = fScaleValue;
							}
						}
						RE::BSFixedString nodeName = *ait->first;
						RE::NiPointer<RE::NiAVObject> transformable{root->GetObjectByName(nodeName)};
						if (transformable) {
							transformable->local = (*baseTransform) * combinedTransform;

							// Collect Node Movements
							bool noTarget = target == RE::BSFixedString("");
							if (!noTarget) {
								RE::NiPointer<RE::NiAVObject> targetNode{root->GetObjectByName(target)};
								if (targetNode) {
									RE::NiNode * parentNode = targetNode.get() ? targetNode.get()->AsNode() : nullptr;
									if (parentNode) {
										nodeMovement.insert_or_assign(transformable.get(), parentNode);
									}
								}
							}
						}
					}
				}
			}

			lastNode = root;

			for (auto & nodePair : nodeMovement)
			{
				RE::NiPointer<RE::NiNode> rc{nodePair.second};
				NIOVTaskMoveNode * newTask = new NIOVTaskMoveNode(rc, RE::NiPointer<RE::NiAVObject>(nodePair.first));
				if (g_task && !immediate) {
					SKEE_AddTask(g_task, newTask);
				}
				else {
					newTask->Run();
					newTask->Dispose();
				}
			}

			NIOVTaskUpdateWorldData * newTask = new NIOVTaskUpdateWorldData(RE::NiPointer<RE::NiAVObject>(root.get()));
			if (g_task && !immediate) {
				SKEE_AddTask(g_task, newTask);
			}
			else {
				newTask->Run();
				newTask->Dispose();
			}
		}
	}
}

void NiTransformInterface::Impl_GetOverrideTransform(OverrideSet * set, std::uint16_t key, RE::NiTransform * result)
{
	OverrideVariant value;
	OverrideSet::iterator it;
	switch (key) {
		case OverrideVariant::kParam_NodeTransformPosition:
		{
			value.key = OverrideVariant::kParam_NodeTransformPosition;
			value.index = 0;
			it = set->find(value);
			if (it != set->end()) {
				result->translate.x = it->data.f;
			}
			value.index = 1;
			it = set->find(value);
			if (it != set->end()) {
				result->translate.y = it->data.f;
			}
			value.index = 2;
			it = set->find(value);
			if (it != set->end()) {
				result->translate.z = it->data.f;
			}
			break;
		}
		case OverrideVariant::kParam_NodeTransformScale:
		{
			value.key = OverrideVariant::kParam_NodeTransformScale;
			value.index = 0;
			it = set->find(value);
			if (it != set->end()) {
				result->scale = it->data.f;
			}
		}
		break;
		case OverrideVariant::kParam_NodeTransformRotation:
		{
			value.key = OverrideVariant::kParam_NodeTransformRotation;
			value.index = 0;
			it = set->find(value);
			if (it != set->end()) {
				result->rotate.entry[0][0] = it->data.f;
			}
			value.index = 1;
			it = set->find(value);
			if (it != set->end()) {
				result->rotate.entry[0][1] = it->data.f;
			}
			value.index = 2;
			it = set->find(value);
			if (it != set->end()) {
				result->rotate.entry[0][2] = it->data.f;
			}
			value.index = 3;
			it = set->find(value);
			if (it != set->end()) {
				result->rotate.entry[1][0] = it->data.f;
			}
			value.index = 4;
			it = set->find(value);
			if (it != set->end()) {
				result->rotate.entry[1][1] = it->data.f;
			}
			value.index = 5;
			it = set->find(value);
			if (it != set->end()) {
				result->rotate.entry[1][2] = it->data.f;
			}
			value.index = 6;
			it = set->find(value);
			if (it != set->end()) {
				result->rotate.entry[2][0] = it->data.f;
			}
			value.index = 7;
			it = set->find(value);
			if (it != set->end()) {
				result->rotate.entry[2][1] = it->data.f;
			}
			value.index = 8;
			it = set->find(value);
			if (it != set->end()) {
				result->rotate.entry[2][2] = it->data.f;
			}
		}
		break;
	}
}

RE::NiTransform * NodeTransformCache::GetBaseTransform(SKEEFixedString rootModel, SKEEFixedString nodeName, bool relative)
{
	std::lock_guard lock(m_lock);

	auto it = m_data.find(rootModel);
	if (it != m_data.end()) {
		auto nodeIt = it->second.find(nodeName);
		if (nodeIt != it->second.end()) {
			return &nodeIt->second;
		}
		else
			return NULL;
	}

	char pathBuffer[REX::W32::MAX_PATH];
	SKEEFixedString newPath = rootModel;
	if (relative) {
		memset(pathBuffer, 0, REX::W32::MAX_PATH);
		sprintf_s(pathBuffer, REX::W32::MAX_PATH, "meshes\\%s", rootModel.c_str());
		newPath = pathBuffer;
	}

	// No skeleton path found, why is this?
	RE::BSResourceNiBinaryStream binaryStream{newPath.c_str()};

	NodeMap transformMap;
	RE::NiTransform * foundTransform = nullptr;

	NifStreamWrapper niStream;
	niStream->Load1(&binaryStream);
	for (std::uint32_t i = 0; i < niStream->topObjects.size(); i++) {
		RE::NiObject * object = niStream->topObjects[i].get();
		if (object) {
			RE::NiAVObject * node = netimmerse_cast<RE::NiAVObject*>(object);
			if (node) {
				VisitObjects(node, [&](RE::NiAVObject* child)
				{
					if (child->name.length() == 0)
						return false;

					SKEEFixedString localName(child->name.c_str());
					transformMap.insert_or_assign(localName, child->local);
					return false;
				});
			}
		}
	}

	auto modelIt = m_data.insert_or_assign(rootModel, transformMap);
	if (modelIt.second) {
		auto nodeIt = modelIt.first->second.find(nodeName);
		if (nodeIt != modelIt.first->second.end()) {
			return &nodeIt->second;
		}
	}

	return NULL;
}

SKEEFixedString NiTransformInterface::GetRootModelPath(RE::TESObjectREFR * refr, bool firstPerson, bool isFemale)
{
	RE::TESModel * model = nullptr;
	RE::Actor * character = refr ? refr->As<RE::Actor>() : nullptr;
	if (character) {
		if (firstPerson) {
			RE::Setting* setting = RE::GameSettingCollection::GetSingleton()->GetSetting("sFirstPersonSkeleton");
			if (setting && setting->GetType() == RE::Setting::Type::kString)
				return SKEEFixedString(setting->GetString());
		}

		RE::TESRace * race = nullptr;
		if (auto* npc = character->GetBaseObject() ? character->GetBaseObject()->As<RE::TESNPC>() : nullptr) {
			race = npc->GetRace();
		}

		if (race)
			model = &race->skeletonModels[isFemale ? 1 : 0];
	}
	else
		model = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESModel>() : nullptr;

	if (model)
		return SKEEFixedString(model->GetModel());

	return SKEEFixedString("");
}

void NiTransformInterface::PrintDiagnostics()
{
	Console_Print("NiTransformInterface Diagnostics:");
	transformData.Lock();
	Console_Print("\t%llu actors with transforms", transformData.m_data.size());
	for (auto& entry : transformData.m_data)
	{
		RE::TESForm* form = RE::TESForm::LookupByID(entry.first);
		RE::TESObjectREFR* refr = form ? form->As<RE::TESObjectREFR>() : nullptr;
		SKSE::log::info("Reference: {:08X} ({}) ({}/{} male 3p/fp {}/{} female 3p/fp transforms)", entry.first, refr ? refr->GetName() : "", entry.second[0][0].size(), entry.second[0][1].size(), entry.second[1][0].size(), entry.second[1][1].size());
		for (uint8_t gender = 0; gender <= 1; ++gender)
		{
			for (uint8_t persp = 0; persp <= 1; ++persp)
			{
				SKSE::log::info("\tGender: {} Perspective: {} ({} nodes)", gender == 0 ? "male" : "female", persp == 0 ? "third" : "first", entry.second[gender][persp].size());
				for (auto& xForm : entry.second[gender][persp])
				{
					SKSE::log::info("\t\tNode: {} ({} keys)", xForm.first ? xForm.first->c_str() : "", xForm.second.size());
					for (auto& ovr : xForm.second)
					{
						SKSE::log::info("\t\t\tKey: {} ({} overrides)", ovr.first ? ovr.first->c_str() : "", ovr.second.size());
					}
				}
			}
		}
	}
	transformData.Release();
	transformCache.Lock();
	Console_Print("\t%llu skeletons cached", transformCache.m_data.size());
	transformCache.Release();
}

bool NiTransformInterface::HasNodeTransformPosition(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	RE::NiTransform transform;
	return Impl_GetOverrideNodeTransform(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformPosition, &transform);
}

bool NiTransformInterface::HasNodeTransformRotation(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	RE::NiTransform transform;
	return Impl_GetOverrideNodeTransform(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, &transform);
}

bool NiTransformInterface::HasNodeTransformScale(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	return Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScale, 0);
}

bool NiTransformInterface::HasNodeTransformScaleMode(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	OverrideVariant overrideVariant = Impl_GetOverrideNodeValue(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScaleMode, 0);
	if (overrideVariant.type == OverrideVariant::kType_Int && overrideVariant.key == OverrideVariant::kParam_NodeTransformScale)
	{
		return true;
	}

	return false;
}

void NiTransformInterface::AddNodeTransformPosition(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, Position& position)
{
	float* pos = reinterpret_cast<float*>(&position);
	OverrideVariant posV[3];
	for (std::uint32_t i = 0; i < 3; i++) {
		PackValue<float>(&posV[i], OverrideVariant::kParam_NodeTransformPosition, i, &pos[i]);
		Impl_AddNodeTransform(ref, firstPerson, isFemale, node, name, posV[i]);
	}
}

void NiTransformInterface::AddNodeTransformRotation(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, Rotation& rotationEuler)
{
	RE::NiMatrix3 rotation;

	rotationEuler.heading *= std::numbers::pi_v<float> / 180;
	rotationEuler.attitude *= std::numbers::pi_v<float> / 180;
	rotationEuler.bank *= std::numbers::pi_v<float> / 180;

	rotation.SetEulerAnglesXYZ(rotationEuler.heading, rotationEuler.attitude, rotationEuler.bank);

	OverrideVariant rotV[9];
	for (std::uint32_t i = 0; i < 9; i++) {
		PackValue<float>(&rotV[i], OverrideVariant::kParam_NodeTransformRotation, i, &rotation.entry[i/3][i%3]);
		Impl_AddNodeTransform(ref, firstPerson, isFemale, node, name, rotV[i]);
	}
}

void NiTransformInterface::AddNodeTransformScale(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, float scale)
{
	OverrideVariant scaleVar;
	PackValue<float>(&scaleVar, OverrideVariant::kParam_NodeTransformScale, 0, &scale);
	Impl_AddNodeTransform(ref, firstPerson, isFemale, node, name, scaleVar);
}

void NiTransformInterface::AddNodeTransformScaleMode(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, skee_u32 scaleMode)
{
	OverrideVariant scaleModeVar;
	std::uint32_t sMode = scaleMode;
	PackValue<std::uint32_t>(&scaleModeVar, OverrideVariant::kParam_NodeTransformScaleMode, 0, &sMode);
	Impl_AddNodeTransform(ref, firstPerson, isFemale, node, name, scaleModeVar);
}

INiTransformInterface::Position NiTransformInterface::GetNodeTransformPosition(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	Position position;
	RE::NiTransform transform;
	bool ret = Impl_GetOverrideNodeTransform(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformPosition, &transform);
	position.x = transform.translate.x;
	position.y = transform.translate.y;
	position.z = transform.translate.z;
	return position;
}

INiTransformInterface::Rotation NiTransformInterface::GetNodeTransformRotation(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	RE::NiTransform transform;
	bool ret = Impl_GetOverrideNodeTransform(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, &transform);
	Rotation rotation;
	transform.rotate.ToEulerAnglesXYZ(rotation.heading, rotation.attitude, rotation.bank);
	rotation.heading *= 180 / std::numbers::pi_v<float>;
	rotation.attitude *= 180 / std::numbers::pi_v<float>;
	rotation.bank *= 180 / std::numbers::pi_v<float>;
	return rotation;
}

float NiTransformInterface::GetNodeTransformScale(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	RE::NiTransform transform;
	Impl_GetOverrideNodeTransform(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScale, &transform);
	return transform.scale;
}

skee_u32 NiTransformInterface::GetNodeTransformScaleMode(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	OverrideVariant overrideVariant = Impl_GetOverrideNodeValue(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScaleMode, 0);
	if (overrideVariant.type == OverrideVariant::kType_Int && overrideVariant.key == OverrideVariant::kParam_NodeTransformScale)
	{
		return overrideVariant.data.u;
	}

	return -1;
}

bool NiTransformInterface::RemoveNodeTransformPosition(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	bool ret = false;
	if (Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformPosition, 0))
		ret = true;
	if (Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformPosition, 1))
		ret = true;
	if (Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformPosition, 2))
		ret = true;
	return ret;
}

bool NiTransformInterface::RemoveNodeTransformRotation(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	bool ret = false;
	if (Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 0))
		ret = true;
	if (Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 1))
		ret = true;
	if (Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 2))
		ret = true;
	if (Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 3))
		ret = true;
	if (Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 4))
		ret = true;
	if (Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 5))
		ret = true;
	if (Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 6))
		ret = true;
	if (Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 7))
		ret = true;
	if (Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, 8))
		ret = true;
	return ret;
}

bool NiTransformInterface::RemoveNodeTransformScale(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	return Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScale, 0);
}

bool NiTransformInterface::RemoveNodeTransformScaleMode(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	return Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScaleMode, 0);
}

bool NiTransformInterface::RemoveNodeTransform(RE::TESObjectREFR* refr, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	return Impl_RemoveNodeTransform(refr, firstPerson, isFemale, node, name);
}

void NiTransformInterface::RemoveAllReferenceTransforms(RE::TESObjectREFR* refr)
{
	Impl_RemoveAllReferenceTransforms(refr);
}

bool NiTransformInterface::GetOverrideNodeTransform(RE::TESObjectREFR* refr, bool firstPerson, bool isFemale, const char* node, const char* name, skee_u16 key, RE::NiTransform* result)
{
	return Impl_GetOverrideNodeTransform(refr, firstPerson, isFemale, node, name, key, result);
}

void NiTransformInterface::UpdateNodeAllTransforms(RE::TESObjectREFR* ref)
{
	Impl_UpdateNodeAllTransforms(ref);
}

void NiTransformInterface::VisitNodes(RE::TESObjectREFR* refr, bool firstPerson, bool isFemale, NodeVisitor& visitor)
{
	Impl_VisitNodes(refr, firstPerson, isFemale, [&](const SKEEFixedString& node, OverrideRegistration<StringTableItem>* reg)
	{
		for (auto& set : *reg)
		{
			for (auto& item : set.second)
			{
				if (item.key == OverrideVariant::kParam_NodeTransformPosition && item.index == 0)
				{
					Position pos = GetNodeTransformPosition(refr, firstPerson, isFemale, node.c_str(), set.first->c_str());
					visitor.VisitPosition(node.c_str(), set.first->c_str(), pos);
				}
				else if (item.key == OverrideVariant::kParam_NodeTransformRotation && item.index == 0)
				{
					Rotation rot = GetNodeTransformRotation(refr, firstPerson, isFemale, node.c_str(), set.first->c_str());
					visitor.VisitRotation(node.c_str(), set.first->c_str(), rot);
				}
				else if (item.key == OverrideVariant::kParam_NodeTransformScale)
				{
					float scale = GetNodeTransformScale(refr, firstPerson, isFemale, node.c_str(), set.first->c_str());
					visitor.VisitScale(node.c_str(), set.first->c_str(), scale);
				}
				else if (item.key == OverrideVariant::kParam_NodeTransformScaleMode)
				{
					std::uint32_t scaleMode = GetNodeTransformScaleMode(refr, firstPerson, isFemale, node.c_str(), set.first->c_str());
					visitor.VisitScaleMode(node.c_str(), set.first->c_str(), scaleMode);
				}
			}
		}
		return false;
	});
}

void NiTransformInterface::UpdateNodeTransforms(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node)
{
	Impl_UpdateNodeTransforms(ref, firstPerson, isFemale, node);
}