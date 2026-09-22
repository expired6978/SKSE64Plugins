#include "CDXNifCommands.h"
#include "SKEETasks.h"
#include "SculptTrace.h"
#include "SculptHistoryDispatch.h"
#include "CDXNifMesh.h"
#include "CDXNifScene.h"

#include "RE/N/NiGeometry.h"
#include "RE/N/NiExtraData.h"

#include "FaceMorphInterface.h"
#include "FileUtils.h"
#include "NifUtils.h"
#include "SKEEHooks.h"


#include <cstdint>

extern FaceMorphInterface	g_morphInterface;
extern const SKSE::TaskInterface* g_task;
extern CDXNifScene			g_World;

using namespace DirectX;

void ApplyMorphData(RE::BSTriShape * geometry, CDXVectorMap & vectorMap, float multiplier)
{
	RE::Actor * actor = g_World.GetWorkingActor();
	RE::TESNPC * npc = actor->GetBaseObject() ? actor->GetBaseObject()->As<RE::TESNPC>() : nullptr;

	// Create mapped extra vertex data for NPC
	auto sculptTarget = g_morphInterface.GetSculptTarget(npc, true);
	if (sculptTarget) {
		std::string headPartName = geometry->name.c_str();
		RE::BGSHeadPart * headPart = GetHeadPartByName(headPartName);
		if (headPart) {
			auto sculptHost = sculptTarget->GetSculptHost(SculptData::GetHostByPart(headPart), true);
			if (sculptHost) {
				RE::BSFaceGenBaseMorphExtraData * morphData = (RE::BSFaceGenBaseMorphExtraData *)geometry->GetExtraData("FOD");
				if (morphData) {
					for (auto it : vectorMap) {
						// Store it in the NPC mapped data
						RE::NiPoint3 temp = *(RE::NiPoint3*)&it.second;
						temp *= multiplier;

						sculptHost->add(std::make_pair(it.first, temp));

						// Write it to FaceGen
						morphData->vertexData[it.first] += temp;
					}

					// Update FaceGen
					if (g_task)
						SKEE_AddTask(g_task, new CRGNTaskUpdateModel(geometry));
				}
			}
		}
	}
}

void AddStrokeCommand(CDXStroke * stroke, RE::BSTriShape * geometry, std::int32_t id)
{
	if (g_task) {
		SKEE::SculptTrace::Count(SKEE::SculptTrace::Event::HistoryQueued);
		SKEE_AddUITask(g_task, new CRGNUITaskAddStroke(stroke, geometry, id));
	} else SKEE::SculptTrace::Count(SKEE::SculptTrace::Event::HistoryNoTask);
}

void CDXNifInflateStroke::Undo()
{
	CDXInflateStroke::Undo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifInflateStroke::Redo()
{
	CDXInflateStroke::Redo();
	CDXLegacyNifMesh * nifMesh = static_cast<CDXLegacyNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifDeflateStroke::Undo()
{
	CDXDeflateStroke::Undo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifDeflateStroke::Redo()
{
	CDXDeflateStroke::Redo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifSmoothStroke::Undo()
{
	CDXSmoothStroke::Undo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}
void CDXNifSmoothStroke::Redo()
{
	CDXSmoothStroke::Redo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifMoveStroke::Undo()
{
	CDXMoveStroke::Undo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifMoveStroke::Redo()
{
	CDXMoveStroke::Redo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifInflateStroke::Apply(std::int32_t i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		AddStrokeCommand(this, geometry, i);
	}
}

void CDXNifDeflateStroke::Apply(std::int32_t i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		AddStrokeCommand(this, geometry, i);
	}
}

void CDXNifSmoothStroke::Apply(std::int32_t i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		AddStrokeCommand(this, geometry, i);
	}	
}

void CDXNifMoveStroke::Apply(std::int32_t i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		AddStrokeCommand(this, geometry, i);
	}
}

void CDXNifMaskAddStroke::Apply(std::int32_t i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		AddStrokeCommand(this, geometry, i);
	}
}

void CDXNifMaskSubtractStroke::Apply(std::int32_t i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		AddStrokeCommand(this, geometry, i);
	}
}

void CDXNifResetMask::Apply(std::int32_t i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		if (g_task)
			SKEE_AddUITask(g_task, new CRGNUITaskStandardCommand(this, geometry, i));
	}
}

CRGNTaskUpdateModel::CRGNTaskUpdateModel(RE::BSTriShape * geometry)
{
	m_geometry.reset(geometry);
}

void CRGNTaskUpdateModel::Run()
{
	if (m_geometry)
		SKEE::UpdateModelFace(m_geometry.get());
}

void CRGNTaskUpdateModel::Dispose()
{
	delete this;
}

CRGNUITaskAddStroke::CRGNUITaskAddStroke(CDXStroke * stroke, RE::BSTriShape * geometry, std::int32_t id)
{
	m_id = id;
	m_editorGeneration = g_World.GetEditorGeneration();
	m_undoType = stroke->GetUndoType();
	m_strokeType = stroke->GetStrokeType();
	m_vertices = stroke->Length();
	m_mirror = stroke->IsMirror();
	m_geometry.reset(geometry);
}

