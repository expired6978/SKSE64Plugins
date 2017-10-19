#ifdef FIXME

#include "CDXNifCommands.h"
#include "CDXNifMesh.h"
#include "CDXNifScene.h"

#include "skse64/GameData.h"
#include "skse64/GameReferences.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameMenus.h"

#include "skse64/NiGeometry.h"
#include "skse64/NiExtraData.h"

#include "MorphHandler.h"
#include "FileUtils.h"
#include "NifUtils.h"

#include "skse64/ScaleformCallbacks.h"

#include "skse64/PluginAPI.h"

extern MorphHandler			g_morphHandler;
extern SKSETaskInterface	* g_task;
extern CDXNifScene			g_World;

void ApplyMorphData(NiGeometry * geometry, CDXVectorMap & vectorMap, float multiplier)
{
	Actor * actor = g_World.GetWorkingActor();
	TESNPC * npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);

	// Create mapped extra vertex data for NPC
	auto sculptTarget = g_morphHandler.GetSculptTarget(npc, true);
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

void AddStrokeCommand(CDXStroke * stroke, NiGeometry * geometry, SInt32 id)
{
	if (g_task)
		g_task->AddUITask(new CRGNUITaskAddStroke(stroke, geometry, id));
}

void CDXNifInflateStroke::Undo()
{
	CDXInflateStroke::Undo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifInflateStroke::Redo()
{
	CDXInflateStroke::Redo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifDeflateStroke::Undo()
{
	CDXDeflateStroke::Undo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifDeflateStroke::Redo()
{
	CDXDeflateStroke::Redo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifSmoothStroke::Undo()
{
	CDXSmoothStroke::Undo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}
void CDXNifSmoothStroke::Redo()
{
	CDXSmoothStroke::Redo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifMoveStroke::Undo()
{
	CDXMoveStroke::Undo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifMoveStroke::Redo()
{
	CDXMoveStroke::Redo();
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifInflateStroke::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		AddStrokeCommand(this, geometry, i);
	}
}

void CDXNifDeflateStroke::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		AddStrokeCommand(this, geometry, i);
	}
}

void CDXNifSmoothStroke::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		AddStrokeCommand(this, geometry, i);
	}	
}

void CDXNifMoveStroke::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		AddStrokeCommand(this, geometry, i);
	}
}

void CDXNifMaskAddStroke::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		AddStrokeCommand(this, geometry, i);
	}
}

void CDXNifMaskSubtractStroke::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		AddStrokeCommand(this, geometry, i);
	}
}


void CDXNifResetMask::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		if (g_task)
			g_task->AddUITask(new CRGNUITaskStandardCommand(this, geometry, i));
	}
}

CRGNTaskUpdateModel::CRGNTaskUpdateModel(NiGeometry * geometry)
{
	m_geometry = geometry;
	if (m_geometry)
		m_geometry->IncRef();
}

void CRGNTaskUpdateModel::Run()
{
	if (m_geometry)
		UpdateModelFace(m_geometry);
}

void CRGNTaskUpdateModel::Dispose()
{
	if (m_geometry)
		m_geometry->DecRef();
	delete this;
}

CRGNUITaskAddStroke::CRGNUITaskAddStroke(CDXStroke * stroke, NiGeometry * geometry, SInt32 id)
{
	m_id = id;
	m_stroke = stroke;
	m_geometry = geometry;
	if (m_geometry)
		m_geometry->IncRef();
}

void CRGNUITaskAddStroke::Dispose()
{
	if (m_geometry)
		m_geometry->DecRef();
	delete this;
}

void CRGNUITaskAddStroke::Run()
{
	IMenu * menu = MenuManager::GetSingleton()->GetMenu(&UIStringHolder::GetSingleton()->raceSexMenu);
	if (menu && menu->view) {
		GFxValue obj;
		menu->view->CreateObject(&obj);
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

		FxResponseArgsList args;
		args.Add(&obj);
		InvokeFunction(menu->view, "AddAction", &args);
	}
}

CRGNUITaskStandardCommand::CRGNUITaskStandardCommand(CDXUndoCommand * cmd, NiGeometry * geometry, SInt32 id)
{
	m_id = id;
	m_cmd = cmd;
	m_geometry = geometry;
	if (m_geometry)
		m_geometry->IncRef();
}

void CRGNUITaskStandardCommand::Dispose()
{
	if (m_geometry)
		m_geometry->DecRef();
	delete this;
}

void CRGNUITaskStandardCommand::Run()
{
	IMenu * menu = MenuManager::GetSingleton()->GetMenu(&UIStringHolder::GetSingleton()->raceSexMenu);
	if (menu && menu->view) {
		GFxValue obj;
		menu->view->CreateObject(&obj);
		GFxValue commandId;
		commandId.SetNumber(m_id);
		obj.SetMember("id", &commandId);
		GFxValue type;
		type.SetNumber(m_cmd->GetUndoType());
		obj.SetMember("type", &type);
		GFxValue partName;
		partName.SetString(m_geometry->m_name);
		obj.SetMember("part", &partName);

		FxResponseArgsList args;
		args.Add(&obj);
		InvokeFunction(menu->view, "AddAction", &args);
	}
}

