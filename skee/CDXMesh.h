#pragma once

#ifdef FIXME

#ifdef CDX_MUTEX
#include <mutex>
#endif

#include <set>

typedef D3DXMATRIX		CDXMatrix;
typedef unsigned short	CDXMeshIndex;
typedef D3DXVECTOR3		CDXVec3;
typedef D3DXVECTOR2		CDXVec2;
typedef D3DCOLOR		CDXColor;

typedef std::set<CDXMeshIndex> CDXMeshIndexSet;
typedef std::map<CDXMeshIndex, float> CDXHitIndexMap;

struct CDXMeshEdge
{
	CDXMeshIndex	p1;
	CDXMeshIndex	p2;

	CDXMeshEdge(CDXMeshIndex _p1, CDXMeshIndex _p2)
	{
		p1 = _p1;
		p2 = _p2;
	}
};

struct CDXMeshFace
{
	CDXMeshIndex	v1;
	CDXMeshIndex	v2;
	CDXMeshIndex	v3;

	CDXMeshFace(CDXMeshIndex _v1, CDXMeshIndex _v2, CDXMeshIndex _v3)
	{
		v1 = _v1;
		v2 = _v2;
		v3 = _v3;
	}
};

namespace std {
	template<> struct hash < CDXMeshEdge >
	{
		std::size_t operator() (const CDXMeshEdge & t) const
		{
			return ((t.p2 << 16) | (t.p1 & 0xFFFF));
		}

	};

	template <> struct equal_to < CDXMeshEdge >
	{
		bool operator() (const CDXMeshEdge& t1, const CDXMeshEdge& t2) const
		{
			return ((t1.p1 == t2.p1) && (t1.p2 == t2.p2));
		}
	};

	template <> struct hash < CDXMeshFace > {
		std::size_t operator() (const CDXMeshFace& t) const {
			char* d = (char*)&t;
			size_t len = sizeof(CDXMeshFace);
			size_t hash, i;
			for (hash = i = 0; i < len; ++i)
			{
				hash += d[i];
				hash += (hash << 10);
				hash ^= (hash >> 6);
			}
			hash += (hash << 3);
			hash ^= (hash >> 11);
			hash += (hash << 15);
			return hash;
		}
	};
	template <> struct equal_to < CDXMeshFace >
	{
		bool operator() (const CDXMeshFace& t1, const CDXMeshFace& t2) const
		{
			return ((t1.v1 == t2.v1) && (t1.v2 == t2.v2) && (t1.v3 == t2.v3));
		}
	};
}

struct CDXMeshVert
{
	CDXVec3		Position;
	CDXVec3		Normal;
	CDXVec2		Tex;
	CDXColor	Color;
};

const static D3DVERTEXELEMENT9 VertexDecl[6] =
{
	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
	{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	{ 0, 32, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	D3DDECL_END()
};

class CDXMaterial;
class CDXPicker;
class CDXShader;
class CDXEditableMesh;
class CDXRayInfo;
class CDXPickInfo;

class CDXMesh
{
public:
	CDXMesh();
	~CDXMesh();

	virtual void Render(LPDIRECT3DDEVICE9 pDevice, CDXShader * shader);
	virtual void Pass(LPDIRECT3DDEVICE9 pDevice, UInt32 iPass, CDXShader * shader);
	virtual void Release();
	virtual bool Pick(CDXRayInfo & rayInfo, CDXPickInfo & pickInfo);
	virtual bool IsEditable() { return false; }
	virtual bool IsLocked() { return true; }

	void SetVisible(bool visible);
	bool IsVisible();

	void SetMaterial(CDXMaterial * material);
	CDXMaterial * GetMaterial();

	CDXMeshVert* LockVertices();
	CDXMeshIndex* LockIndices();

	void UnlockVertices();
	void UnlockIndices();

	LPDIRECT3DVERTEXBUFFER9 GetVertexBuffer();
	LPDIRECT3DINDEXBUFFER9 GetIndexBuffer();

	UInt32 GetPrimitiveCount();
	UInt32 GetVertexCount();

protected:
	bool					m_visible;
	LPDIRECT3DVERTEXBUFFER9	m_vertexBuffer;
	UInt32					m_vertCount;
	LPDIRECT3DINDEXBUFFER9	m_indexBuffer;
	UInt32					m_primitiveCount;
	UInt8					m_primitiveType;
	CDXMaterial				* m_material;
	CDXMatrix				m_transform;

#ifdef CDX_MUTEX
	std::mutex				m_mutex;
#endif
};

D3DXVECTOR3 CalculateFaceNormal(UInt32 f, CDXMeshIndex * faces, CDXMeshVert * vertices);
bool IntersectSphere(float radius, float & dist, CDXVec3 & center, CDXVec3 & rayOrigin, CDXVec3 & rayDir);
bool IntersectTriangle(const CDXVec3& orig, const CDXVec3& dir, CDXVec3& v0, CDXVec3& v1, CDXVec3& v2, float* t, float* u, float* v);

#endif