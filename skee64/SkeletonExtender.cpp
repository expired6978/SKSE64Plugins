#include "SkeletonExtender.h"
#include "ShaderUtilities.h"
#include "NifUtils.h"

#include "RE/N/NiExtraData.h"
#include "RE/N/NiStringsExtraData.h"
#include "RE/N/NiStringExtraData.h"
#include "RE/N/NiFloatExtraData.h"
#include "RE/N/NiStream.h"
#include "RE/N/NiBinaryStream.h"
#include "RE/N/NiMath.h"
#include "RE/N/NiMatrix3.h"
#include "SKEEHooks.h"

#include <cstdint>
#include <vector>

extern bool	g_enableEquippableTransforms;

void SkeletonExtenderInterface::Attach(RE::TESObjectREFR * refr, RE::NiNode * skeleton, RE::NiAVObject * objectRoot)
{
	RE::NiStringsExtraData * extraData = netimmerse_cast<RE::NiStringsExtraData*>(FindExtraData(objectRoot, "EXTN"));
	if(extraData)
	{
		if(extraData->size % 3 != 0) {
	#ifdef _DEBUG
			SKSE::log::error("{} - Error attaching additional skeleton info to {:08X} invalid entry count, must be divisible by 3.", __FUNCTION__, refr->formID);
	#endif
			return;
		}

		for(std::uint32_t i = 0; i < extraData->size; i += 3)
		{
			RE::BSFixedString targetNodeName = extraData->value[i];
			RE::BSFixedString sourceNodeName = extraData->value[i+1];
			RE::BSFixedString templatePath = extraData->value[i+2];

			RE::NiAVObject * targetNode = skeleton->GetObjectByName(targetNodeName);
			if(targetNode) {
				RE::NiAVObject * sourceNode = targetNode->GetObjectByName(sourceNodeName);
				if(!sourceNode) { // Make sure the source node doesn't exist
					RE::NiNode * targetNiNode = targetNode ? targetNode->AsNode() : nullptr;
					if(targetNiNode) {
						if(!LoadTemplate(targetNiNode, templatePath.c_str()))
							SKSE::log::error("{} - Error attaching additional skeleton info to {:08X} failed to load target path {} onto {}.", __FUNCTION__, refr->formID, templatePath.c_str(), targetNodeName.c_str());
					}
				}
			} else {
	#ifdef _DEBUG
				SKSE::log::error("{} - Error attaching additional skeleton info to {:08X} target node {} does not exist.", __FUNCTION__, refr->formID, targetNodeName.c_str());
	#endif
			}
		}
	}
}

RE::NiNode * SkeletonExtenderInterface::LoadTemplate(RE::NiNode * parent, const char * path)
{
	RE::NiNode * rootNode = nullptr;
	NifStreamWrapper niStream;

	RE::BSResourceNiBinaryStream binaryStream(path);
	if(binaryStream.good() && niStream.LoadStream(&binaryStream))
	{
		if(niStream->topObjects.size() > 0)
		{
			if(niStream->topObjects[0]) // Get the root node
				rootNode = niStream->topObjects[0].get() ? niStream->topObjects[0].get()->AsNode() : nullptr;
			if(rootNode)
				parent->AttachChild(rootNode, false);
		}
	}

	return rootNode;
}

#include "json\json.h"
#include <set>
#include <algorithm>
#include <iterator>
#include "NiTransformInterface.h"

extern NiTransformInterface	g_transformInterface;

