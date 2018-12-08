#ifndef __CDXEDITABLEMESH__
#define __CDXEDITABLEMESH__

#pragma once

#ifdef FIXME

#include "CDXMesh.h"

#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <functional>

typedef std::unordered_map<CDXMeshIndex, CDXColor>	CDXMaskMap;
typedef std::pair<CDXMeshIndex, CDXColor>			CDXMaskPair;

typedef std::unordered_map<CDXMeshIndex, CDXVec3>	CDXVectorMap;
typedef std::pair<CDXMeshIndex, CDXVec3>			CDXVectorPair;

typedef std::vector<CDXMeshFace>					CDXAdjacentList;
typedef std::map<CDXMeshIndex, CDXAdjacentList>		CDXAdjacencyMap;

typedef std::unordered_map<CDXMeshEdge, UInt32>		CDXEdgeMap;
typedef std::unordered_set<CDXMeshIndex>			CDXVertexEdgeList;

#define COLOR_UNSELECTED	CDXColor(255, 255, 255, 255)
#define COLOR_SELECTED		CDXColor(0, 0, 255, 255)

class CDXEditableMesh : public CDXMesh
{
public:
	CDXEditableMesh();
	~CDXEditableMesh();

	virtual void Render(ID3D11Device * pDevice, CDXShader * shader);
	virtual bool IsEditable();
	virtual bool IsLocked();

	bool ShowWireframe();
	void SetShowWireframe(bool wf);
	void SetLocked(bool l);

	void VisitAdjacencies(CDXMeshIndex i, std::function<bool(CDXMeshFace&)> functor);
	bool IsEdgeVertex(CDXMeshIndex i) const;

	CDXVec CalculateVertexNormal(CDXMeshIndex i);

protected:
	CDXAdjacencyMap		m_adjacency;
	CDXVertexEdgeList	m_vertexEdges;
	bool				m_wireframe;
	bool				m_locked;
};

#endif
#endif
