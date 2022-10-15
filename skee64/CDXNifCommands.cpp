#include "CDXNifCommands.h"
#include "CDXNifMesh.h"
#include "CDXNifScene.h"

#include "skse64/GameData.h"
#include "skse64/GameReferences.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameMenus.h"

#include "skse64/NiGeometry.h"
#include "skse64/NiExtraData.h"

#include "FaceMorphInterface.h"
#include "FileUtils.h"
#include "NifUtils.h"
#include "SKEEHooks.h"

#include "skse64/ScaleformCallbacks.h"

#include "skse64/PluginAPI.h"

extern FaceMorphInterface	g_morphInterface;
extern SKSETaskInterface	* g_task;
extern CDXNifScene			g_World;

using namespace DirectX;

void ApplyMorphData(BSTriShape * geometry, CDXVectorMap & vectorMap, float multiplier)
{
	Actor * actor = g_World.GetWorkingActor();
	TESNPC * npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);

	// Create mapped extra vertex data for NPC
	auto sculptTarget = g_morphInterface.GetSculptTarget(npc, true);
	if (sculptTarget) {
		std::string headPartName = geometry->m_name;
		BGSHeadPart * headPart = GetHeadPartByName(headPartName);
		if (headPart) {
			auto sculptHost = sculptTarget->GetSculptHost(SculptData::GetHostByPart(headPart), true);
			if (sculptHost) {
				BSFaceGenBaseMorphExtraData * morphData = (BSFaceGenBaseMorphExtraData *)geometry->GetExtraData("FOD");
				if (morphData) {
					for (auto it : vectorMap) {
						// Store it in the NPC mapped data
						NiPoint3 temp = *(NiPoint3*)&it.second;
						temp *= multiplier;

						sculptHost->add(std::make_pair(it.first, temp));

						// Write it to FaceGen
						morphData->vertexData[it.first] += temp;
					}

					// Update FaceGen
					if (g_task)
						g_task->AddTask(new CRGNTaskUpdateModel(geometry));
				}
			}
		}
	}
}

void AddStrokeCommand(CDXStroke * stroke, BSTriShape * geometry, SInt32 id)
{
	if (g_task)
		g_task->AddUITask(new CRGNUITaskAddStroke(stroke, geometry, id));
}

void CDXNifInflateStroke::Undo()
{
	CDXInflateStroke::Undo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifInflateStroke::Redo()
{
	CDXInflateStroke::Redo();
	CDXLegacyNifMesh * nifMesh = static_cast<CDXLegacyNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifDeflateStroke::Undo()
{
	CDXDeflateStroke::Undo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifDeflateStroke::Redo()
{
	CDXDeflateStroke::Redo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifSmoothStroke::Undo()
{
	CDXSmoothStroke::Undo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}
void CDXNifSmoothStroke::Redo()
{
	CDXSmoothStroke::Redo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifMoveStroke::Undo()
{
	CDXMoveStroke::Undo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifMoveStroke::Redo()
{
	CDXMoveStroke::Redo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifInflateStroke::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		AddStrokeCommand(this, geometry, i);
	}
}

void CDXNifDeflateStroke::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		AddStrokeCommand(this, geometry, i);
	}
}

void CDXNifSmoothStroke::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		AddStrokeCommand(this, geometry, i);
	}	
}

void CDXNifMoveStroke::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		AddStrokeCommand(this, geometry, i);
	}
}

void CDXNifMaskAddStroke::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		AddStrokeCommand(this, geometry, i);
	}
}

void CDXNifMaskSubtractStroke::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		AddStrokeCommand(this, geometry, i);
	}
}


void CDXNifResetMask::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		if (g_task)
			g_task->AddUITask(new CRGNUITaskStandardCommand(this, geometry, i));
	}
}

CRGNTaskUpdateModel::CRGNTaskUpdateModel(BSTriShape * geometry)
{
	m_geometry = geometry;
}

