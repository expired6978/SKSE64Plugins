#include "SkeletonExtender.h"
#include "ShaderUtilities.h"
#include "NifUtils.h"

#include "skse64/NiNodes.h"
#include "skse64/NiObjects.h"
#include "skse64/NiExtraData.h"
#include "skse64/NiSerialization.h"

#include "skse64/GameReferences.h"
#include "skse64/GameStreams.h"

void SkeletonExtender::Attach(TESObjectREFR * refr, NiNode * skeleton, NiAVObject * objectRoot)
{
	NiStringsExtraData * extraData = ni_cast(FindExtraData(objectRoot, "EXTN"), NiStringsExtraData);
	if(extraData)
	{
		if(extraData->m_size % 3 != 0) {
	#ifdef _DEBUG
			_ERROR("%s - Error attaching additional skeleton info to %08X invalid entry count, must be divisible by 3.", __FUNCTION__, refr->formID);
	#endif
			return;
		}

		for(UInt32 i = 0; i < extraData->m_size; i += 3)
		{
			BSFixedString targetNodeName = extraData->m_data[i];
			BSFixedString sourceNodeName = extraData->m_data[i+1];
			BSFixedString templatePath = extraData->m_data[i+2];

			NiAVObject * targetNode = skeleton->GetObjectByName(&targetNodeName.data);
			if(targetNode) {
				NiAVObject * sourceNode = targetNode->GetObjectByName(&sourceNodeName.data);
				if(!sourceNode) { // Make sure the source node doesn't exist
					NiNode * targetNiNode = targetNode->GetAsNiNode();
					if(targetNiNode) {
						if(!LoadTemplate(targetNiNode, templatePath.data))
							_ERROR("%s - Error attaching additional skeleton info to %08X failed to load target path %s onto %s.", __FUNCTION__, refr->formID, templatePath.data, targetNodeName.data);
					}
				}
			} else {
	#ifdef _DEBUG
				_ERROR("%s - Error attaching additional skeleton info to %08X target node %s does not exist.", __FUNCTION__, refr->formID, targetNodeName.data);
	#endif
			}
		}
	}
}

NiNode * SkeletonExtender::LoadTemplate(NiNode * parent, const char * path)
{
	NiNode * rootNode = NULL;
	UInt8 niStreamMemory[0x5B4];
	memset(niStreamMemory, 0, 0x5B4);
	NiStream * niStream = (NiStream *)niStreamMemory;
	CALL_MEMBER_FN(niStream, ctor)();

	BSResourceNiBinaryStream binaryStream(path);
	if(!binaryStream.IsValid()) {
		goto destroy_stream;
	}

	niStream->LoadStream(&binaryStream);
	if(niStream->m_rootObjects.m_data)
	{
		if(niStream->m_rootObjects.m_data[0]) // Get the root node
			rootNode = niStream->m_rootObjects.m_data[0]->GetAsNiNode();
		if(rootNode)
			parent->AttachChild(rootNode, false);
	}

destroy_stream:
	CALL_MEMBER_FN(niStream, dtor)();
	return rootNode;
}

#include "json\json.h"
#include <set>
#include <algorithm>
#include <iterator>
#include "NiTransformInterface.h"
#include "skse64/GameRTTI.h"

extern NiTransformInterface	g_transformInterface;

