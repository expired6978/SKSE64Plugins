#include "CDXResetMask.h"
#include <DirectXMath.h>

using namespace DirectX;

CDXResetMask::CDXResetMask(CDXMesh * mesh)
{
	m_mesh = mesh;

	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);

	auto* pVertices = vertexAccess.Get();
	if (pVertices)
	{
		for (CDXMeshIndex i = 0; i < m_mesh->GetVertexCount(); i++) {
			if (XMVector3NotEqual(XMLoadFloat3(&pVertices[i].Color), COLOR_UNSELECTED)) {
				m_previous[i] = pVertices[i].Color;
				XMStoreFloat3(&pVertices[i].Color, COLOR_UNSELECTED);
				XMStoreFloat3(&m_current[i], COLOR_UNSELECTED);
			}
		}

		vertexAccess.Release();
	}
}

CDXResetMask::~CDXResetMask()
{
	m_current.clear();
	m_previous.clear();
}

CDXUndoCommand::UndoType CDXResetMask::GetUndoType()
{
	return kUndoType_ResetMask;
}

void CDXResetMask::Redo()
{
	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices)
		return;

	// Do what we have now
	for (auto it : m_current)
		pVertices[it.first].Color = it.second;

	vertexAccess.Release();
}

void CDXResetMask::Undo()
{
	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices)
		return;

	// Undo what we did
	for (auto it : m_previous)
		pVertices[it.first].Color = it.second;

	vertexAccess.Release();
}