CDXNifResetSculpt::CDXNifResetSculpt(CDXNifMesh * mesh) : CDXUndoCommand()
{
	m_mesh = mesh;

	Actor * actor = g_World.GetWorkingActor();
	TESNPC * npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
	NiGeometry * geometry = m_mesh->GetNifGeometry();

	if (geometry) {
		// Create mapped extra vertex data for NPC
		auto sculptTarget = g_morphHandler.GetSculptTarget(npc, false);
		if (sculptTarget) {
			std::string headPartName = geometry->m_name;
			BGSHeadPart * headPart = GetHeadPartByName(headPartName);
			if (headPart) {
				auto sculptHost = sculptTarget->GetSculptHost(SculptData::GetHostByPart(headPart), false);
				if (sculptHost) {
					CDXMeshVert* pVertices = m_mesh->LockVertices();
					for (auto it : *sculptHost) {
						// Skip masked vertices
						if (pVertices[it.first].Color != COLOR_UNSELECTED)
							continue;
						// Store it in the NPC mapped data
						CDXVec3 temp = *(CDXVec3*)&it.second;
						pVertices[it.first].Position -= temp;
						m_current.emplace(it.first, -temp);
					}

					m_mesh->UnlockVertices();
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
	CDXMeshVert* pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	// Do what we have now
	for (auto it : m_current)
		pVertices[it.first].Position += it.second;

	m_mesh->UnlockVertices();

	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifResetSculpt::Undo()
{
	CDXMeshVert* pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	// Undo what we did
	for (auto it : m_current)
		pVertices[it.first].Position -= it.second;

	m_mesh->UnlockVertices();

	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifResetSculpt::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		if (g_task)
			g_task->AddUITask(new CRGNUITaskStandardCommand(this, geometry, i));
	}
}

CDXNifImportGeometry::CDXNifImportGeometry(CDXNifMesh * mesh, NiGeometry * source) : CDXUndoCommand()
{
	m_mesh = mesh;

	Actor * actor = g_World.GetWorkingActor();
	TESNPC * npc = DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC);
	NiGeometry * geometry = m_mesh->GetNifGeometry();

	if (geometry) {
		// Create mapped extra vertex data for NPC
		auto sculptTarget = g_morphHandler.GetSculptTarget(npc, true);
		if (sculptTarget) {
			std::string headPartName = geometry->m_name;
			BGSHeadPart * headPart = GetHeadPartByName(headPartName);
			if (headPart) {
				auto sculptHost = sculptTarget->GetSculptHost(SculptData::GetHostByPart(headPart), true);
				if (sculptHost) {
					// Create differences from source and destination geometry
					NiGeometryData * dstData = niptr_cast<NiGeometryData>(geometry->m_spModelData);
					NiGeometryData * srcData = niptr_cast<NiGeometryData>(source->m_spModelData);

					NiTransform dstTransform = GetGeometryTransform(geometry);
					NiTransform srcTransform = GetGeometryTransform(source);

					if (dstData && srcData && dstData->m_usVertices == srcData->m_usVertices) {

						CDXMeshVert* pVertices = m_mesh->LockVertices();

						for (UInt32 i = 0; i < srcData->m_usVertices; i++) {

							if (pVertices[i].Color != COLOR_UNSELECTED)
								continue;

							NiPoint3 diff = (srcTransform * srcData->m_pkVertex[i]) - (dstTransform * dstData->m_pkVertex[i]);
							CDXVec3 temp = *(CDXVec3*)&diff;

							pVertices[i].Position += temp;

							// Store it in the action
							m_current.emplace(i, temp);
						}

						m_mesh->UnlockVertices();
					}
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
	CDXMeshVert* pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	// Do what we have now
	for (auto it : m_current)
		pVertices[it.first].Position += it.second;

	m_mesh->UnlockVertices();

	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
	}
}

void CDXNifImportGeometry::Undo()
{
	CDXMeshVert* pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	// Undo what we did
	for (auto it : m_current)
		pVertices[it.first].Position -= it.second;

	m_mesh->UnlockVertices();

	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, -1.0);
	}
}

void CDXNifImportGeometry::Apply(SInt32 i)
{
	CDXNifMesh * nifMesh = static_cast<CDXNifMesh*>(m_mesh);
	NiGeometry * geometry = nifMesh->GetNifGeometry();
	if (geometry) {
		ApplyMorphData(geometry, m_current, 1.0);
		if (g_task)
			g_task->AddUITask(new CRGNUITaskStandardCommand(this, geometry, i));
	}
}

#endif