void SkeletonExtender::AddTransforms(TESObjectREFR * refr, bool isFirstPerson, NiNode * skeleton, NiAVObject * objectRoot)
{
	std::set<BSFixedString> current_nodes, previous_nodes, diffs, changes, update;

	UInt8 gender = 0;
	TESNPC * actorBase = DYNAMIC_CAST(refr->baseForm, TESForm, TESNPC);
	if (actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();

	NiStringsExtraData * globalData = ni_cast(FindExtraData(skeleton, "BNDT"), NiStringsExtraData);
	if (globalData)
	{
		for (int i = 0; i < globalData->m_size; i++)
		{
			BSFixedString node(globalData->m_data[i]);
			previous_nodes.insert(node);
		}
	}

	VisitObjects(skeleton, [&](NiAVObject*object)
	{
		NiStringExtraData * stringData = ni_cast(object->GetExtraData("SDTA"), NiStringExtraData);
		if (stringData)
		{
			try
			{
				Json::Features features;
				features.all();

				Json::Value root;
				Json::Reader reader(features);

				bool parseSuccess = reader.parse(stringData->m_pString, root);
				if (parseSuccess)
				{
					for (auto & objects : root)
					{
						BSFixedString node = objects["name"].asCString();
						current_nodes.insert(node);
					}
				}
			}
			catch (...)
			{
				_ERROR("%s - Error - Failed to parse skeleton transform data", __FUNCTION__);
			}
		}

		NiFloatExtraData * floatData = ni_cast(object->GetExtraData("HH_OFFSET"), NiFloatExtraData);
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
		g_transformInterface.RemoveNodeTransform(refr, isFirstPerson, gender == 1, node, "internal");
	}

	diffs.clear();

	NiStringExtraData * stringData = ni_cast(FindExtraData(objectRoot, "SDTA"), NiStringExtraData);
	if (stringData)
	{
		ReadTransforms(refr, stringData->m_pString, isFirstPerson, gender == 1, current_nodes, changes);
	}
	NiFloatExtraData * floatData = ni_cast(FindExtraData(objectRoot, "HH_OFFSET"), NiFloatExtraData);
	if (floatData)
	{
		char buffer[32 + std::numeric_limits<float>::digits];
		sprintf_s(buffer, sizeof(buffer), "[{\"name\":\"NPC\",\"pos\":[0,0,%f]}]", floatData->m_data);
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
		g_transformInterface.UpdateNodeTransforms(refr, isFirstPerson, gender == 1, node);
	}

	std::vector<BSFixedString> newNodes;
	for (auto & node : current_nodes)
	{
		newNodes.push_back(node);
	}

	// Was already there, set the new current nodes
	if (globalData)
	{
		if (newNodes.size() == 0)
		{
			globalData->SetData(nullptr, 0);
		}
		else
		{
			globalData->SetData(&newNodes.at(0), newNodes.size());
		}
		
	}

	// No previous nodes, and we have new nodes
	if (!globalData && current_nodes.size() > 0)
	{
		NiStringsExtraData * strData = NiStringsExtraData::Create("BNDT", &newNodes.at(0), newNodes.size());
		skeleton->AddExtraData(strData);
	}
}

void SkeletonExtender::ReadTransforms(TESObjectREFR * refr, const char * jsonData, bool isFirstPerson, bool isFemale, std::set<BSFixedString> & nodes, std::set<BSFixedString> & changes)
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
				BSFixedString node = objects["name"].asCString();
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
					for (UInt32 i = 0; i < 3; i++)
					{
						OverrideVariant posOld = g_transformInterface.GetOverrideNodeValue(refr, isFirstPerson, isFemale, node, "internal", OverrideVariant::kParam_NodeTransformPosition, i);
						UnpackValue<float>(&oldPosition[i], &posOld);

						if (position[i] != oldPosition[i])
							changed = true;
					}

					for (UInt32 i = 0; i < 3; i++)
					{
						PackValue<float>(&posV[i], OverrideVariant::kParam_NodeTransformPosition, i, &position[i]);
						g_transformInterface.AddNodeTransform(refr, isFirstPerson, isFemale, node, "internal", posV[i]);
					}
				}

				Json::Value rot = objects["rot"];
				if (pos.isArray() && pos.size() == 3)
				{
					NiMatrix33 rotation;
					float rotationEuler[3];
					rotationEuler[0] = rot[0].asFloat() * MATH_PI / 180;
					rotationEuler[1] = rot[1].asFloat() * MATH_PI / 180;
					rotationEuler[2] = rot[2].asFloat() * MATH_PI / 180;
					rotation.SetEulerAngles(rotationEuler[0], rotationEuler[1], rotationEuler[2]);

					float oldRotation[9];
					for (UInt32 i = 0; i < 9; i++)
					{
						OverrideVariant potOld = g_transformInterface.GetOverrideNodeValue(refr, isFirstPerson, isFemale, node, "internal", OverrideVariant::kParam_NodeTransformRotation, i);
						UnpackValue<float>(&oldRotation[i], &potOld);

						if (rotation.arr[i] != oldRotation[i])
							changed = true;
					}

					OverrideVariant rotV[9];
					for (UInt32 i = 0; i < 9; i++) {
						PackValue<float>(&rotV[i], OverrideVariant::kParam_NodeTransformRotation, i, &rotation.arr[i]);
						g_transformInterface.AddNodeTransform(refr, isFirstPerson, isFemale, node, "internal", rotV[i]);
					}
				}

				Json::Value scale = objects["scale"];
				if (scale.isDouble() && scale.asFloat() > 0)
				{
					float oldScale = 0;
					OverrideVariant scaleOld = g_transformInterface.GetOverrideNodeValue(refr, isFirstPerson, isFemale, node, "internal", OverrideVariant::kParam_NodeTransformScale, 0);
					UnpackValue<float>(&oldScale, &scaleOld);

					float fScale = scale.asFloat();
					OverrideVariant scaleV;
					PackValue<float>(&scaleV, OverrideVariant::kParam_NodeTransformScale, 0, &fScale);
					g_transformInterface.AddNodeTransform(refr, isFirstPerson, isFemale, node, "internal", scaleV);

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
		_ERROR("%s - Error - Failed to parse skeleton transform data", __FUNCTION__);
	}
}