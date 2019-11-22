#include "CDXEditableMesh.h"
#include "CDXMaterial.h"
#include "CDXShader.h"

using namespace DirectX;

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

bool CDXEditableMesh::IsEditable() const
{
	return true;
}

bool CDXEditableMesh::IsLocked() const
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return m_locked;
}

bool CDXEditableMesh::ShowWireframe() const
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

void CDXEditableMesh::BuildAdjacency()
{
	CDXMeshIndex* pIndices = LockIndices();

	if (!pIndices)
		return;

	for (UInt16 i = 0; i < GetVertexCount(); i++) {
		for (UInt32 f = 0; f < GetFaceCount(); f++) {
			CDXMeshFace * face = (CDXMeshFace *)&pIndices[f * 3];
			if (i == face->v1 || i == face->v2 || i == face->v3)
				m_adjacency[i].push_back(*face);
		}
	}

	UnlockIndices();
}

void CDXEditableMesh::BuildFacemap()
{
	CDXMeshIndex* pIndices = LockIndices();
	if (!pIndices)
		return;

	CDXEdgeMap edges;
	for (UInt32 f = 0; f < GetFaceCount(); f++)
	{
		CDXMeshFace * face = (CDXMeshFace *)&pIndices[f * 3];
		auto it = edges.emplace(CDXMeshEdge(min(face->v1, face->v2), max(face->v1, face->v2)), 1);
		if (it.second == false)
			it.first->second++;
		it = edges.emplace(CDXMeshEdge(min(face->v2, face->v3), max(face->v2, face->v3)), 1);
		if (it.second == false)
			it.first->second++;
		it = edges.emplace(CDXMeshEdge(min(face->v3, face->v1), max(face->v3, face->v1)), 1);
		if (it.second == false)
			it.first->second++;
	}

	for (auto e : edges) {
		if (e.second == 1) {
			m_vertexEdges.insert(e.first.p1);
			m_vertexEdges.insert(e.first.p2);
		}
	}
	UnlockIndices();
}

void CDXEditableMesh::BuildNormals()
{
	CDXMeshVert* pVertices = LockVertices(LockMode::WRITE);
	if (!pVertices)
		return;

	for (UInt16 i = 0; i < GetVertexCount(); i++) {
		 XMStoreFloat3(&pVertices[i].Normal, CalculateVertexNormal(i));
	}
	UnlockVertices(LockMode::WRITE);
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

void CDXEditableMesh::Render(CDXD3DDevice * pDevice, CDXShader * shader)
{
	CDXMesh::Render(pDevice, shader);

	// Render again but in wireframe
	if (m_wireframe) {
		// Now set the rasterizer state.
		if (m_material) {
			m_material->SetWireframe(true);
			CDXMesh::Render(pDevice, shader);
			m_material->SetWireframe(false);
		}
	}
}

CDXVec CDXEditableMesh::CalculateVertexNormal(CDXMeshIndex i)
{
	CDXMeshVert* pVertices = LockVertices(LockMode::WRITE);

	CDXVec vNormal = XMVectorZero();
	if (!pVertices)
		return vNormal;

	auto it = m_adjacency.find(i);
	if (it != m_adjacency.end()) {
		for (auto tri : it->second) {
			CDXMeshVert * v1 = &pVertices[tri.v1];
			CDXMeshVert * v2 = &pVertices[tri.v2];
			CDXMeshVert * v3 = &pVertices[tri.v3];

			auto e1 = XMVectorSubtract(XMLoadFloat3(&v2->Position), XMLoadFloat3(&v1->Position));
			auto e2 = XMVectorSubtract(XMLoadFloat3(&v3->Position), XMLoadFloat3(&v2->Position));

			auto faceNormal = XMVector3Cross(e1, e2);
			faceNormal = XMVector3Normalize(faceNormal);
			vNormal = XMVectorAdd(vNormal, faceNormal);
		}
		vNormal = XMVector3Normalize(vNormal);
	}

	UnlockVertices(LockMode::WRITE);
	return vNormal;
}

bool CDXEditableMesh::IsEdgeVertex(CDXMeshIndex i) const
{
	auto it = m_vertexEdges.find(i);
	if (it != m_vertexEdges.end())
		return true;

	return false;
}
