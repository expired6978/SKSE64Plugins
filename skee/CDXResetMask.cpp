#ifdef FIXME

#include "CDXResetMask.h"

CDXResetMask::CDXResetMask(CDXMesh * mesh)
{
	m_mesh = mesh;

	CDXMeshVert* pVertices = NULL;
	LPDIRECT3DVERTEXBUFFER9 pVB = m_mesh->GetVertexBuffer();

	pVB->Lock(0, 0, (void**)&pVertices, 0);
	for (CDXMeshIndex i = 0; i < m_mesh->GetVertexCount(); i++) {
		CDXColor unselected = COLOR_UNSELECTED;
		if (pVertices[i].Color != unselected) {
			m_previous[i] = pVertices[i].Color;
			pVertices[i].Color = unselected;
			m_current[i] = unselected;
		}
	}

	pVB->Unlock();
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
	CDXMeshVert* pVertices = NULL;
	CDXMeshIndex* pIndices = NULL;

	LPDIRECT3DVERTEXBUFFER9 pVB = m_mesh->GetVertexBuffer();

	pVB->Lock(0, 0, (void**)&pVertices, 0);

	// Do what we have now
	for (auto it : m_current)
		pVertices[it.first].Color = it.second;

	pVB->Unlock();
}

void CDXResetMask::Undo()
{
	CDXMeshVert* pVertices = NULL;
	CDXMeshIndex* pIndices = NULL;

	LPDIRECT3DVERTEXBUFFER9 pVB = m_mesh->GetVertexBuffer();

	pVB->Lock(0, 0, (void**)&pVertices, 0);

	// Undo what we did
	for (auto it : m_previous)
		pVertices[it.first].Color = it.second;

	pVB->Unlock();
}

#endif