#include "CDXD3DDevice.h"
#include "CDXMesh.h"
#include "CDXShader.h"
#include "CDXCamera.h"
#include "CDXMaterial.h"
#include "CDXPicker.h"
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>



using namespace DirectX;

CDXMesh::CDXMesh()
{
	m_vertexBuffer = nullptr;
	m_vertCount = 0;
	m_indexBuffer = nullptr;
	m_indexCount = 0;
	m_visible = true;
	m_material = nullptr;
	m_transform = XMMatrixIdentity();
	m_topology = REX::W32::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

CDXMesh::~CDXMesh()
{

}

void CDXMesh::SetMaterial(const std::shared_ptr<CDXMaterial>& material)
{
	std::lock_guard guard(m_dataMutex);
	m_material = material;
}

std::shared_ptr<CDXMaterial> CDXMesh::GetMaterial()
{
	std::lock_guard guard(m_dataMutex);
	return m_material;
}

void CDXMesh::SetVisible(bool visible)
{
	std::lock_guard guard(m_dataMutex);
	m_visible = visible;
}

bool CDXMesh::IsVisible() const
{
	std::lock_guard guard(m_dataMutex);
	return m_visible;
}

REX::W32::ComPtr<REX::W32::ID3D11Buffer> CDXMesh::GetVertexBuffer()
{
	std::lock_guard guard(m_dataMutex);
	return m_vertexBuffer;
}
REX::W32::ComPtr<REX::W32::ID3D11Buffer> CDXMesh::GetIndexBuffer()
{
	std::lock_guard guard(m_dataMutex);
	return m_indexBuffer;
}

std::uint32_t CDXMesh::GetIndexCount()
{
	std::lock_guard guard(m_dataMutex);
	return m_indexCount;
}

std::uint32_t CDXMesh::GetFaceCount()
{
	std::lock_guard guard(m_dataMutex);
	return m_topology == REX::W32::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST ? m_indexCount / 3 : 0;
}

std::uint32_t CDXMesh::GetVertexCount()
{
	std::lock_guard guard(m_dataMutex);
	return m_vertCount;
}

XMVECTOR CalculateFaceNormal(std::uint32_t f, CDXMeshIndex * faces, CDXMeshVert * vertices)
{
	XMVECTOR vNormal;
	CDXMeshVert * v1 = &vertices[faces[f]];
	CDXMeshVert * v2 = &vertices[faces[f + 1]];
	CDXMeshVert * v3 = &vertices[faces[f + 2]];

	auto e1 = XMVectorSubtract(XMLoadFloat3(&v2->Position), XMLoadFloat3(&v1->Position));
	auto e2 = XMVectorSubtract(XMLoadFloat3(&v3->Position), XMLoadFloat3(&v2->Position));

	vNormal = XMVector3Cross(e1, e2);
	vNormal = XMVector3Normalize(vNormal);
	return vNormal;
}

bool IntersectSphere(float radius, float & dist, CDXVec & center, CDXVec & rayOrigin, CDXVec & rayDir)
{
	//FLOAT t0 = -1, t1 = -1; // solutions for t if the ray intersects

	CDXVec L = XMVectorSubtract(center, rayOrigin);
	float tca = XMVectorGetX(XMVector3Dot(L, rayDir));
	if (tca < 0) return false;
	float d2 = XMVector3Dot(L, L).m128_f32[0] - tca * tca;
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
	CDXVec edge1 = XMVectorSubtract(v1, v0);
	CDXVec edge2 = XMVectorSubtract(v2, v0);

	// Begin calculating determinant - also used to calculate U parameter
	CDXVec pvec = XMVector3Cross(dir, edge2);

	// If determinant is near zero, ray lies in plane of triangle
	float det = XMVectorGetX(XMVector3Dot(edge1, pvec));

	CDXVec tvec;
	if(det > 0) {
		tvec = XMVectorSubtract(orig, v0);
	} else {
		tvec = XMVectorSubtract(v0, orig);
		det = -det;
	}

	if(det < 0.0001f)
		return false;

	// Calculate U parameter and test bounds
	*u = XMVectorGetX(XMVector3Dot(tvec, pvec));
	if(*u < 0.0f || *u > det)
		return false;

	// Prepare to test V parameter
	CDXVec qvec = XMVector3Cross(tvec, edge1);

	// Calculate V parameter and test bounds
	*v = XMVectorGetX(XMVector3Dot(dir, qvec));
	if(*v < 0.0f || *u + *v > det)
		return false;

	// Calculate t, scale parameters, ray intersects triangle
	*t = XMVectorGetX(XMVector3Dot(edge2, qvec));
	float fInvDet = 1.0f / det;
	*t *= fInvDet;
	*u *= fInvDet;
	*v *= fInvDet;

	return true;
}

CDXMeshVert * CDXMesh::LockVertices(const LockMode type)
{
	m_dataMutex.lock();
	// CPU-owned authoritative copy. D3D write mappings can be write-combined:
	// reading them during picking or read/modify/write strokes is very costly.
	return m_vertices.get();
}

CDXMeshIndex * CDXMesh::LockIndices()
{
	m_dataMutex.lock();
	return m_indices.get();
}

void CDXMesh::UnlockVertices(const LockMode type)
{
	if (type == LockMode::WRITE && m_vertices) m_verticesDirty = true;
	m_dataMutex.unlock();
}

bool CDXMesh::FlushVertices()
{
	std::lock_guard guard(m_dataMutex);
	// Validate clean meshes too, before authorizing a draw.
	if (!m_pDevice || !m_vertexBuffer.Get() || !m_indexBuffer.Get() ||
		!m_vertices || !m_indices || !m_vertCount || !m_indexCount) return false;
	auto context = m_pDevice->GetDeviceContext();
	if (!context.Get()) return false;
	if (!m_verticesDirty) return true;
	REX::W32::D3D11_MAPPED_SUBRESOURCE resource{};
	if (FAILED(MapVertexBuffer(context.Get(), resource)))
		return false; // Keep dirty; retry on the next render, without losing edits.
	if (!resource.data) {
		context->Unmap(m_vertexBuffer.Get(), 0);
		return false; // A malformed success cannot discard the authoritative edit.
	}
	std::memcpy(resource.data, m_vertices.get(), sizeof(CDXMeshVert) * m_vertCount);
	context->Unmap(m_vertexBuffer.Get(), 0);
	m_verticesDirty = false;
	return true;
}

HRESULT CDXMesh::MapVertexBuffer(REX::W32::ID3D11DeviceContext* context,
	REX::W32::D3D11_MAPPED_SUBRESOURCE& resource)
{
	return context->Map(m_vertexBuffer.Get(), 0, REX::W32::D3D11_MAP_WRITE_DISCARD, 0, &resource);
}

HRESULT CDXMesh::CreateMeshBuffer(REX::W32::ID3D11Device* device,
	const REX::W32::D3D11_BUFFER_DESC& desc, const REX::W32::D3D11_SUBRESOURCE_DATA& data,
	REX::W32::ID3D11Buffer** result)
{
	return device->CreateBuffer(&desc, &data, result);
}

void CDXMesh::UnlockIndices(bool write)
{
	if (write && m_pDevice && m_indexBuffer.Get() && m_indices) {
		auto context = m_pDevice->GetDeviceContext();
		if (context.Get()) context->UpdateSubresource(m_indexBuffer.Get(), 0, nullptr, m_indices.get(), 0, 0);
	}
	m_dataMutex.unlock();
}

bool CDXMesh::Pick(CDXRayInfo & rayInfo, CDXPickInfo & pickInfo)
{
	std::lock_guard guard(m_dataMutex);
	pickInfo.isHit = false;
	if (m_topology != REX::W32::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST || m_indexCount % 3) return false;
	CDXMesh::VertexAccess vertexAccess(*this, LockMode::READ);
	auto* pVertices = vertexAccess.Get();
	CDXMesh::IndexAccess indexAccess(*this);
	auto* pIndices = indexAccess.Get();

	if (!pVertices || !pIndices) {
		vertexAccess.Release();
		indexAccess.Release();
		return false;
	}

	float hitDist = FLT_MAX;
	CDXVec hitNormal = XMVectorZero();

	// Edges = Face * 3
	for(std::uint32_t e = 0; e < m_indexCount; e += 3)
	{
		if (pIndices[e] >= m_vertCount || pIndices[e + 1] >= m_vertCount || pIndices[e + 2] >= m_vertCount) return false;
		CDXVec v0 = XMVector3Transform(XMLoadFloat3(&pVertices[pIndices[e + 0]].Position), m_transform);
		CDXVec v1 = XMVector3Transform(XMLoadFloat3(&pVertices[pIndices[e + 1]].Position), m_transform);
		CDXVec v2 = XMVector3Transform(XMLoadFloat3(&pVertices[pIndices[e + 2]].Position), m_transform);

		// Calculate the norm of the face
		CDXVec fNormal = XMVectorZero();
		CDXVec f1 = XMVectorSubtract(v1, v0);
		CDXVec f2 = XMVectorSubtract(v2, v1);
		fNormal = XMVector3Cross(f1, f2);
		fNormal = XMVector3Normalize(fNormal);

		// Normalize the direction, just in case
		CDXVec vDir = XMVector3Normalize(rayInfo.direction);

		// Skip faces that are in the same direction as the ray
		if(XMVectorGetX(XMVector3Dot(vDir, fNormal)) >= 0)
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

	vertexAccess.Release();
	indexAccess.Release();

	pickInfo.ray = rayInfo;
	pickInfo.dist = hitDist;

	if (hitDist != FLT_MAX) {
		CDXVec hitVec = XMVectorReplicate(hitDist);
		CDXVec vHit = XMVectorMultiplyAdd(rayInfo.direction, hitVec, rayInfo.origin);
		pickInfo.origin = XMVectorSetW(vHit, 0.0f);
		pickInfo.normal = hitNormal;
		pickInfo.isHit = true;
	}
	else {
		pickInfo.origin = XMVectorZero();
		pickInfo.normal = XMVectorZero();
		pickInfo.isHit = false;
	}

	return pickInfo.isHit;
}

bool CDXMesh::InitializeBuffers(CDXD3DDevice * device, std::uint32_t vertexCount, std::uint32_t indexCount, std::function<void(CDXMeshVert*, CDXMeshIndex*)> fillFunction)
{
	std::lock_guard guard(m_dataMutex);
	REX::W32::D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
	REX::W32::D3D11_SUBRESOURCE_DATA vertexData, indexData;
	HRESULT result;

	if (!device || !fillFunction || !vertexCount || !indexCount || indexCount % 3 ||
		// Existing stroke/mask iterators and engine vertex counts are 16-bit.
		vertexCount > static_cast<std::uint32_t>((std::numeric_limits<CDXMeshIndex>::max)()) ||
		vertexCount > (std::numeric_limits<std::uint32_t>::max)() / sizeof(CDXMeshVert) ||
		indexCount > (std::numeric_limits<std::uint32_t>::max)() / sizeof(CDXMeshIndex)) {
		SKSE::log::error("{} - Invalid device, fill callback or mesh counts", __FUNCTION__);
		return false;
	}

	auto pDevice = device->GetDevice();
	if (!pDevice.Get()) {
		SKSE::log::error("{} - No device3 found", __FUNCTION__);
		return false;
	}

	auto pDeviceContext = device->GetDeviceContext();
	if (!pDeviceContext.Get()) {
		SKSE::log::error("{} - No device deviceContext4 found", __FUNCTION__);
		return false;
	}

	// Create the vertex array.
	auto vertices = std::unique_ptr<CDXMeshVert[]>(new (std::nothrow) CDXMeshVert[vertexCount]{});
	if (!vertices)
	{
		SKSE::log::error("{} - Failed to create vertex array", __FUNCTION__);
		return false;
	}

	// Create the index array.
	auto indices = std::unique_ptr<CDXMeshIndex[]>(new (std::nothrow) CDXMeshIndex[indexCount]{});
	if (!indices)
	{
		SKSE::log::error("{} - Failed to create index array", __FUNCTION__);
		return false;
	}

	// Load the vertex array and index array with data.
	try { fillFunction(vertices.get(), indices.get()); }
	catch (const std::exception& e) {
		SKSE::log::error("{} - Mesh fill failed: {}", __FUNCTION__, e.what());
		return false;
	}
	catch (...) {
		SKSE::log::error("{} - Mesh fill failed with a non-standard exception", __FUNCTION__);
		return false;
	}
	for (std::uint32_t i = 0; i < indexCount; ++i) {
		if (indices[i] >= vertexCount) return false;
	}
	REX::W32::ComPtr<REX::W32::ID3D11Buffer> vertexBuffer, indexBuffer;
	
	// Set up the description of the static vertex buffer.
	vertexBufferDesc.usage = REX::W32::D3D11_USAGE_DYNAMIC;
	vertexBufferDesc.byteWidth = static_cast<std::uint32_t>(sizeof(CDXMeshVert) * vertexCount);
	vertexBufferDesc.bindFlags = REX::W32::D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.cpuAccessFlags = REX::W32::D3D11_CPU_ACCESS_WRITE;
	vertexBufferDesc.miscFlags = 0;
	vertexBufferDesc.structureByteStride = 0;

	// Give the subresource structure a pointer to the vertex data.
	vertexData.sysMem = vertices.get();
	vertexData.sysMemPitch = 0;
	vertexData.sysMemSlicePitch = 0;

	// Now create the vertex buffer.
	result = CreateMeshBuffer(pDevice.Get(), vertexBufferDesc, vertexData, vertexBuffer.GetAddressOf());
	if (FAILED(result) || !vertexBuffer.Get())
	{
		SKSE::log::error("{} - Failed to create vertex buffer", __FUNCTION__);
		return false;
	}

	// Set up the description of the static index buffer.
	indexBufferDesc.usage = REX::W32::D3D11_USAGE_DEFAULT;
	indexBufferDesc.byteWidth = static_cast<std::uint32_t>(sizeof(CDXMeshIndex) * indexCount);
	indexBufferDesc.bindFlags = REX::W32::D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.cpuAccessFlags = 0;
	indexBufferDesc.miscFlags = 0;
	indexBufferDesc.structureByteStride = 0;

	// Give the subresource structure a pointer to the index data.
	indexData.sysMem = indices.get();
	indexData.sysMemPitch = 0;
	indexData.sysMemSlicePitch = 0;

	// Create the index buffer.
	result = CreateMeshBuffer(pDevice.Get(), indexBufferDesc, indexData, indexBuffer.GetAddressOf());
	if (FAILED(result) || !indexBuffer.Get())
	{
		SKSE::log::error("{} - Failed to create index buffer", __FUNCTION__);
		return false;
	}

	// Publish only after all allocations and fill succeeded. Failed replacement
	// leaves the old mesh, device and any pending edits intact.
	m_pDevice = device;
	m_vertCount = vertexCount;
	m_indexCount = indexCount;
	m_vertices = std::move(vertices);
	m_indices = std::move(indices);
	m_topology = REX::W32::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_vertexBuffer = std::move(vertexBuffer);
	m_indexBuffer = std::move(indexBuffer);
	m_verticesDirty = false;
	return true;
}

void CDXMesh::Render(CDXD3DDevice * device, CDXShader * shader)
{
	std::lock_guard dataGuard(m_dataMutex);
	if (!device || device != m_pDevice || !shader || !device->GetDeviceContext().Get()) return;
	if (!FlushVertices()) return;
	unsigned int stride;
	unsigned int offset;

	// Set vertex buffer stride and offset.
	stride = sizeof(CDXMeshVert);
	offset = 0;

	auto pDeviceContext = device->GetDeviceContext();

	CDXShader::TransformBuffer xform;
	xform.transform = DirectX::XMMatrixTranspose(GetTransform());
	shader->VSSetTransformBuffer(device, xform);

	// Set the vertex buffer to active in the input assembler so it can be rendered.
	REX::W32::ID3D11Buffer* vertexBuffer[] = { m_vertexBuffer.Get() };
	pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffer, &stride, &offset);

	// Set the index buffer to active in the input assembler so it can be rendered.
	pDeviceContext->IASetIndexBuffer(m_indexBuffer.Get(), REX::W32::DXGI_FORMAT_R16_UINT, 0);

	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	pDeviceContext->IASetPrimitiveTopology(m_topology);

	shader->RenderShader(device, m_material);

	// Render the triangle.
	pDeviceContext->DrawIndexed(m_indexCount, 0, 0);
}
