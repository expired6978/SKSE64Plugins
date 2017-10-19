#include "CDXMesh.h"
#include "CDXShader.h"
#include "CDXCamera.h"
#include "CDXMaterial.h"
#include "CDXPicker.h"

#ifdef FIXME

CDXMesh::CDXMesh()
{
	m_vertexBuffer = NULL;
	m_vertCount = 0;
	m_indexBuffer = NULL;
	m_primitiveCount = 0;
	m_visible = true;
	m_material = NULL;
	m_primitiveType = D3DPT_TRIANGLELIST;

	D3DXMatrixIdentity(&m_transform);
}

CDXMesh::~CDXMesh()
{
	
}

void CDXMesh::Release()
{
	m_vertCount = 0;
	m_primitiveCount = 0;
	m_visible = false;
	if(m_vertexBuffer) {
		m_vertexBuffer->Release();
		m_vertexBuffer = NULL;
	}
	if(m_indexBuffer) {
		m_indexBuffer->Release();
		m_indexBuffer = NULL;
	}
	if(m_material) {
		m_material->Release();
		delete m_material;
		m_material = NULL;
	}
}

void CDXMesh::SetMaterial(CDXMaterial * material)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	m_material = material;
}

CDXMaterial * CDXMesh::GetMaterial()
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return m_material;
}

void CDXMesh::SetVisible(bool visible)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	m_visible = visible;
}
bool CDXMesh::IsVisible()
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return m_visible;
}

LPDIRECT3DVERTEXBUFFER9 CDXMesh::GetVertexBuffer()
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return m_vertexBuffer;
}
LPDIRECT3DINDEXBUFFER9 CDXMesh::GetIndexBuffer()
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return m_indexBuffer;
}

UInt32 CDXMesh::GetPrimitiveCount()
{
	return m_primitiveCount;
}
UInt32 CDXMesh::GetVertexCount()
{
	return m_vertCount;
}

D3DXVECTOR3 CalculateFaceNormal(UInt32 f, CDXMeshIndex * faces, CDXMeshVert * vertices)
{
	D3DXVECTOR3 vNormal(0, 0, 0);
	CDXMeshVert * v1 = &vertices[faces[f]];
	CDXMeshVert * v2 = &vertices[faces[f + 1]];
	CDXMeshVert * v3 = &vertices[faces[f + 2]];

	D3DXVECTOR3 e1 = *(D3DXVECTOR3*)v2 - *(D3DXVECTOR3*)v1;
	D3DXVECTOR3 e2 = *(D3DXVECTOR3*)v3 - *(D3DXVECTOR3*)v2;

	D3DXVec3Cross(&vNormal, &e1, &e2);
	D3DXVec3Normalize(&vNormal, &vNormal);
	return vNormal;
}

bool IntersectSphere(float radius, float & dist, CDXVec3 & center, CDXVec3 & rayOrigin, CDXVec3 & rayDir)
{
	//FLOAT t0 = -1, t1 = -1; // solutions for t if the ray intersects

	CDXVec3 L = center - rayOrigin;
	float tca = D3DXVec3Dot(&L, &rayDir);
	if (tca < 0) return false;
	float d2 = D3DXVec3Dot(&L, &L) - tca * tca;
	if (d2 > radius) return false;
	float thc = sqrt(radius - d2);
	//t0 = tca - thc;
	//t1 = tca + thc;
	dist = d2;
	return true;
}

bool IntersectTriangle( const CDXVec3& orig, const CDXVec3& dir, CDXVec3& v0, CDXVec3& v1, CDXVec3& v2, float* t, float* u, float* v )
{
	// Find vectors for two edges sharing vert0
	CDXVec3 edge1 = v1 - v0;
	CDXVec3 edge2 = v2 - v0;

	// Begin calculating determinant - also used to calculate U parameter
	CDXVec3 pvec;
	D3DXVec3Cross(&pvec, &dir, &edge2);

	// If determinant is near zero, ray lies in plane of triangle
	float det = D3DXVec3Dot(&edge1, &pvec);

	CDXVec3 tvec;
	if(det > 0) {
		tvec = orig - v0;
	} else {
		tvec = v0 - orig;
		det = -det;
	}

	if(det < 0.0001f)
		return false;

	// Calculate U parameter and test bounds
	*u = D3DXVec3Dot(&tvec, &pvec);
	if(*u < 0.0f || *u > det)
		return false;

	// Prepare to test V parameter
	CDXVec3 qvec;
	D3DXVec3Cross(&qvec, &tvec, &edge1);

	// Calculate V parameter and test bounds
	*v = D3DXVec3Dot(&dir, &qvec);
	if(*v < 0.0f || *u + *v > det)
		return false;

	// Calculate t, scale parameters, ray intersects triangle
	*t = D3DXVec3Dot(&edge2, &qvec);
	float fInvDet = 1.0f / det;
	*t *= fInvDet;
	*u *= fInvDet;
	*v *= fInvDet;

	return true;
}

