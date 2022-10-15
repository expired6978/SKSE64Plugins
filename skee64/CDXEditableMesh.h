#ifndef __CDXEDITABLEMESH__
#define __CDXEDITABLEMESH__

#pragma once

#include "CDXMesh.h"

#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <functional>

typedef std::unordered_map<CDXMeshIndex, CDXColor>	CDXMaskMap;
typedef std::pair<CDXMeshIndex, CDXColor>			CDXMaskPair;

typedef std::unordered_map<CDXMeshIndex, CDXVec>	CDXVectorMap;
typedef std::pair<CDXMeshIndex, CDXVec>				CDXVectorPair;

typedef std::vector<CDXMeshFace>					CDXAdjacentList;
typedef std::map<CDXMeshIndex, CDXAdjacentList>		CDXAdjacencyMap;

typedef std::unordered_map<CDXMeshEdge, UInt32>		CDXEdgeMap;
typedef std::unordered_set<CDXMeshIndex>			CDXVertexEdgeList;

static DirectX::XMVECTOR COLOR_UNSELECTED = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
static DirectX::XMVECTOR COLOR_SELECTED = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

class CDXEditableMesh : public CDXMesh
{
public:
	CDXEditableMesh();
	virtual ~CDXEditableMesh();

	virtual void Render(CDXD3DDevice * pDevice, CDXShader * shader);
	virtual bool IsEditable() const override;
	virtual bool IsLocked() const override;
	virtual bool ShowWireframe() const override;

	void SetShowWireframe(bool wf);
	void SetLocked(bool l);

	void BuildAdjacency();
	void BuildFacemap();
	void BuildNormals();

	void VisitAdjacencies(CDXMeshIndex i, std::function<bool(CDXMeshFace&)> functor);
	bool IsEdgeVertex(CDXMeshIndex i) const;

	CDXVec CalculateVertexNormal(CDXMeshIndex i);

protected:
#ifdef CDX_MUTEX
	mutable std::mutex m_mutex;
#endif
	CDXAdjacencyMap		m_adjacency;
	CDXVertexEdgeList	m_vertexEdges;
	bool				m_wireframe;
	bool				m_locked;
};

#endif