void CRGNUITaskAddStroke::Dispose()
{
	delete this;
}

void CRGNUITaskAddStroke::Run()
{
	SKEE::SculptTrace::Count(SKEE::SculptTrace::Event::HistoryRun);
	if (m_editorGeneration != g_World.GetEditorGeneration() || !g_World.GetNumMeshes()) {
		SKEE::SculptTrace::Count(SKEE::SculptTrace::Event::HistoryStale);
		return;
	}
	RE::IMenu * menu = RE::UI::GetSingleton()->GetMenu(RE::InterfaceStrings::GetSingleton()->raceSexMenu).get();
	if (menu && menu->uiMovie) {
		RE::GFxValue obj{};
		menu->uiMovie->CreateObject(&obj);
		RE::GFxValue commandId{};
		commandId.SetNumber(m_id);
		obj.SetMember("id", commandId);
		RE::GFxValue type{};
		type.SetNumber(m_undoType);
		obj.SetMember("type", type);
		RE::GFxValue strokeType{};
		strokeType.SetNumber(m_strokeType);
		obj.SetMember("stroke", strokeType);
		RE::GFxValue vertices{};
		vertices.SetNumber(m_vertices);
		obj.SetMember("vertices", vertices);
		RE::GFxValue mirror{};
		mirror.SetBoolean(m_mirror);
		obj.SetMember("mirror", mirror);
		RE::GFxValue partName{};
		partName.SetString(m_geometry->name.c_str());
		obj.SetMember("part", partName);
		const bool invoked = SKEE::DispatchSculptHistory(REL::Module::IsVR(), obj,
			[&](const char* method, const RE::GFxValue* args, std::uint32_t count) {
				return menu->uiMovie->Invoke(method, nullptr, args, count);
			});
		SKEE::SculptTrace::Count(invoked ? SKEE::SculptTrace::Event::HistoryInvoked : SKEE::SculptTrace::Event::HistoryInvokeFailed);
	}
	else SKEE::SculptTrace::Count(SKEE::SculptTrace::Event::HistoryNoMovie);
}

CRGNUITaskStandardCommand::CRGNUITaskStandardCommand(CDXUndoCommand * cmd, RE::BSTriShape * geometry, std::int32_t id)
{
	m_id = id;
	m_editorGeneration = g_World.GetEditorGeneration();
	m_undoType = cmd->GetUndoType();
	m_geometry.reset(geometry);
}

void CRGNUITaskStandardCommand::Dispose()
{
	delete this;
}

void CRGNUITaskStandardCommand::Run()
{
	if (m_editorGeneration != g_World.GetEditorGeneration() || !g_World.GetNumMeshes()) return;
	RE::IMenu * menu = RE::UI::GetSingleton()->GetMenu(RE::InterfaceStrings::GetSingleton()->raceSexMenu).get();
	if (menu && menu->uiMovie) {
		RE::GFxValue obj;
		menu->uiMovie->CreateObject(&obj);
		RE::GFxValue commandId;
		commandId.SetNumber(m_id);
		obj.SetMember("id", commandId);
		RE::GFxValue type;
		type.SetNumber(m_undoType);
		obj.SetMember("type", type);
		RE::GFxValue partName;
		partName.SetString(m_geometry->name.c_str());
		obj.SetMember("part", partName);
		SKEE::DispatchSculptHistory(REL::Module::IsVR(), obj,
			[&](const char* method, const RE::GFxValue* args, std::uint32_t count) {
				return menu->uiMovie->Invoke(method, nullptr, args, count);
			});
	}
}

