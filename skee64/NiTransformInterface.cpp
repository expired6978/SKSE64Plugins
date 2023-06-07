#include "NiTransformInterface.h"
#include "ShaderUtilities.h"
#include "SkeletonExtender.h"
#include "ActorUpdateManager.h"
#include "StringTable.h"
#include "NifUtils.h"
#include "Utilities.h"

#include "skse64/PluginAPI.h"
#include "skse64/GameReferences.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameData.h"

#include "skse64/GameStreams.h"

#include "skse64/NiNodes.h"
#include "skse64/NiSerialization.h"
#include "skse64/NiExtraData.h"

#include <algorithm>

extern SKSETaskInterface			* g_task;
extern StringTable					g_stringTable;
extern bool							g_enableEquippableTransforms;
extern UInt16						g_scaleMode;
extern SkeletonExtenderInterface	g_skeletonExtenderInterface;
extern ActorUpdateManager			g_actorUpdateManager;

skee_u32 NiTransformInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void NodeTransformKeys::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	UInt32 numNodes = this->size();
	intfc->WriteRecordData(&numNodes, sizeof(numNodes));

	for (NodeTransformKeys::iterator it = this->begin(); it != this->end(); ++it)
	{
		intfc->OpenRecord('NOTM', kVersion);

		// Key
		WriteKey<StringTableItem>(intfc, it->first, kVersion);

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool NodeTransformKeys::Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	UInt32 type, length, version;
	bool error = false;

	// Handle count
	UInt32 numRegs = 0;
	if (!intfc->ReadRecordData(&numRegs, sizeof(numRegs)))
	{
		_ERROR("%s - Error loading override registration count", __FUNCTION__);
		error = true;
		return error;
	}

	for (UInt32 i = 0; i < numRegs; i++)
	{
		if (intfc->GetNextRecordInfo(&type, &version, &length))
		{
			switch (type)
			{
				case 'NOTM':
				{
					StringTableItem key;
					if (ReadKey<StringTableItem>(intfc, key, kVersion, stringTable)) {
						_ERROR("%s - Error loading node entry key", __FUNCTION__);
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
						_ERROR("%s - Error loading node overrides", __FUNCTION__);
						error = true;
						return error;
					}
					break;
				}
				default:
				{
					_ERROR("%s - Error loading unexpected chunk type %08X (%.4s)", __FUNCTION__, type, reinterpret_cast<char*>(&type));
					error = true;
					return error;
				}
			}
		}
	}

	return error;
}

void NodeTransformRegistrationMapHolder::Save(SKSESerializationInterface* intfc, UInt32 kVersion)
{
	for (NodeTransformRegistrationMapHolder::RegMap::iterator it = m_data.begin(); it != m_data.end(); ++it) {
		intfc->OpenRecord('ACTM', kVersion);

		// Key
		UInt64 handle = it->first;
		intfc->WriteRecordData(&handle, sizeof(handle));

#ifdef _DEBUG
		_MESSAGE("%s - Saving Handle %016llX", __FUNCTION__, handle);
#endif

		// Value
		it->second.Save(intfc, kVersion);
	}
}

bool NodeTransformRegistrationMapHolder::Load(SKSESerializationInterface* intfc, UInt32 kVersion, UInt32 * outFormId, const StringIdMap & stringTable)
{
	bool error = false;

	UInt64 handle = 0;
	// Key
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		_ERROR("%s - Error loading reg key", __FUNCTION__);
		error = true;
		return error;
	}

	MultiRegistration<MultiRegistration<NodeTransformKeys, 2>,2> reg;
	if (reg.Load(intfc, kVersion, stringTable))
	{
		_ERROR("%s - Error loading transform gender registrations", __FUNCTION__);
		error = true;
		return error;
	}

	UInt64 newHandle = 0;
	if (!ResolveAnyHandle(intfc, handle, &newHandle)) {
		*outFormId = 0;
		return error;
	}

	UInt32 formId = newHandle & 0xFFFFFFFFF;

	TESForm* form = LookupFormByID(formId);
	if (!form || form->formType != Character::kTypeID) {
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
	_MESSAGE("%s - Loaded FormId %08X", __FUNCTION__, formId);
#endif

	//SetHandleProperties(newHandle, false);
	return error;
}