void CRGNTaskUpdateModel::Run()
{
	if (m_geometry)
		UpdateModelFace(m_geometry);
}

void CRGNTaskUpdateModel::Dispose()
{
	delete this;
}

CRGNUITaskAddStroke::CRGNUITaskAddStroke(CDXStroke * stroke, BSTriShape * geometry, SInt32 id)
{
	m_id = id;
	m_stroke = stroke;
	m_geometry = geometry;
}

void CRGNUITaskAddStroke::Dispose()
{
	delete this;
}

void CRGNUITaskAddStroke::Run()
{
	IMenu * menu = MenuManager::GetSingleton()->GetMenu(&UIStringHolder::GetSingleton()->raceSexMenu);
	if (menu && menu->view) {
		FxResponseArgs<1> args;
		menu->view->CreateObject(&args.args[1]);
		GFxValue & obj = args.args[1];
		GFxValue commandId;
		commandId.SetNumber(m_id);
		obj.SetMember("id", &commandId);
		GFxValue type;
		type.SetNumber(m_stroke->GetUndoType());
		obj.SetMember("type", &type);
		GFxValue strokeType;
		strokeType.SetNumber(m_stroke->GetStrokeType());
		obj.SetMember("stroke", &strokeType);
		GFxValue vertices;
		vertices.SetNumber(m_stroke->Length());
		obj.SetMember("vertices", &vertices);
		GFxValue mirror;
		mirror.SetBool(m_stroke->IsMirror());
		obj.SetMember("mirror", &mirror);
		GFxValue partName;
		partName.SetString(m_geometry->m_name);
		obj.SetMember("part", &partName);
		InvokeFunction(menu->view, "AddAction", &args);
	}
}

CRGNUITaskStandardCommand::CRGNUITaskStandardCommand(CDXUndoCommand * cmd, BSTriShape * geometry, SInt32 id)
{
	m_id = id;
	m_cmd = cmd;
	m_geometry = geometry;
}

void CRGNUITaskStandardCommand::Dispose()
{
	delete this;
}

void CRGNUITaskStandardCommand::Run()
{
	IMenu * menu = MenuManager::GetSingleton()->GetMenu(&UIStringHolder::GetSingleton()->raceSexMenu);
	if (menu && menu->view) {
		FxResponseArgs<1> args;
		menu->view->CreateObject(&args.args[1]);
		GFxValue & obj = args.args[1];
		GFxValue commandId;
		commandId.SetNumber(m_id);
		obj.SetMember("id", &commandId);
		GFxValue type;
		type.SetNumber(m_cmd->GetUndoType());
		obj.SetMember("type", &type);
		GFxValue partName;
		partName.SetString(m_geometry->m_name);
		obj.SetMember("part", &partName);
		InvokeFunction(menu->view, "AddAction", &args);
	}
}

CDXNifResetSculpt::CDXNifResetSculpt(CDXNifMesh * mesh) : CDXUndoCommand()
{
	m_mesh = mesh;

	Actor * actor = g_World.GetWorkingActor();
	TESNPC * npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
	BSTriShape * geometry = m_mesh->GetGeometry();
	if (geometry) {
		// Create mapped extra vertex data for NPC
		auto sculptTarget = g_morphInterface.GetSculptTarget(npc, false);
		if (sculptTarget) {
			std::string headPartName = geometry->m_name;
			BGSHeadPart * headPart = GetHeadPartByName(headPartName);
			if (headPart) {
				auto sculptHost = sculptTarget->GetSculptHost(SculptData::GetHostByPart(headPart), false);
				if (sculptHost) {
					CDXMeshVert* pVertices = m_mesh->LockVertices(CDXMesh::LockMode::WRITE);
					if (pVertices) {

						for (auto it : *sculptHost) {
							// Skip masked vertices
							if (XMVector3Equal(XMLoadFloat3(&pVertices[it.first].Color), COLOR_SELECTED))
								continue;
							// Store it in the NPC mapped data
							auto delta = XMLoadFloat3((const XMFLOAT3*)&it.second);
							XMStoreFloat3(&pVertices[it.first].Position, XMVectorSubtract(XMLoadFloat3(&pVertices[it.first].Position), delta));
							m_current.emplace(it.first, XMVectorNegate(delta));
						}

						m_mesh->UnlockVertices(CDXMesh::LockMode::WRITE);
					}
				}
			}
		}
	}
}