CDXNifResetSculpt::CDXNifResetSculpt(CDXNifMesh * mesh) : CDXUndoCommand()
{
	m_mesh = mesh;

	RE::Actor * actor = g_World.GetWorkingActor();
	RE::TESNPC * npc = actor->GetBaseObject() ? actor->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	RE::BSTriShape * geometry = m_mesh->GetGeometry();
	if (geometry) {
		// Create mapped extra vertex data for NPC
		auto sculptTarget = g_morphInterface.GetSculptTarget(npc, false);
		if (sculptTarget) {
			std::string headPartName = geometry->name.c_str();
			RE::BGSHeadPart * headPart = GetHeadPartByName(headPartName);
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
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
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
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifResetSculpt::Apply(std::int32_t i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		if (g_task)
			SKEE_AddUITask(g_task, new CRGNUITaskStandardCommand(this, geometry, i));
	}
}

CDXNifImportGeometry::CDXNifImportGeometry(CDXNifMesh * mesh, RE::NiAVObject * source) : CDXUndoCommand()
{
	m_mesh = mesh;

	RE::Actor * actor = g_World.GetWorkingActor();
	RE::TESNPC * npc = actor->GetBaseObject() ? actor->GetBaseObject()->As<RE::TESNPC>() : nullptr;
	RE::BSTriShape * target = m_mesh->GetGeometry();
	if (target) {
		// Create mapped extra vertex data for NPC
		auto sculptTarget = g_morphInterface.GetSculptTarget(npc, true);
		if (sculptTarget) {
			std::string headPartName = target->name.c_str();
			RE::BGSHeadPart * headPart = GetHeadPartByName(headPartName);
			if (headPart) {
				auto sculptHost = sculptTarget->GetSculptHost(SculptData::GetHostByPart(headPart), true);
				if (sculptHost) {
					std::uint32_t srcNumVertices = 0;
					std::uint32_t dstNumVertices = target->vertexCount;

					RE::NiPoint3 * srcGeometry = nullptr;
					RE::NiPoint3 * dstGeometry = nullptr;

					std::uint32_t srcStride = 0;
					std::uint32_t dstStride = 0;

					RE::NiTransform srcTransform;
					RE::NiTransform dstTransform;

					RE::BSSpinLock * srcLock = nullptr;
					RE::BSSpinLock * dstLock = nullptr;

					RE::BSDynamicTriShape * dstDynamicShape = target ? target->AsDynamicTriShape() : nullptr;
					if (dstDynamicShape) {
						dstGeometry = reinterpret_cast<RE::NiPoint3*>(dstDynamicShape->dynamicData);
						dstStride = sizeof(XMFLOAT4);
						dstLock = &dstDynamicShape->lock;
						dstTransform = GetGeometryTransform(dstDynamicShape);
					}

					RE::NiGeometry * legacyGeometry = source ? source->AsNiGeometry() : nullptr;
					if (legacyGeometry && legacyGeometry->spModelData) {
						RE::NiGeometryData * srcData = legacyGeometry->spModelData.get();
						srcNumVertices = srcData->vertices;
						srcGeometry = srcData->vertex;
						srcStride = sizeof(RE::NiPoint3);
						srcTransform = GetLegacyGeometryTransform(legacyGeometry);
					}
					RE::BSTriShape * sourceGeometry = source ? source->AsTriShape() : nullptr;
					if (sourceGeometry) {
						srcTransform = GetGeometryTransform(sourceGeometry);
						srcNumVertices = sourceGeometry->vertexCount;

						RE::BSDynamicTriShape * srcDynamicShape = sourceGeometry ? sourceGeometry->AsDynamicTriShape() : nullptr;
						if (srcDynamicShape) {
							srcGeometry = reinterpret_cast<RE::NiPoint3*>(srcDynamicShape->dynamicData);
							srcLock = &srcDynamicShape->lock;
							srcStride = sizeof(XMFLOAT4);
						}
						else {
							const RE::NiSkinInstance * skinInstance = sourceGeometry->skinInstance.get();
							const RE::NiSkinPartition * skinPartition = skinInstance ? skinInstance->skinPartition.get() : nullptr;
							srcGeometry = skinPartition ? reinterpret_cast<RE::NiPoint3*>(skinPartition->partitions[0].buffData->rawVertexData) : nullptr;
							srcStride = sourceGeometry->vertexDesc.GetSize();
						}
					}

					if (srcLock) srcLock->Lock();
					if (dstLock) dstLock->Lock();

					if (srcNumVertices == dstNumVertices && srcGeometry && dstGeometry) {
						CDXMeshVert* pVertices = m_mesh->LockVertices(CDXMesh::LockMode::WRITE);
						if (pVertices) {
							for (std::uint32_t i = 0; i < srcNumVertices; i++) {
								// Skip masked vertices
								if (XMVector3Equal(XMLoadFloat3(&pVertices[i].Color), COLOR_SELECTED))
									continue;

								RE::NiPoint3* srcVertex = reinterpret_cast<RE::NiPoint3*>(reinterpret_cast<std::uint8_t*>(srcGeometry) + (srcStride * i));
								RE::NiPoint3* dstVertex = reinterpret_cast<RE::NiPoint3*>(reinterpret_cast<std::uint8_t*>(dstGeometry) + (dstStride * i));

								RE::NiPoint3 diff = (srcTransform * (*srcVertex)) - (dstTransform * (*dstVertex));
								XMVECTOR diffVector = XMLoadFloat3(reinterpret_cast<XMFLOAT3*>(&diff));

								XMStoreFloat3(&pVertices[i].Position, XMVectorAdd(XMLoadFloat3(&pVertices[i].Position), diffVector));

								m_current.emplace(i, diffVector);
							}

							m_mesh->UnlockVertices(CDXMesh::LockMode::WRITE);
						}
					}

					if (srcLock) srcLock->Unlock();
					if (dstLock) dstLock->Unlock();
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
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
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
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifImportGeometry::Apply(std::int32_t i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	RE::BSTriShape * geometry = nifMesh->GetGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		if (g_task)
			SKEE_AddUITask(g_task, new CRGNUITaskStandardCommand(this, geometry, i));
	}
}
