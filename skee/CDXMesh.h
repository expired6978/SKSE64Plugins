#pragma once

#ifdef CDX_MUTEX
#include <mutex>
#endif

#include "CDXTypes.h"

/*const static D3DVERTEXELEMENT9 VertexDecl[6] =
{
	{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
	{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
	{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
	{ 0, 32, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
	D3DDECL_END()
};*/

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

	virtual void Render(ID3D11Device * pDevice, CDXShader * shader);
	virtual void Pass(ID3D11Device * pDevice, UInt32 iPass, CDXShader * shader);
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

	ID3D11Buffer * GetVertexBuffer();
	ID3D11Buffer * GetIndexBuffer();

	UInt32 GetPrimitiveCount();
	UInt32 GetVertexCount();

protected:
	bool					m_visible;
	ID3D11Buffer		*	m_vertexBuffer;
	UInt32					m_vertCount;
	ID3D11Buffer		*	m_indexBuffer;
	UInt32					m_primitiveCount;
	//UInt8					m_primitiveType;
	//CDXMaterial				* m_material;
	CDXMatrix				m_transform;

#ifdef CDX_MUTEX
	std::mutex				m_mutex;
#endif
};

DirectX::XMVECTOR CalculateFaceNormal(UInt32 f, CDXMeshIndex * faces, CDXMeshVert * vertices);
bool IntersectSphere(float radius, float & dist, CDXVec & center, CDXVec & rayOrigin, CDXVec & rayDir);
bool IntersectTriangle(const CDXVec& orig, const CDXVec& dir, CDXVec& v0, CDXVec& v1, CDXVec& v2, float* t, float* u, float* v);