bool CDXMesh::Pick(CDXRayInfo & rayInfo, CDXPickInfo & pickInfo)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	CDXMeshVert* pVertices = LockVertices();
	CDXMeshIndex* pIndices = LockIndices();

	if (!pVertices || !pIndices)
		return false;

	float hitDist = FLT_MAX;
	CDXVec3 hitNormal(0, 0, 0);

	// Edges = Face * 3
	for(UInt32 e = 0; e < m_primitiveCount * 3; e += 3)
	{
		CDXVec3 v0 = pVertices[pIndices[e + 0]].Position;
		CDXVec3 v1 = pVertices[pIndices[e + 1]].Position;
		CDXVec3 v2 = pVertices[pIndices[e + 2]].Position;

		// Calculate the norm of the face
		CDXVec3 fNormal(0,0,0);
		CDXVec3 f1 = v1 - v0;
		CDXVec3 f2 = v2 - v1;
		D3DXVec3Cross(&fNormal, &f1, &f2);
		D3DXVec3Normalize(&fNormal, &fNormal);

		// Normalize the direction, just in case
		CDXVec3 vDir = rayInfo.direction;
		D3DXVec3Normalize(&vDir, &vDir);

		// Skip faces that are in the same direction as the ray
		if(D3DXVec3Dot(&vDir, &fNormal) >= 0)
			continue;

		// Skip face that doesn't intersect with the ray
		float fDist = -1;
		float fBary1 = 0;
		float fBary2 = 0;
		if (!IntersectTriangle(rayInfo.origin, rayInfo.direction, v0, v1, v2, &fDist, &fBary1, &fBary2))
			continue;

		if (fDist < hitDist) {
			hitDist = fDist;
			hitNormal = fNormal;
		}
	}

	pickInfo.ray = rayInfo;
	pickInfo.dist = hitDist;

	if (hitDist != FLT_MAX) {
		CDXVec3 vHit = rayInfo.origin + rayInfo.direction * hitDist;
		pickInfo.origin = vHit;
		pickInfo.normal = hitNormal;
		pickInfo.isHit = true;
	}
	else {
		pickInfo.origin = CDXVec3(0, 0, 0);
		pickInfo.normal = CDXVec3(0, 0, 0);
		pickInfo.isHit = false;
	}

	UnlockVertices();
	UnlockIndices();
	return pickInfo.isHit;
}

CDXMeshVert* CDXMesh::LockVertices()
{
	CDXMeshVert* pVertices = NULL;
	if (!m_vertexBuffer)
		return NULL;

	if (FAILED(m_vertexBuffer->Lock(0, 0, (void**)&pVertices, D3DLOCK_DISCARD)))
		return NULL;

	return pVertices;
}

CDXMeshIndex* CDXMesh::LockIndices()
{
	CDXMeshIndex* pIndices = NULL;
	if (!m_indexBuffer)
		return NULL;

	if (FAILED(m_indexBuffer->Lock(0, 0, (void**)&pIndices, D3DLOCK_DISCARD)))
		return NULL;

	return pIndices;
}

void CDXMesh::UnlockVertices()
{
	m_vertexBuffer->Unlock();
}
void CDXMesh::UnlockIndices()
{
	m_indexBuffer->Unlock();
}

void CDXMesh::Render(LPDIRECT3DDEVICE9 pDevice, CDXShader * shader)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	ID3DXEffect * effect = shader->GetEffect();
	effect->SetMatrix(shader->m_hTransform, &m_transform);

	if (m_material && m_material->IsWireframe()) {
		effect->SetTechnique(effect->GetTechniqueByName("Wireframe"));
		effect->SetTexture(shader->m_hTexture, NULL);
	} else if (m_material && m_material->GetDiffuseTexture()) {
		effect->SetTechnique(effect->GetTechniqueByName("TexturedSpecular"));
		effect->SetTexture(shader->m_hTexture, m_material->GetDiffuseTexture());
	} else {
		effect->SetTechnique(effect->GetTechniqueByName("Specular"));
		effect->SetTexture(shader->m_hTexture, NULL);
	}

	effect->CommitChanges();

	UINT iPass, cPasses;
	effect->Begin(&cPasses, 0);
	for (iPass = 0; iPass < cPasses; iPass++)
	{
		effect->BeginPass(iPass);
		Pass(pDevice, iPass, shader);
		effect->EndPass();
	}

	effect->End();
}

void CDXMesh::Pass(LPDIRECT3DDEVICE9 pDevice, UInt32 iPass, CDXShader * shader)
{
	if (!m_vertexBuffer || !m_indexBuffer)
		return;

	pDevice->SetStreamSource(0, m_vertexBuffer, 0, sizeof(CDXMeshVert));
	pDevice->SetIndices(m_indexBuffer);
	pDevice->DrawIndexedPrimitive((D3DPRIMITIVETYPE)m_primitiveType, 0, 0, m_vertCount, 0, m_primitiveCount);
}

#endif