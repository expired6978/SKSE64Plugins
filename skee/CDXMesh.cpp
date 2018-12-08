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
	//m_material = NULL;
	//m_primitiveType = D3DPT_TRIANGLELIST;

	m_transform = DirectX::XMMatrixIdentity();
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
	/*if(m_material) {
		m_material->Release();
		delete m_material;
		m_material = NULL;
	}*/
}

void CDXMesh::SetMaterial(CDXMaterial * material)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	//m_material = material;
}

CDXMaterial * CDXMesh::GetMaterial()
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return nullptr;//m_material;
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

ID3D11Buffer * CDXMesh::GetVertexBuffer()
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return m_vertexBuffer;
}
ID3D11Buffer * CDXMesh::GetIndexBuffer()
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

DirectX::XMVECTOR CalculateFaceNormal(UInt32 f, CDXMeshIndex * faces, CDXMeshVert * vertices)
{
	DirectX::XMVECTOR vNormal;
	CDXMeshVert * v1 = &vertices[faces[f]];
	CDXMeshVert * v2 = &vertices[faces[f + 1]];
	CDXMeshVert * v3 = &vertices[faces[f + 2]];

	auto e1 = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&v2->Position), DirectX::XMLoadFloat3(&v1->Position));
	auto e2 = DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&v3->Position), DirectX::XMLoadFloat3(&v2->Position));

	vNormal = DirectX::XMVector3Cross(e1, e2);
	vNormal = DirectX::XMVector3Normalize(vNormal);
	return vNormal;
}

bool IntersectSphere(float radius, float & dist, CDXVec & center, CDXVec & rayOrigin, CDXVec & rayDir)
{
	//FLOAT t0 = -1, t1 = -1; // solutions for t if the ray intersects

	CDXVec L = DirectX::XMVectorSubtract(center, rayOrigin);
	float tca = DirectX::XMVector3Dot(L, rayDir).m128_f32[0];
	if (tca < 0) return false;
	float d2 = DirectX::XMVector3Dot(L, L).m128_f32[0] - tca * tca;
	if (d2 > radius) return false;
	float thc = sqrt(radius - d2);
	//t0 = tca - thc;
	//t1 = tca + thc;
	dist = d2;
	return true;
}

bool IntersectTriangle( const CDXVec& orig, const CDXVec& dir, CDXVec& v0, CDXVec& v1, CDXVec& v2, float* t, float* u, float* v )
{
	// Find vectors for two edges sharing vert0
	CDXVec edge1 = DirectX::XMVectorSubtract(v1, v0);
	CDXVec edge2 = DirectX::XMVectorSubtract(v2, v0);

	// Begin calculating determinant - also used to calculate U parameter
	CDXVec pvec = DirectX::XMVector3Cross(dir, edge2);

	// If determinant is near zero, ray lies in plane of triangle
	float det = DirectX::XMVector3Dot(edge1, pvec).m128_f32[0];

	CDXVec tvec;
	if(det > 0) {
		tvec = DirectX::XMVectorSubtract(orig, v0);
	} else {
		tvec = DirectX::XMVectorSubtract(v0, orig);
		det = -det;
	}

	if(det < 0.0001f)
		return false;

	// Calculate U parameter and test bounds
	*u = DirectX::XMVector3Dot(tvec, pvec).m128_f32[0];
	if(*u < 0.0f || *u > det)
		return false;

	// Prepare to test V parameter
	CDXVec qvec = DirectX::XMVector3Cross(tvec, edge1);

	// Calculate V parameter and test bounds
	*v = DirectX::XMVector3Dot(dir, qvec).m128_f32[0];
	if(*v < 0.0f || *u + *v > det)
		return false;

	// Calculate t, scale parameters, ray intersects triangle
	*t = DirectX::XMVector3Dot(edge2, qvec).m128_f32[0];
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
	CDXVec hitNormal = DirectX::XMVectorZero();

	// Edges = Face * 3
	for(UInt32 e = 0; e < m_primitiveCount * 3; e += 3)
	{
		CDXVec v0 = DirectX::XMLoadFloat3(&pVertices[pIndices[e + 0]].Position);
		CDXVec v1 = DirectX::XMLoadFloat3(&pVertices[pIndices[e + 1]].Position);
		CDXVec v2 = DirectX::XMLoadFloat3(&pVertices[pIndices[e + 2]].Position);

		// Calculate the norm of the face
		CDXVec fNormal = DirectX::XMVectorZero();
		CDXVec f1 = DirectX::XMVectorSubtract(v1, v0);
		CDXVec f2 = DirectX::XMVectorSubtract(v2, v1);
		fNormal = DirectX::XMVector3Cross(f1, f2);
		fNormal = DirectX::XMVector3Normalize(fNormal);

		// Normalize the direction, just in case
		CDXVec vDir = DirectX::XMVector3Normalize(rayInfo.direction);

		// Skip faces that are in the same direction as the ray
		if(DirectX::XMVector3Dot(vDir, fNormal).m128_f32[0] >= 0)
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
		CDXVec hitVec = DirectX::XMVectorReplicate(hitDist);
		CDXVec vHit = DirectX::XMVectorMultiplyAdd(rayInfo.direction, hitVec, rayInfo.origin);
		pickInfo.origin = vHit;
		pickInfo.normal = hitNormal;
		pickInfo.isHit = true;
	}
	else {
		pickInfo.origin = DirectX::XMVectorZero();
		pickInfo.normal = DirectX::XMVectorZero();
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

	/*if (FAILED(m_vertexBuffer->Lock(0, 0, (void**)&pVertices, D3DLOCK_DISCARD)))
		return NULL;*/

	return pVertices;
}

CDXMeshIndex* CDXMesh::LockIndices()
{
	CDXMeshIndex* pIndices = NULL;
	if (!m_indexBuffer)
		return NULL;

	/*if (FAILED(m_indexBuffer->Lock(0, 0, (void**)&pIndices, D3DLOCK_DISCARD)))
		return NULL;*/

	return pIndices;
}

void CDXMesh::UnlockVertices()
{
	//m_vertexBuffer->Unlock();
}
void CDXMesh::UnlockIndices()
{
	//m_indexBuffer->Unlock();
}

void CDXMesh::Render(ID3D11Device * pDevice, CDXShader * shader)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	/*ID3DXEffect * effect = shader->GetEffect();
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

	effect->End();*/
}

void CDXMesh::Pass(ID3D11Device * pDevice, UInt32 iPass, CDXShader * shader)
{
	if (!m_vertexBuffer || !m_indexBuffer)
		return;

	//pDevice->SetStreamSource(0, m_vertexBuffer, 0, sizeof(CDXMeshVert));
	//pDevice->SetIndices(m_indexBuffer);
	//pDevice->DrawIndexedPrimitive((D3DPRIMITIVETYPE)m_primitiveType, 0, 0, m_vertCount, 0, m_primitiveCount);
}
#endif