CDXNifResetSculpt::~CDXNifResetSculpt()
{
	m_current.clear();
}

CDXUndoCommand::UndoType CDXNifResetSculpt::GetUndoType()
{
	return kUndoType_ResetSculpt;
}

void CDXNifResetSculpt::Redo()
{
	CDXMeshVert* pVertices = m_mesh->LockVertices(CDXMesh::LockMode::WRITE);
	if (!pVertices)
		return;

	// Do what we have now
	for (auto it : m_current)
		XMStoreFloat3(&pVertices[it.first].Position, XMVectorAdd(XMLoadFloat3(&pVertices[it.first].Position), it.second));

	m_mesh->UnlockVertices(CDXMesh::LockMode::WRITE);

	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifResetSculpt::Undo()
{
	CDXMeshVert* pVertices = m_mesh->LockVertices(CDXMesh::LockMode::WRITE);
	if (!pVertices)
		return;

	// Undo what we did
	for (auto it : m_current)
		XMStoreFloat3(&pVertices[it.first].Position, XMVectorSubtract(XMLoadFloat3(&pVertices[it.first].Position), it.second));

	m_mesh->UnlockVertices(CDXMesh::LockMode::WRITE);

	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifResetSculpt::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		if (g_task)
			g_task->AddUITask(new CRGNUITaskStandardCommand(this, geometry, i));
	}
}

