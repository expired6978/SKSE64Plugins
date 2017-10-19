#include "CDXEditableMesh.h"
#include "CDXMaterial.h"
#include "CDXShader.h"

#ifdef FIXME

CDXEditableMesh::CDXEditableMesh() : CDXMesh()
{
	m_wireframe = true;
	m_locked = false;
}

CDXEditableMesh::~CDXEditableMesh()
{
	m_adjacency.clear();
	m_vertexEdges.clear();
}

bool CDXEditableMesh::IsEditable()
{
	return true;
}

bool CDXEditableMesh::IsLocked()
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return m_locked;
}

bool CDXEditableMesh::ShowWireframe()
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return m_wireframe;
}
void CDXEditableMesh::SetShowWireframe(bool wf)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	m_wireframe = wf;
}
void CDXEditableMesh::SetLocked(bool l)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	m_locked = l;
}

void CDXEditableMesh::VisitAdjacencies(CDXMeshIndex i, std::function<bool(CDXMeshFace&)> functor)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	auto it = m_adjacency.find(i);
	if (it != m_adjacency.end()) {
		for (auto adj : it->second) {
			if (functor(adj))
				break;
		}
	}
}

void CDXEditableMesh::Render(LPDIRECT3DDEVICE9 pDevice, CDXShader * shader)
{
	pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

	CDXMesh::Render(pDevice, shader);

	// Render again but in wireframe
	if (m_wireframe) {
		pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);

		if (m_material) {
			m_material->SetWireframe(true);
			CDXMesh::Render(pDevice, shader);
			m_material->SetWireframe(false);
		}
	}
}

CDXVec3 CDXEditableMesh::CalculateVertexNormal(CDXMeshIndex i)
{
	CDXMeshVert* pVertices = NULL;
	CDXMeshIndex* pIndices = NULL;

	D3DXVECTOR3 vNormal(0, 0, 0);
	LPDIRECT3DVERTEXBUFFER9 pVB = GetVertexBuffer();
	pVB->Lock(0, 0, (void**)&pVertices, 0);
	if (!pVertices)
		return vNormal;

	auto it = m_adjacency.find(i);
	if (it != m_adjacency.end()) {
		for (auto tri : it->second) {
			CDXMeshVert * v1 = &pVertices[tri.v1];
			CDXMeshVert * v2 = &pVertices[tri.v2];
			CDXMeshVert * v3 = &pVertices[tri.v3];

			D3DXVECTOR3 e1 = *(D3DXVECTOR3*)v2 - *(D3DXVECTOR3*)v1;
			D3DXVECTOR3 e2 = *(D3DXVECTOR3*)v3 - *(D3DXVECTOR3*)v2;
			D3DXVec3Cross(&vNormal, &e1, &e2);
			D3DXVec3Normalize(&vNormal, &vNormal);
		}
		D3DXVec3Normalize(&vNormal, &vNormal);
	}

	pVB->Unlock();
	return vNormal;
}

bool CDXEditableMesh::IsEdgeVertex(CDXMeshIndex i) const
{
	auto it = m_vertexEdges.find(i);
	if (it != m_vertexEdges.end())
		return true;

	return false;
}

#endif