void SkeletonExtenderInterface::AddTransforms(RE::TESObjectREFR * refr, bool isFirstPerson, RE::NiNode * skeleton, RE::NiAVObject * objectRoot)
{
	std::set<SKEEFixedString> current_nodes, previous_nodes, diffs, changes, update;

	std::uint8_t gender = 0;
	RE::TESNPC * actorBase = refr->GetBaseObject() ? refr->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	if (actorBase)
		gender = static_cast<std::uint8_t>(actorBase->GetSex());

	RE::NiStringsExtraData * globalData = netimmerse_cast<RE::NiStringsExtraData*>(FindExtraData(skeleton, "BNDT"));
	if (globalData)
	{
		for (int i = 0; i < globalData->size; i++)
		{
			SKEEFixedString node(globalData->value[i]);
			previous_nodes.insert(node);
		}
	}

	VisitObjects(skeleton, [&, isFirstPerson](RE::NiAVObject* object)
	{
		RE::NiStringExtraData * stringData = netimmerse_cast<RE::NiStringExtraData*>(object->GetExtraData("SDTA"));
		if (stringData)
		{
			try
			{
				Json::Features features;
				features.all();

				Json::Value root;
				Json::Reader reader(features);

				bool parseSuccess = reader.parse(stringData->value, root);
				if (parseSuccess)
				{
					for (auto & objects : root)
					{
						SKEEFixedString node = objects["name"].asCString();
						current_nodes.insert(node);
					}
				}
			}
			catch (...)
			{
				SKSE::log::error("{} - Error - Failed to parse skeleton transform data", __FUNCTION__);
			}
		}

		RE::NiFloatExtraData * floatData = netimmerse_cast<RE::NiFloatExtraData*>(object->GetExtraData("HH_OFFSET"));
		if (floatData)
		{
			current_nodes.insert("NPC");
		}

		return false;
	});

	// Differences here means we lost nodes
	std::set_symmetric_difference(current_nodes.begin(), current_nodes.end(),
						previous_nodes.begin(), previous_nodes.end(),
						std::inserter(diffs, diffs.begin()));

	for (auto & node : diffs)
	{
		g_transformInterface.Impl_RemoveNodeTransform(refr, isFirstPerson, gender == 1, node, "internal");
	}

	diffs.clear();

	RE::NiStringExtraData * stringData = netimmerse_cast<RE::NiStringExtraData*>(FindExtraData(objectRoot, "SDTA"));
	if (stringData)
	{
		ReadTransforms(refr, stringData->value, isFirstPerson, gender == 1, current_nodes, changes);
	}
	RE::NiFloatExtraData * floatData = netimmerse_cast<RE::NiFloatExtraData*>(FindExtraData(objectRoot, "HH_OFFSET"));
	if (floatData)
	{
		char buffer[32 + std::numeric_limits<float>::digits];
		sprintf_s(buffer, sizeof(buffer), "[{\"name\":\"NPC\",\"pos\":[0,0,%f]}]", floatData->value);
		ReadTransforms(refr, buffer, isFirstPerson, gender == 1, current_nodes, changes);
	}
	

	std::set_symmetric_difference(current_nodes.begin(), current_nodes.end(),
						previous_nodes.begin(), previous_nodes.end(),
						std::inserter(diffs, diffs.begin()));

	std::set_union(diffs.begin(), diffs.end(),
					changes.begin(), changes.end(),
					std::inserter(update, update.begin()));

	for (auto & node : update)
	{
		g_transformInterface.Impl_UpdateNodeTransforms(refr, isFirstPerson, gender == 1, node);
	}

	std::vector<RE::BSFixedString> newNodes;
	for (auto & node : current_nodes)
	{
		newNodes.push_back(node);
	}

	// Was already there, set the new current nodes. Mutate the existing BNDT
	// extra data in place (legacy SetData semantics: clear all entries, or
	// completely replace them) instead of swapping in a new object.
	if (globalData)
	{
        NiStringsExtraDataHelper::Replace(globalData, newNodes);
	}

	// No previous nodes, and we have new nodes
	if (!globalData && current_nodes.size() > 0)
	{
		std::vector<RE::BSFixedString> strings;
		for (auto& node : newNodes) {
			strings.push_back(node);
		}
		RE::NiStringsExtraData * strData = RE::NiStringsExtraData::Create("BNDT", strings);
		skeleton->AddExtraData(strData);
	}
}

