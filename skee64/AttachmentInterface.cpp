#include "AttachmentInterface.h"
#include "BodyMorphInterface.h"
#include "OverrideInterface.h"

#include "skse64/GameReferences.h"
#include "skse64/GameStreams.h"
#include "skse64/GameObjects.h"
#include "skse64/GameRTTI.h"

#include "skse64/NiNodes.h"
#include "skse64/NiGeometry.h"

#include "NifUtils.h"

extern SKSETaskInterface* g_task;
extern AttachmentInterface	g_attachmentInterface;
extern BodyMorphInterface	g_bodyMorphInterface;
extern OverrideInterface	g_overrideInterface;
const char* AttachmentInterface::ATTACHMENT_HOLDER = "NiAttachments [NiOA]";

SKSEAttachSkinnedMesh::SKSEAttachSkinnedMesh(TESObjectREFR* ref, const BSFixedString& nifPath, const BSFixedString& name, bool firstPerson, bool replace, const std::vector<BSFixedString>& filter)
	: m_formId(ref->formID)
	, m_nifPath(nifPath)
	, m_name(name)
	, m_firstPerson(firstPerson)
	, m_replace(replace)
	, m_filter(filter)
{

}

void SKSEDetachSkinnedMesh::Run()
{
	TESForm* form = LookupFormByID(m_formId);
	if (!form) {
		return;
	}

	if (form->formType != Character::kTypeID && form->formType != TESObjectREFR::kTypeID) {
		return;
	}

	g_attachmentInterface.DetachMesh(static_cast<TESObjectREFR*>(form), m_name.c_str(), m_firstPerson);
}

SKSEDetachSkinnedMesh::SKSEDetachSkinnedMesh(TESObjectREFR* ref, const BSFixedString& name, bool firstPerson)
	: m_formId(ref->formID)
	, m_name(name)
	, m_firstPerson(firstPerson)
{

}


void SKSEAttachSkinnedMesh::Run()
{
	TESForm* form = LookupFormByID(m_formId);
	if (!form) {
		return;
	}

	if (form->formType != Character::kTypeID && form->formType != TESObjectREFR::kTypeID) {
		return;
	}

	TESObjectREFR* reference = static_cast<TESObjectREFR*>(form);
	NiAVObject* outRoot = nullptr;

	const char** filter = nullptr;
	if (m_filter.size())
	{
		filter = new const char* [m_filter.size()];
		for (size_t i = 0; i < m_filter.size(); ++i)
		{
			filter[i] = m_filter[i].c_str();
		}
	}

	if (g_attachmentInterface.AttachMesh(reference, m_nifPath.c_str(), m_name.c_str(), m_firstPerson, m_replace, filter, m_filter.size(), outRoot, nullptr, 0) && form->formType == Character::kTypeID)
	{
		g_bodyMorphInterface.ApplyVertexDiff(reference, outRoot);
		g_overrideInterface.Impl_ApplyNodeOverrides(reference, outRoot, true);
	}

	if (filter)
	{
		delete[] filter;
	}
}

void SKSEDetachAllSkinnedMeshes::Run()
{
	TESForm* form = LookupFormByID(m_formId);
	if (!form) {
		return;
	}

	if (form->formType != Character::kTypeID && form->formType != TESObjectREFR::kTypeID) {
		return;
	}

	TESObjectREFR* reference = static_cast<TESObjectREFR*>(form);

	VisitSkeletalRoots(reference, [&](NiNode* rootNode, bool isFirstPerson)
	{
		BSFixedString parentName("NPC Root [Root]");
		NiAVObject* destination = rootNode->GetObjectByName(&parentName.data);
		if (destination) {
			NiNode* destinationNode = destination->GetAsNiNode();
			if (destinationNode) {
				BSFixedString holderName(AttachmentInterface::ATTACHMENT_HOLDER);
				auto holderNode = destinationNode->GetObjectByName(&holderName.data);
				if (holderNode) {
					destinationNode->RemoveChild(holderNode);
				}
			}
		}
	});
}


void AttachmentInterface::Revert()
{
	if (g_task)
	{
		m_attachedLock.lock();
		for (auto& formId : m_attached)
		{
			g_task->AddTask(new SKSEDetachAllSkinnedMeshes(formId));
		}
		m_attachedLock.unlock();
	}
}