class NIOVTaskUpdateReference : public TaskDelegate
{
public:
	NIOVTaskUpdateReference(UInt32 formId, NiTransformInterface * xFormInterface)
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

	UInt32 m_formId;
	NiTransformInterface * m_interface;
};

void NiTransformInterface::VisitStrings(std::function<void(SKEEFixedString)> functor)
{
	for (auto & i1 : transformData.m_data) {
		for (UInt8 gender = 0; gender <= 1; gender++) {
			for (UInt8 fp = 0; fp <= 1; fp++) {
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

void NiTransformInterface::Save(SKSESerializationInterface * intfc, UInt32 kVersion)
{
	transformData.Save(intfc, kVersion);
}
bool NiTransformInterface::Load(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap & stringTable)
{
	UInt32 formId = 0;
	if (!transformData.Load(intfc, kVersion, &formId, stringTable))
	{
		RemoveInvalidTransforms(formId);
		RemoveNamedTransforms(formId, "internal");

		g_actorUpdateManager.AddTransformUpdate(formId);
	}

	return false;
}

bool NiTransformInterface::Impl_AddNodeTransform(TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, OverrideVariant & value)
{
	SimpleLocker lock(&transformData.m_lock);

	transformData.m_data[refr->formID][isFemale ? 1 : 0][firstPerson ? 1 : 0][g_stringTable.GetString(node)][g_stringTable.GetString(name)].erase(value);
	transformData.m_data[refr->formID][isFemale ? 1 : 0][firstPerson ? 1 : 0][g_stringTable.GetString(node)][g_stringTable.GetString(name)].insert(value);
	return true;
}


bool NiTransformInterface::Impl_RemoveNodeTransform(TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name)
{
	SimpleLocker lock(&transformData.m_lock);

	UInt8 gender = isFemale ? 1 : 0;
	UInt8 fp = firstPerson ? 1 : 0;

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

void NiTransformInterface::RemoveInvalidTransforms(UInt32 formId)
{
	auto it = transformData.m_data.find(formId);
	if (it != transformData.m_data.end())
	{
		for (UInt8 gender = 0; gender <= 1; gender++)
		{
			for (UInt8 fp = 0; fp <= 1; fp++)
			{
				for (auto & ait : it->second[gender][fp])
				{
					for (auto it = ait.second.begin(); it != ait.second.end();)
					{
						std::string strKey(*it->first);
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

void NiTransformInterface::RemoveNamedTransforms(UInt32 formId, SKEEFixedString name)
{
	SimpleLocker lock(&transformData.m_lock);

	auto it = transformData.m_data.find(formId);
	if (it != transformData.m_data.end())
	{
		for (UInt8 gender = 0; gender <= 1; gender++)
		{
			for (UInt8 fp = 0; fp <= 1; fp++)
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

	SimpleLocker lock(&transformData.m_lock);
	transformData.m_data.clear();
}

void NiTransformInterface::Impl_RemoveAllReferenceTransforms(TESObjectREFR * refr)
{
	SimpleLocker lock(&transformData.m_lock);

	auto it = transformData.m_data.find(refr->formID);
	if (it != transformData.m_data.end())
	{
		transformData.m_data.erase(it);
	}
}

bool NiTransformInterface::Impl_RemoveNodeTransformComponent(TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, UInt16 key, UInt16 index)
{
	SimpleLocker lock(&transformData.m_lock);

	UInt8 gender = isFemale ? 1 : 0;
	UInt8 fp = firstPerson ? 1 : 0;
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

void NiTransformInterface::Impl_VisitNodes(TESObjectREFR * refr, bool firstPerson, bool isFemale, std::function<bool(SKEEFixedString key, OverrideRegistration<StringTableItem> * value)> functor)
{
	SimpleLocker lock(&transformData.m_lock);

	UInt8 gender = isFemale ? 1 : 0;
	UInt8 fp = firstPerson ? 1 : 0;

	auto it = transformData.m_data.find(refr->formID); // Find ActorHandle
	if (it != transformData.m_data.end())
	{
		for (auto node : it->second[gender][fp]) {
			if (functor(*node.first, &node.second))
				break;
		}
	}
}

bool NiTransformInterface::Impl_VisitNodeTransforms(TESObjectREFR * refr, bool firstPerson, bool isFemale, BSFixedString node, std::function<bool(OverrideRegistration<StringTableItem>*)> each_key, std::function<void(NiNode*, NiAVObject*, NiTransform*)> finalize)
{
	SimpleLocker lock(&transformData.m_lock);

	bool ret = false;
	UInt8 gender = isFemale ? 1 : 0;
	UInt8 fp = firstPerson ? 1 : 0;
	auto it = transformData.m_data.find(refr->formID); // Find ActorHandle
	if (it != transformData.m_data.end())
	{
		NiPointer<NiNode> root = refr->GetNiRootNode(fp);
		if (root) {
			BSFixedString skeleton = GetRootModelPath(refr, firstPerson, isFemale);
			NiPointer<NiAVObject> foundNode = root->GetObjectByName(&node.data);
			if (foundNode) {
				NiTransform * baseTransform = transformCache.GetBaseTransform(skeleton, node, true);
				if (!baseTransform) {
					// Look at extensions
					VisitObjects(root, [&](NiAVObject * root)
					{
						NiPointer<NiExtraData> extraData = root->GetExtraData(BSFixedString("EXTN").data);
						if (extraData) {
							NiStringsExtraData * extraSkeletons = ni_cast(extraData, NiStringsExtraData);
							if (extraSkeletons && (extraSkeletons->m_size % 3) == 0) {
								for (UInt32 i = 0; i < extraSkeletons->m_size; i+= 3) {
									SKEEFixedString extnSkeleton = extraSkeletons->m_data[i+2];
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
						finalize(root, foundNode, baseTransform);
				}
			}
		}
	}

	return ret;
}

void NiTransformInterface::Impl_UpdateNodeTransforms(TESObjectREFR * ref, bool firstPerson, bool isFemale, SKEEFixedString node)
{
	BSFixedString target("");
	NiTransform transformResult;
	Impl_VisitNodeTransforms(ref, firstPerson, isFemale, node, 
	[&](OverrideRegistration<StringTableItem>* keys)
	{
		for (auto dit = keys->begin(); dit != keys->end(); ++dit) {// Loop Keys
			NiTransform localTransform;
			Impl_GetOverrideTransform(&dit->second, OverrideVariant::kParam_NodeTransformPosition, &localTransform);
			Impl_GetOverrideTransform(&dit->second, OverrideVariant::kParam_NodeTransformScale, &localTransform);
			Impl_GetOverrideTransform(&dit->second, OverrideVariant::kParam_NodeTransformRotation, &localTransform);
			transformResult = localTransform * transformResult;

			OverrideVariant value;
			value.key = OverrideVariant::kParam_NodeDestination;
			auto it = dit->second.find(value);
			if (it != dit->second.end()) {
				target = it->str ? it->str->AsBSFixedString() : BSFixedString("");
			}
		}
		return false;
	}, 
	[&](NiNode * root, NiAVObject * foundNode, NiTransform * baseTransform)
	{
		// Process Node Movement
		bool noTarget = target == BSFixedString("");
		if (!noTarget) {
			NiAVObject * targetNode = root->GetObjectByName(&target.data);
			if (targetNode) {
				NiNode * parentNode = targetNode->GetAsNiNode();
				if (parentNode) {
					if (g_task)
						g_task->AddTask(new NIOVTaskMoveNode(parentNode, foundNode));
				}
			}
		}

		// Process Transform
		foundNode->m_localTransform = (*baseTransform) * transformResult;
		if (g_task)
			g_task->AddTask(new NIOVTaskUpdateWorldData(foundNode));
	});
}

OverrideVariant NiTransformInterface::Impl_GetOverrideNodeValue(TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, UInt16 key, SInt8 index)
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
	std::function<void(NiNode*,NiAVObject*,NiTransform*)>());
	return foundValue;
}

bool NiTransformInterface::Impl_GetOverrideNodeTransform(TESObjectREFR * refr, bool firstPerson, bool isFemale, SKEEFixedString node, SKEEFixedString name, UInt16 key, NiTransform * result)
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
	[&](NiNode * root, NiAVObject * foundNode, NiTransform * baseTransform)
	{
		if (name == SKEEFixedString(""))
			*result = *baseTransform;
	});
}

void NiTransformInterface::Impl_UpdateNodeAllTransforms(TESObjectREFR * refr)
{
	SetTransforms(refr->formID);
}

void NiTransformInterface::SetTransforms(UInt32 formId, bool immediate, bool reset)
{
	SimpleLocker lock(&transformData.m_lock);

	TESForm* form = LookupFormByID(formId);
	if (!form || form->formType != Character::kTypeID)
	{
		return;
	}

	TESObjectREFR * refr = static_cast<TESObjectREFR*>(form);
	if (!refr) {
		return;
	}

	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if (actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	auto it = transformData.m_data.find(formId); // Find ActorHandle
	if (it != transformData.m_data.end())
	{
		std::unordered_map<NiAVObject*, NiNode*> nodeMovement;
		NiPointer<NiNode> lastNode = NULL;
		for (UInt8 i = 0; i <= 1; i++)
		{
			NiPointer<NiNode> root = refr->GetNiRootNode(i);
			if (root == lastNode) // First and Third are the same, skip
				continue;

			SKEEFixedString skeleton = GetRootModelPath(refr, i >= 1 ? true : false, gender >= 1 ? true : false);
			if (root)
			{
				// Gather up skeleton extensions
				std::vector<SKEEFixedString> additionalSkeletons;
				std::set<SKEEFixedString> modified, changed;
				VisitObjects(root, [&](NiAVObject * root)
				{
					NiPointer<NiExtraData> extraData = root->GetExtraData(BSFixedString("EXTN").data);
					if (extraData) {
						NiStringsExtraData * extraSkeletons = ni_cast(extraData, NiStringsExtraData);
						if (extraSkeletons && (extraSkeletons->m_size % 3) == 0) {
							for (UInt32 i = 0; i < extraSkeletons->m_size; i += 3) {
								SKEEFixedString extnSkeleton = extraSkeletons->m_data[i+2];
								additionalSkeletons.push_back(extnSkeleton);
							}
						}
					}
					if (g_enableEquippableTransforms)
					{
						NiStringExtraData * stringData = ni_cast(root->GetExtraData(BSFixedString("SDTA").data), NiStringExtraData);
						if (stringData)
						{
							g_skeletonExtenderInterface.ReadTransforms(refr, stringData->m_pString, i >= 1 ? true : false, gender >= 1 ? true : false, modified, changed);
						}
						NiFloatExtraData * floatData = ni_cast(root->GetExtraData(BSFixedString("HH_OFFSET").data), NiFloatExtraData);
						if (floatData)
						{
							char buffer[32 + std::numeric_limits<float>::digits];
							sprintf_s(buffer, sizeof(buffer), "[{\"name\":\"NPC\",\"pos\":[0,0,%f]}]", floatData->m_data);
							g_skeletonExtenderInterface.ReadTransforms(refr, buffer, i >= 1 ? true : false, gender >= 1 ? true : false, modified, changed);
						}
					}

					return false;
				});

				if (g_enableEquippableTransforms)
				{
					NiStringsExtraData * globalData = ni_cast(FindExtraData(root, "BNDT"), NiStringsExtraData);
					if (globalData)
					{
						if (modified.size() > 0)
						{
							std::vector<BSFixedString> newNodes;
							for (auto & node : modified)
							{
								newNodes.push_back(node);
							}

							globalData->SetData(&newNodes.at(0), newNodes.size());
						}
					}
				}

				for (auto ait = it->second[gender][i].begin(); ait != it->second[gender][i].end(); ++ait) // Loop Nodes
				{
					NiTransform * baseTransform = transformCache.GetBaseTransform(skeleton, *ait->first, true);
					if (!baseTransform) { // Not found in base skeleton, search additional skeletons
						for (auto & secondaryPath : additionalSkeletons) {
							baseTransform = transformCache.GetBaseTransform(secondaryPath, *ait->first, false);
							if (baseTransform)
								break;
						}
					}

					if (baseTransform)
					{
						BSFixedString target("");
						float fScaleValue = 1.0;
						NiTransform combinedTransform;
						if (!reset) {
							UInt16 scaleMode = g_scaleMode;
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
								NiTransform localTransform;
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
									target = BSFixedString(it->str ? it->str->c_str() : "");
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
						BSFixedString nodeName = *ait->first;
						NiPointer<NiAVObject> transformable = root->GetObjectByName(&nodeName.data);
						if (transformable) {
							transformable->m_localTransform = (*baseTransform) * combinedTransform;

							// Collect Node Movements
							bool noTarget = target == BSFixedString("");
							if (!noTarget) {
								NiPointer<NiAVObject> targetNode = root->GetObjectByName(&target.data);
								if (targetNode) {
									NiNode * parentNode = targetNode->GetAsNiNode();
									if (parentNode) {
										nodeMovement.insert_or_assign(transformable, parentNode);
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
				NiPointer<NiNode> rc = nodePair.second;
				NIOVTaskMoveNode * newTask = new NIOVTaskMoveNode(rc, nodePair.first);
				if (g_task && !immediate) {
					g_task->AddTask(newTask);
				}
				else {
					newTask->Run();
					newTask->Dispose();
				}
			}

			NIOVTaskUpdateWorldData * newTask = new NIOVTaskUpdateWorldData(root.get());
			if (g_task && !immediate) {
				g_task->AddTask(newTask);
			}
			else {
				newTask->Run();
				newTask->Dispose();
			}
		}
	}
}

void NiTransformInterface::Impl_GetOverrideTransform(OverrideSet * set, UInt16 key, NiTransform * result)
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
				result->pos.x = it->data.f;
			}
			value.index = 1;
			it = set->find(value);
			if (it != set->end()) {
				result->pos.y = it->data.f;
			}
			value.index = 2;
			it = set->find(value);
			if (it != set->end()) {
				result->pos.z = it->data.f;
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
				result->rot.data[0][0] = it->data.f;
			}
			value.index = 1;
			it = set->find(value);
			if (it != set->end()) {
				result->rot.data[0][1] = it->data.f;
			}
			value.index = 2;
			it = set->find(value);
			if (it != set->end()) {
				result->rot.data[0][2] = it->data.f;
			}
			value.index = 3;
			it = set->find(value);
			if (it != set->end()) {
				result->rot.data[1][0] = it->data.f;
			}
			value.index = 4;
			it = set->find(value);
			if (it != set->end()) {
				result->rot.data[1][1] = it->data.f;
			}
			value.index = 5;
			it = set->find(value);
			if (it != set->end()) {
				result->rot.data[1][2] = it->data.f;
			}
			value.index = 6;
			it = set->find(value);
			if (it != set->end()) {
				result->rot.data[2][0] = it->data.f;
			}
			value.index = 7;
			it = set->find(value);
			if (it != set->end()) {
				result->rot.data[2][1] = it->data.f;
			}
			value.index = 8;
			it = set->find(value);
			if (it != set->end()) {
				result->rot.data[2][2] = it->data.f;
			}
		}
		break;
	}
}


NiTransform * NodeTransformCache::GetBaseTransform(SKEEFixedString rootModel, SKEEFixedString nodeName, bool relative)
{
	SimpleLocker lock(&m_lock);

	auto it = m_data.find(rootModel);
	if (it != m_data.end()) {
		auto nodeIt = it->second.find(nodeName);
		if (nodeIt != it->second.end()) {
			return &nodeIt->second;
		}
		else
			return NULL;
	}

	char pathBuffer[MAX_PATH];
	SKEEFixedString newPath = rootModel;
	if (relative) {
		memset(pathBuffer, 0, MAX_PATH);
		sprintf_s(pathBuffer, MAX_PATH, "meshes\\%s", rootModel.c_str());
		newPath = pathBuffer;
	}

	// No skeleton path found, why is this?
	BSResourceNiBinaryStream binaryStream(newPath.c_str());
	if (!binaryStream.IsValid()) {
		_ERROR("%s - Failed to acquire skeleton at \"%s\".", __FUNCTION__, newPath.c_str());
		return NULL;
	}

	NodeMap transformMap;
	NiTransform * foundTransform = NULL;

	UInt8 niStreamMemory[sizeof(NiStream)];
	memset(niStreamMemory, 0, sizeof(NiStream));
	NiStream * niStream = (NiStream *)niStreamMemory;
	CALL_MEMBER_FN(niStream, ctor)();

	niStream->LoadStream(&binaryStream);
	if (niStream->m_rootObjects.m_data)
	{
		for (UInt32 i = 0; i < niStream->m_rootObjects.m_emptyRunStart; i++) {
			NiObject * object = niStream->m_rootObjects.m_data[i];
			if (object) {
				NiAVObject * node = ni_cast(object, NiAVObject);
				if (node) {
					VisitObjects(node, [&](NiAVObject* child)
					{
						if (child->m_name == NULL)
							return false;

						SKEEFixedString localName(child->m_name);
						if (localName.length() == 0)
							return false;

						transformMap.insert_or_assign(localName, child->m_localTransform);
						return false;
					});
				}
			}
		}
	}

	CALL_MEMBER_FN(niStream, dtor)();
	auto modelIt = m_data.insert_or_assign(rootModel, transformMap);
	if (modelIt.second) {
		auto nodeIt = modelIt.first->second.find(nodeName);
		if (nodeIt != modelIt.first->second.end()) {
			return &nodeIt->second;
		}
	}

	return NULL;
}

SKEEFixedString NiTransformInterface::GetRootModelPath(TESObjectREFR * refr, bool firstPerson, bool isFemale)
{
	TESModel * model = NULL;
	Character * character = DYNAMIC_CAST(refr, TESObjectREFR, Character);
	if (character) {
		if (firstPerson) {
			Setting	* setting = (*g_gameSettingCollection)->Get("sFirstPersonSkeleton");
			if (setting && setting->GetType() == Setting::kType_String)
				return SKEEFixedString(setting->data.s);
		}

		TESRace * race = character->race;
		if (!race) {
			TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
			if (actorBase)
				race = actorBase->race.race;
		}

		if (race)
			model = &race->models[isFemale ? 1 : 0];
	}
	else
		model = DYNAMIC_CAST(refr->baseForm, TESForm, TESModel);

	if (model)
		return SKEEFixedString(model->GetModelName());

	return SKEEFixedString("");
}

void NiTransformInterface::PrintDiagnostics()
{
	Console_Print("NiTransformInterface Diagnostics:");
	transformData.Lock();
	Console_Print("\t%llu actors with transforms", transformData.m_data.size());
	for (auto& entry : transformData.m_data)
	{
		TESForm* form = LookupFormByID(entry.first);
		TESObjectREFR* refr = form ? DYNAMIC_CAST(form, TESForm, TESObjectREFR) : nullptr;
		_MESSAGE("Reference: %08X (%s) (%lld/%lld male 3p/fp %lld/%lld female 3p/fp transforms)", entry.first, refr ? CALL_MEMBER_FN(refr, GetReferenceName)() : "", entry.second[0][0].size(), entry.second[0][1].size(), entry.second[1][0].size(), entry.second[1][1].size());
		for (uint8_t gender = 0; gender <= 1; ++gender)
		{
			for (uint8_t persp = 0; persp <= 1; ++persp)
			{
				_MESSAGE("\tGender: %s Perspective: %s (%lld nodes)", gender == 0 ? "male" : "female", persp == 0 ? "third" : "first", entry.second[gender][persp].size());
				for (auto& xForm : entry.second[gender][persp])
				{
					_MESSAGE("\t\tNode: %s (%lld keys)", xForm.first ? xForm.first->c_str() : "", xForm.second.size());
					for (auto& ovr : xForm.second)
					{
						_MESSAGE("\t\t\tKey: %s (%lld overrides)", ovr.first ? ovr.first->c_str() : "", ovr.second.size());
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

bool NiTransformInterface::HasNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	NiTransform transform;
	return Impl_GetOverrideNodeTransform(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformPosition, &transform);
}

bool NiTransformInterface::HasNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	NiTransform transform;
	return Impl_GetOverrideNodeTransform(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, &transform);
}

bool NiTransformInterface::HasNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	return Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScale, 0);
}

bool NiTransformInterface::HasNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	OverrideVariant overrideVariant = Impl_GetOverrideNodeValue(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScaleMode, 0);
	if (overrideVariant.type == OverrideVariant::kType_Int && overrideVariant.key == OverrideVariant::kParam_NodeTransformScale)
	{
		return true;
	}

	return false;
}

void NiTransformInterface::AddNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, Position& position)
{
	float* pos = reinterpret_cast<float*>(&position);
	OverrideVariant posV[3];
	for (UInt32 i = 0; i < 3; i++) {
		PackValue<float>(&posV[i], OverrideVariant::kParam_NodeTransformPosition, i, &pos[i]);
		Impl_AddNodeTransform(ref, firstPerson, isFemale, node, name, posV[i]);
	}
}

void NiTransformInterface::AddNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, Rotation& rotationEuler)
{
	NiMatrix33 rotation;

	rotationEuler.heading *= MATH_PI / 180;
	rotationEuler.attitude *= MATH_PI / 180;
	rotationEuler.bank *= MATH_PI / 180;

	rotation.SetEulerAngles(rotationEuler.heading, rotationEuler.attitude, rotationEuler.bank);

	OverrideVariant rotV[9];
	for (UInt32 i = 0; i < 9; i++) {
		PackValue<float>(&rotV[i], OverrideVariant::kParam_NodeTransformRotation, i, &rotation.arr[i]);
		Impl_AddNodeTransform(ref, firstPerson, isFemale, node, name, rotV[i]);
	}
}

void NiTransformInterface::AddNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, float scale)
{
	OverrideVariant scaleVar;
	PackValue<float>(&scaleVar, OverrideVariant::kParam_NodeTransformScale, 0, &scale);
	Impl_AddNodeTransform(ref, firstPerson, isFemale, node, name, scaleVar);
}

void NiTransformInterface::AddNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, skee_u32 scaleMode)
{
	OverrideVariant scaleModeVar;
	UInt32 sMode = scaleMode;
	PackValue<UInt32>(&scaleModeVar, OverrideVariant::kParam_NodeTransformScaleMode, 0, &sMode);
	Impl_AddNodeTransform(ref, firstPerson, isFemale, node, name, scaleModeVar);
}

INiTransformInterface::Position NiTransformInterface::GetNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	Position position;
	NiTransform transform;
	bool ret = Impl_GetOverrideNodeTransform(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformPosition, &transform);
	position.x = transform.pos.x;
	position.y = transform.pos.y;
	position.z = transform.pos.z;
	return position;
}

INiTransformInterface::Rotation NiTransformInterface::GetNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	NiTransform transform;
	bool ret = Impl_GetOverrideNodeTransform(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformRotation, &transform);
	Rotation rotation;
	transform.rot.GetEulerAngles(&rotation.heading, &rotation.attitude, &rotation.bank);
	rotation.heading *= 180 / MATH_PI;
	rotation.attitude *= 180 / MATH_PI;
	rotation.bank *= 180 / MATH_PI;
	return rotation;
}

float NiTransformInterface::GetNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	NiTransform transform;
	Impl_GetOverrideNodeTransform(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScale, &transform);
	return transform.scale;
}

skee_u32 NiTransformInterface::GetNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	OverrideVariant overrideVariant = Impl_GetOverrideNodeValue(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScaleMode, 0);
	if (overrideVariant.type == OverrideVariant::kType_Int && overrideVariant.key == OverrideVariant::kParam_NodeTransformScale)
	{
		return overrideVariant.data.u;
	}

	return -1;
}

bool NiTransformInterface::RemoveNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
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

bool NiTransformInterface::RemoveNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
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

bool NiTransformInterface::RemoveNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	return Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScale, 0);
}

bool NiTransformInterface::RemoveNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	return Impl_RemoveNodeTransformComponent(ref, firstPerson, isFemale, node, name, OverrideVariant::kParam_NodeTransformScaleMode, 0);
}


bool NiTransformInterface::RemoveNodeTransform(TESObjectREFR* refr, bool firstPerson, bool isFemale, const char* node, const char* name)
{
	return Impl_RemoveNodeTransform(refr, firstPerson, isFemale, node, name);
}

void NiTransformInterface::RemoveAllReferenceTransforms(TESObjectREFR* refr)
{
	Impl_RemoveAllReferenceTransforms(refr);
}

bool NiTransformInterface::GetOverrideNodeTransform(TESObjectREFR* refr, bool firstPerson, bool isFemale, const char* node, const char* name, skee_u16 key, NiTransform* result)
{
	return Impl_GetOverrideNodeTransform(refr, firstPerson, isFemale, node, name, key, result);
}

void NiTransformInterface::UpdateNodeAllTransforms(TESObjectREFR* ref)
{
	Impl_UpdateNodeAllTransforms(ref);
}

void NiTransformInterface::VisitNodes(TESObjectREFR* refr, bool firstPerson, bool isFemale, NodeVisitor& visitor)
{
	Impl_VisitNodes(refr, firstPerson, isFemale, [&](SKEEFixedString node, OverrideRegistration<StringTableItem>* reg)
	{
		for (auto& set : *reg)
		{
			for (auto& item : set.second)
			{
				if (item.key == OverrideVariant::kParam_NodeTransformPosition && item.index == 0)
				{
					Position pos = GetNodeTransformPosition(refr, firstPerson, isFemale, node, set.first->c_str());
					visitor.VisitPosition(node, set.first->c_str(), pos);
				}
				else if (item.key == OverrideVariant::kParam_NodeTransformRotation && item.index == 0)
				{
					Rotation rot = GetNodeTransformRotation(refr, firstPerson, isFemale, node, set.first->c_str());
					visitor.VisitRotation(node, set.first->c_str(), rot);
				}
				else if (item.key == OverrideVariant::kParam_NodeTransformScale)
				{
					float scale = GetNodeTransformScale(refr, firstPerson, isFemale, node, set.first->c_str());
					visitor.VisitScale(node, set.first->c_str(), scale);
				}
				else if (item.key == OverrideVariant::kParam_NodeTransformScaleMode)
				{
					UInt32 scaleMode = GetNodeTransformScaleMode(refr, firstPerson, isFemale, node, set.first->c_str());
					visitor.VisitScaleMode(node, set.first->c_str(), scaleMode);
				}
			}
		}
		return false;
	});
}

void NiTransformInterface::UpdateNodeTransforms(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node)
{
	Impl_UpdateNodeTransforms(ref, firstPerson, isFemale, node);
}