CDXNifImportGeometry::CDXNifImportGeometry(CDXNifMesh * mesh, NiAVObject * source) : CDXUndoCommand()
{
	m_mesh = mesh;

	Actor * actor = g_World.GetWorkingActor();
	TESNPC * npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
	BSTriShape * target = m_mesh->GetGeometry();
	if (target) {
		// Create mapped extra vertex data for NPC
		auto sculptTarget = g_morphInterface.GetSculptTarget(npc, true);
		if (sculptTarget) {
			std::string headPartName = target->m_name;
			BGSHeadPart * headPart = GetHeadPartByName(headPartName);
			if (headPart) {
				auto sculptHost = sculptTarget->GetSculptHost(SculptData::GetHostByPart(headPart), true);
				if (sculptHost) {
					UInt32 srcNumVertices = 0;
					UInt32 dstNumVertices = target->numVertices;

					NiPoint3 * srcGeometry = nullptr;
					NiPoint3 * dstGeometry = nullptr;

					UInt32 srcStride = 0;
					UInt32 dstStride = 0;

					NiTransform srcTransform;
					NiTransform dstTransform;

					SimpleLock * srcLock = nullptr;
					SimpleLock * dstLock = nullptr;

					BSDynamicTriShape * dstDynamicShape = ni_cast(target, BSDynamicTriShape);
					if (dstDynamicShape) {
						dstGeometry = reinterpret_cast<NiPoint3*>(dstDynamicShape->pDynamicData);
						dstStride = sizeof(XMFLOAT4);
						dstLock = &dstDynamicShape->lock;
						dstTransform = GetGeometryTransform(dstDynamicShape);
					}

					NiGeometry * legacyGeometry = source->GetAsNiGeometry();
					if (legacyGeometry) {
						NiTriShapeData * srcData = niptr_cast<NiTriShapeData>(legacyGeometry->m_spModelData);
						srcNumVertices = srcData->m_usVertices;
						srcGeometry = reinterpret_cast<NiPoint3*>(srcData->m_pkVertex);
						srcStride = sizeof(NiPoint3);
						srcTransform = GetLegacyGeometryTransform(legacyGeometry);
					}
					BSTriShape * sourceGeometry = source->GetAsBSTriShape();
					if (sourceGeometry) {
						srcTransform = GetGeometryTransform(sourceGeometry);
						srcNumVertices = sourceGeometry->numVertices;

						BSDynamicTriShape * srcDynamicShape = ni_cast(sourceGeometry, BSDynamicTriShape);
						if (srcDynamicShape) {
							srcGeometry = reinterpret_cast<NiPoint3*>(srcDynamicShape->pDynamicData);
							srcLock = &srcDynamicShape->lock;
							srcStride = sizeof(XMFLOAT4);
						}
						else {
							const NiSkinInstance * skinInstance = sourceGeometry->m_spSkinInstance.m_pObject;
							const NiSkinPartition * skinPartition = skinInstance ? skinInstance->m_spSkinPartition.m_pObject : nullptr;
							srcGeometry = skinPartition ? reinterpret_cast<NiPoint3*>(&skinPartition->m_pkPartitions[0].shapeData->m_RawVertexData) : nullptr;
							srcStride = NiSkinPartition::GetVertexSize(sourceGeometry->vertexDesc);
						}
					}

					if (srcLock) srcLock->Lock();
					if (dstLock) dstLock->Lock();

					if (srcNumVertices == dstNumVertices && srcGeometry && dstGeometry) {
						CDXMeshVert* pVertices = m_mesh->LockVertices(CDXMesh::LockMode::WRITE);
						if (pVertices) {
							for (UInt32 i = 0; i < srcNumVertices; i++) {
								// Skip masked vertices
								if (XMVector3Equal(XMLoadFloat3(&pVertices[i].Color), COLOR_SELECTED))
									continue;

								NiPoint3* srcVertex = reinterpret_cast<NiPoint3*>(reinterpret_cast<UInt8*>(srcGeometry) + (srcStride * i));
								NiPoint3* dstVertex = reinterpret_cast<NiPoint3*>(reinterpret_cast<UInt8*>(dstGeometry) + (dstStride * i));

								NiPoint3 diff = (srcTransform * (*srcVertex)) - (dstTransform * (*dstVertex));
								XMVECTOR diffVector = XMLoadFloat3(reinterpret_cast<XMFLOAT3*>(&diff));

								XMStoreFloat3(&pVertices[i].Position, XMVectorAdd(XMLoadFloat3(&pVertices[i].Position), diffVector));

								m_current.emplace(i, diffVector);
							}

							m_mesh->UnlockVertices(CDXMesh::LockMode::WRITE);
						}
					}

					if (srcLock) srcLock->Release();
					if (dstLock) dstLock->Release();
				}
			}
		}
	}
}

CDXNifImportGeometry::~CDXNifImportGeometry()
{
	m_current.clear();
}

CDXUndoCommand::UndoType CDXNifImportGeometry::GetUndoType()
{
	return kUndoType_Import;
}

void CDXNifImportGeometry::Redo()
{
	CDXMeshVert* pVertices = m_mesh->LockVertices(CDXMesh::LockMode::WRITE);
	if (!pVertices)
		return;

	// Do what we have now
	for (auto it : m_current)
		XMStoreFloat3(&pVertices[it.first].Position, XMVectorAdd(XMLoadFloat3(&pVertices[it.first].Position), it.second));

	m_mesh->UnlockVertices(CDXMesh::LockMode::WRITE);

	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifImportGeometry::Undo()
{
	CDXMeshVert* pVertices = m_mesh->LockVertices(CDXMesh::LockMode::WRITE);
	if (!pVertices)
		return;

	// Undo what we did
	for (auto it : m_current)
		XMStoreFloat3(&pVertices[it.first].Position, XMVectorSubtract(XMLoadFloat3(&pVertices[it.first].Position), it.second));

	m_mesh->UnlockVertices(CDXMesh::LockMode::WRITE);

	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifImportGeometry::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		if (g_task)
			g_task->AddUITask(new CRGNUITaskStandardCommand(this, geometry, i));
	}
}