bool AttachmentInterface::AttachMesh(TESObjectREFR* ref, const char* path, const char* nodeName, bool firstPerson, bool replace, const char** filter, skee_u32 filterSize, NiAVObject*& out, char* errBuf, skee_u64 errBufLen)
{
	NiNode* root = ref->GetNiRootNode(firstPerson ? 1 : 0);
	if (!root) {
		return false;
	}

	float weight = 100.0;
	TESNPC* npc = DYNAMIC_CAST(ref->baseForm, TESForm, TESNPC);
	if (npc && npc->nextTemplate) {
		TESNPC* temp = npc->GetRootTemplate();
		if (temp) {
			weight = temp->weight;
		}
	}
	else
		weight = CALL_MEMBER_FN(ref, GetWeight)();
	weight /= 100.0;

	DirectX::XMVECTOR weightScale = DirectX::XMVectorReplicate(weight);

	BSFixedString parentName("NPC Root [Root]");
	NiAVObject* rootDestination = root->GetObjectByName(&parentName.data);
	if (!rootDestination) {
		return false;
	}

	NiNode* rootNode = rootDestination->GetAsNiNode();
	if (!rootNode) {
		return false;
	}

	bool createdHolder = false;
	BSFixedString holderName(ATTACHMENT_HOLDER);

	NiPointer<NiAVObject> holder = rootDestination->GetObjectByName(&holderName.data);
	if (!holder)
	{
		holder = NiNode::Create(0);
		holder->m_name = holderName.data;
		createdHolder = true;
	}

	NiNode* holderNode = holder->GetAsNiNode();
	if (!holderNode) {
		return false;
	}

	BSFixedString targetNodeName(nodeName);
	NiAVObject* target = holderNode->GetObjectByName(&targetNodeName.data);
	if (target) {
		if (replace)
		{
			holderNode->RemoveChild(target);
		}
		else
		{
			// Nothing to do, the mesh is already here
			return true;
		}
	}
	
	BSFixedString nifPath(path);
	NifStreamWrapper niStream;
	BSResourceNiBinaryStream binaryStream(nifPath.data);
	if (!binaryStream.IsValid()) {
		std::snprintf(errBuf, errBufLen, "No file exists at (%s)\n", path);
		return false;
	}

	if (!niStream.LoadStream(&binaryStream)) {
		std::snprintf(errBuf, errBufLen, "File at (%s) is not a valid nif\n", path);
		return false;
	}

	// Try searching for the _0 version of the mesh
	NifStreamWrapper lowStream;
	std::string lowPath(path);
	NiNode* lowRoot = nullptr;
	size_t foundWeight = lowPath.find("_1");
	if (foundWeight != std::string::npos)
	{
		lowPath[foundWeight + 1] = '0';
		BSFixedString lowFile(lowPath.c_str());

		BSResourceNiBinaryStream lowBinaryStream(lowFile.data);
		if (lowBinaryStream.IsValid() && lowStream.LoadStream(&lowBinaryStream)) {
			lowStream.VisitObjects([&lowRoot](NiObject* object)
			{
				if (object->GetAsNiNode()) {
					lowRoot = object->GetAsNiNode();
					return true;
				}

				return false;
			});
		}
	}

	std::unordered_set<BSFixedString> filteredNames;
	for (UInt32 i = 0; i < filterSize; ++i)
	{
		filteredNames.emplace(filter[i]);
	}

	NiNode* loadedRoot = nullptr;
	niStream.VisitObjects([&loadedRoot](NiObject* object)
	{
		if (object->GetAsNiNode()) {
			loadedRoot = object->GetAsNiNode();
			return true;
		}

		return false;
	});

	if (!loadedRoot) {
		return false;
	}

	NiPointer<NiNode> targetRoot = NiNode::Create(0);
	targetRoot->m_name = targetNodeName.data;

	int c = 0;
	if (VisitGeometry(loadedRoot, [&](BSGeometry* geometry)
	{
		if (filteredNames.count(geometry->m_name))
		{
			return false;
		}

		targetRoot->AttachChild(geometry, true);

		auto skinInstance = geometry->m_spSkinInstance;
		if (skinInstance) {
			auto skinData = skinInstance->m_spSkinData;
			if (!skinData) {
				c += std::snprintf(errBuf + c, std::max(0LL, static_cast<SInt64>(errBufLen) - c), "Error attaching skinned mesh to ref (%08X) nif (%s) shape (%s) has no skin data\n", ref->formID, path, geometry->m_name);
				return true;
			}

			auto skinPartition = skinInstance->m_spSkinPartition;
			if (skinPartition && lowRoot)
			{
				NiAVObject* weightedGeom = lowRoot->GetObjectByName(&geometry->m_name);
				if (weightedGeom) {
					BSGeometry* lowGeometry = weightedGeom->GetAsBSGeometry();
					if (lowGeometry) {
						auto lowSkinInstance = lowGeometry->m_spSkinInstance;
						if (lowSkinInstance) {
							auto lowSkinPartition = lowSkinInstance->m_spSkinPartition;
							if (lowSkinPartition) {
								UInt32 hiVerts = skinPartition->vertexCount;
								UInt32 loVerts = lowSkinPartition->vertexCount;

								if (geometry->vertexDesc == lowGeometry->vertexDesc && hiVerts == loVerts) {
									bool hasVertices = (NiSkinPartition::GetVertexFlags(geometry->vertexDesc) & VertexFlags::VF_VERTEX) == VertexFlags::VF_VERTEX;
									if (hasVertices) {
										UInt32 vertexSize = NiSkinPartition::GetVertexSize(geometry->vertexDesc);
										UInt8* hiBlock = reinterpret_cast<UInt8*>(skinPartition->m_pkPartitions[0].shapeData->m_RawVertexData);
										UInt8* loBlock = reinterpret_cast<UInt8*>(lowSkinPartition->m_pkPartitions[0].shapeData->m_RawVertexData);
										for (UInt32 i = 0; i < hiVerts; ++i)
										{
											DirectX::XMVECTOR hiVertex = *reinterpret_cast<DirectX::XMVECTOR*>(&hiBlock[i * vertexSize]);
											DirectX::XMVECTOR loVertex = *reinterpret_cast<DirectX::XMVECTOR*>(&loBlock[i * vertexSize]);
											DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&hiBlock[i * vertexSize]), DirectX::XMVectorMultiplyAdd(DirectX::XMVectorSubtract(hiVertex, loVertex), weightScale, loVertex));
										}

										// Update the resources and propagate to duplicate partitions
										NIOVTaskUpdateSkinPartition update(skinInstance, skinPartition);
										update.Run();
									}
								}
							}
						}
					}
				}
			}

			for (UInt32 i = 0; i < skinData->m_uiBones; ++i)
			{
				auto bone = skinInstance->m_ppkBones[i];
				if (!bone) {
					c += std::snprintf(errBuf + c, std::max(0LL, static_cast<SInt64>(errBufLen) - c), "Error attaching skinned mesh to ref (%08X) nif (%s) shape (%s) has invalid bone at index (%d)\n", ref->formID, path, geometry->m_name, i);
					return true;
				}

				auto boneNode = root->GetObjectByName(&bone->m_name);
				if (!boneNode) {
					c += std::snprintf(errBuf + c, std::max(0LL, static_cast<SInt64>(errBufLen) - c), "Error attaching skinned mesh to ref (%08X) nif (%s) shape (%s) missing bone (%s) on ref root skeleton\n", ref->formID, path, geometry->m_name, bone->m_name);
					return true;
				}

				skinInstance->m_ppkBones[i] = boneNode;
				skinInstance->m_worldTransforms[i] = &boneNode->m_worldTransform;
				skinInstance->m_pkRootParent = root;
			}
		}

		return false;
	}))
	{
		return false;
	}

	holderNode->AttachChild(targetRoot, false);
	if (createdHolder)
	{
		rootNode->AttachChild(holderNode, false);
	}

	m_attachedLock.lock();
	m_attached.emplace(ref->formID);
	m_attachedLock.unlock();

	out = targetRoot;
	return true;
}

bool AttachmentInterface::DetachMesh(TESObjectREFR* ref, const char* nodeName, bool firstPerson)
{
	NiNode* root = ref->GetNiRootNode(firstPerson ? 1 : 0);
	if (!root) {
		return false;
	}

	BSFixedString parentName("NPC Root [Root]");
	NiAVObject* destination = root->GetObjectByName(&parentName.data);
	if (!destination) {
		return false;
	}

	BSFixedString holderName(ATTACHMENT_HOLDER);
	destination = destination->GetObjectByName(&holderName.data);
	if (!destination) {
		return false;
	}

	NiNode* destinationNode = destination->GetAsNiNode();
	if (!destinationNode) {
		return false;
	}

	BSFixedString targetNodeName(nodeName);
	NiAVObject* target = destinationNode->GetObjectByName(&targetNodeName.data);
	if (!target) {
		return false;
	}

	destinationNode->RemoveChild(target);
	return true;
}