void SkeletonExtenderInterface::ReadTransforms(RE::TESObjectREFR * refr, const char * jsonData, bool isFirstPerson, bool isFemale, std::set<SKEEFixedString> & nodes, std::set<SKEEFixedString> & changes)
{
	try
	{
		Json::Features features;
		features.all();

		Json::Value root;
		Json::Reader reader(features);

		bool parseSuccess = reader.parse(jsonData, root);
		if (parseSuccess)
		{

			bool changed = false;
			for (auto & objects : root)
			{
				SKEEFixedString node = objects["name"].asCString();
				nodes.insert(node);

				Json::Value pos = objects["pos"];
				if (pos.isArray() && pos.size() == 3)
				{
					float position[3];
					position[0] = pos[0].asFloat();
					position[1] = pos[1].asFloat();
					position[2] = pos[2].asFloat();

					OverrideVariant posV[3];
					float oldPosition[3];
					for (std::uint32_t i = 0; i < 3; i++)
					{
						OverrideVariant posOld = g_transformInterface.Impl_GetOverrideNodeValue(refr, isFirstPerson, isFemale, node, "internal", OverrideVariant::kParam_NodeTransformPosition, i);
						UnpackValue<float>(&oldPosition[i], &posOld);

						if (position[i] != oldPosition[i])
							changed = true;
					}

					for (std::uint32_t i = 0; i < 3; i++)
					{
						PackValue<float>(&posV[i], OverrideVariant::kParam_NodeTransformPosition, i, &position[i]);
						g_transformInterface.Impl_AddNodeTransform(refr, isFirstPerson, isFemale, node, "internal", posV[i]);
					}
				}

				Json::Value rot = objects["rot"];
				if (rot.isArray() && rot.size() == 3)
				{
					RE::NiMatrix3 rotation;
					float rotationEuler[3];
					rotationEuler[0] = RE::deg_to_rad(rot[0].asFloat());
					rotationEuler[1] = RE::deg_to_rad(rot[1].asFloat());
					rotationEuler[2] = RE::deg_to_rad(rot[2].asFloat());
					rotation.SetEulerAnglesXYZ(rotationEuler[0], rotationEuler[1], rotationEuler[2]);

					float oldRotation[9];
					for (std::uint32_t i = 0; i < 9; i++)
					{
						OverrideVariant potOld = g_transformInterface.Impl_GetOverrideNodeValue(refr, isFirstPerson, isFemale, node, "internal", OverrideVariant::kParam_NodeTransformRotation, i);
						UnpackValue<float>(&oldRotation[i], &potOld);

						if (rotation.entry[i/3][i%3] != oldRotation[i])
							changed = true;
					}

					OverrideVariant rotV[9];
					for (std::uint32_t i = 0; i < 9; i++) {
						float val = rotation.entry[i/3][i%3];
						PackValue<float>(&rotV[i], OverrideVariant::kParam_NodeTransformRotation, i, &val);
						g_transformInterface.Impl_AddNodeTransform(refr, isFirstPerson, isFemale, node, "internal", rotV[i]);
					}
				}

				Json::Value scale = objects["scale"];
				if (scale.isDouble() && scale.asFloat() > 0)
				{
					float oldScale = 0;
					OverrideVariant scaleOld = g_transformInterface.Impl_GetOverrideNodeValue(refr, isFirstPerson, isFemale, node, "internal", OverrideVariant::kParam_NodeTransformScale, 0);
					UnpackValue<float>(&oldScale, &scaleOld);

					float fScale = scale.asFloat();
					OverrideVariant scaleV;
					PackValue<float>(&scaleV, OverrideVariant::kParam_NodeTransformScale, 0, &fScale);
					g_transformInterface.Impl_AddNodeTransform(refr, isFirstPerson, isFemale, node, "internal", scaleV);

					if (fScale != oldScale)
						changed = true;
				}

				if (changed)
				{
					changes.insert(node);
				}
			}
		}
	}
	catch (...)
	{
		SKSE::log::error("{} - Error - Failed to parse skeleton transform data", __FUNCTION__);
	}
}

void SkeletonExtenderInterface::OnAttach(RE::TESObjectREFR * refr, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::NiAVObject * object, bool isFirstPerson, RE::NiNode * skeleton, RE::NiNode * root)
{
	this->Attach(refr, root, object);

	if (g_enableEquippableTransforms)
	{
		this->AddTransforms(refr, isFirstPerson, skeleton, object);
	}
}