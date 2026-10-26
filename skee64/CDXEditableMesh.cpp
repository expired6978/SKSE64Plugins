#include "CDXEditableMesh.h"
#include <algorithm>
#include "CDXMaterial.h"
#include "CDXShader.h"
#include <cstdint>

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
	std::lock_guard guard(m_dataMutex);
	return m_locked;
}

bool CDXEditableMesh::ShowWireframe() const
{
	std::lock_guard guard(m_dataMutex);
	return m_wireframe;
}
void CDXEditableMesh::SetShowWireframe(bool wf)
{
	std::lock_guard guard(m_dataMutex);
	m_wireframe = wf;
}
void CDXEditableMesh::SetLocked(bool l)
{
	std::lock_guard guard(m_dataMutex);
	m_locked = l;
}

void CDXEditableMesh::BuildAdjacency()
{
	CDXMesh::IndexAccess indexAccess(*this);
	auto* pIndices = indexAccess.Get();

	if (!pIndices)
		return;

	m_adjacency.clear();
	for (std::uint32_t i = 0; i < GetVertexCount(); i++) {
		for (std::uint32_t f = 0; f < GetFaceCount(); f++) {
			CDXMeshFace * face = (CDXMeshFace *)&pIndices[f * 3];
			if (i == face->v1 || i == face->v2 || i == face->v3)
				m_adjacency[i].push_back(*face);
		}
	}

	indexAccess.Release();
}

void CDXEditableMesh::BuildFacemap()
{
	CDXMesh::IndexAccess indexAccess(*this);
	auto* pIndices = indexAccess.Get();
	if (!pIndices)
		return;

	CDXEdgeMap edges;
	for (std::uint32_t f = 0; f < GetFaceCount(); f++)
	{
		CDXMeshFace * face = (CDXMeshFace *)&pIndices[f * 3];
		auto it = edges.emplace(CDXMeshEdge(std::min(face->v1, face->v2), std::max(face->v1, face->v2)), 1);
		if (it.second == false)
			it.first->second++;
		it = edges.emplace(CDXMeshEdge(std::min(face->v2, face->v3), std::max(face->v2, face->v3)), 1);
		if (it.second == false)
			it.first->second++;
		it = edges.emplace(CDXMeshEdge(std::min(face->v3, face->v1), std::max(face->v3, face->v1)), 1);
		if (it.second == false)
			it.first->second++;
	}

	m_vertexEdges.clear();
	for (auto e : edges) {
		if (e.second == 1) {
			m_vertexEdges.insert(e.first.p1);
			m_vertexEdges.insert(e.first.p2);
		}
	}
	indexAccess.Release();
}

void CDXEditableMesh::BuildNormals()
{
	CDXMesh::VertexAccess vertexAccess(*this, LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices) {
		vertexAccess.Release();
		return;
	}

	for (std::uint32_t i = 0; i < GetVertexCount(); i++) {
		 XMStoreFloat3(&pVertices[i].Normal, CalculateVertexNormal(i));
	}
	vertexAccess.Release();
}

void CDXEditableMesh::VisitAdjacencies(CDXMeshIndex i, std::function<bool(CDXMeshFace&)> functor)
{
	std::lock_guard guard(m_dataMutex);
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
	std::lock_guard guard(m_dataMutex);
	CDXMesh::Render(pDevice, shader);

	// Render again but in wireframe
	if (m_wireframe) {
		// Now set the rasterizer state.
		if (m_material) {
			const auto previous = m_material->IsWireframe();
			m_material->SetWireframe(true);
			try { CDXMesh::Render(pDevice, shader); }
			catch (...) { m_material->SetWireframe(previous); throw; }
			m_material->SetWireframe(previous);
		}
	}
}

CDXVec CDXEditableMesh::CalculateVertexNormal(CDXMeshIndex i)
{
	CDXMesh::VertexAccess vertexAccess(*this, LockMode::READ);
	auto* pVertices = vertexAccess.Get();

	CDXVec vNormal = XMVectorZero();
	if (!pVertices) {
		vertexAccess.Release();
		return vNormal;
	}

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

	vertexAccess.Release();
	return vNormal;
}

bool CDXEditableMesh::IsEdgeVertex(CDXMeshIndex i) const
{
	std::lock_guard guard(m_dataMutex);
	auto it = m_vertexEdges.find(i);
	if (it != m_vertexEdges.end())
		return true;

	return false;
}
