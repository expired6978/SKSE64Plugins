#include "CDXBrushMesh.h"
#include "CDXD3DDevice.h"
#include "CDXShader.h"
#include "CDXShaderCompile.h"
#include "CDXBrush.h"
#include "CDXShaderFactory.h"

using namespace DirectX;

bool CDXBrushMesh::Create(CDXD3DDevice * device, bool dashed, XMVECTOR ringColor, XMVECTOR dotColor, CDXShaderFactory* factory, CDXShaderFile* vertexShader, CDXShaderFile* precompiledVertexShader, CDXShaderFile* pixelShader, CDXShaderFile* precompiledPixelShader)
{
	static unsigned int NUM_VERTS = 360 / 5;
	static const float RADIUS = 1.0f;
	static const float RADIUS_STEP = 5.0f * (XM_PI / 180.0f);

	Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBuffer;
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBuffer;
	D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
	D3D11_SUBRESOURCE_DATA vertexData, indexData;
	HRESULT result;

	m_pDevice = device;
	if (!device) {
		_ERROR("%s - No device found to create brushes", __FUNCTION__);
		return false;
	}

	m_vertCount = NUM_VERTS;
	m_indexCount = NUM_VERTS;

	m_dashed = dashed;

	auto pDevice = device->GetDevice();
	if (!pDevice) {
		_ERROR("%s - No device3 found", __FUNCTION__);
		return false;
	}

	auto pDeviceContext = device->GetDeviceContext();
	if (!pDevice) {
		_ERROR("%s - No device deviceContext4 found", __FUNCTION__);
		return false;
	}

	// Create the vertex array.
	m_primitive = new ColoredPrimitive[m_vertCount];
	if (!m_primitive)
	{
		_ERROR("%s - Failed to initialize colored primitive with %d vertices", __FUNCTION__, m_vertCount);
		return false;
	}

	// Create the index array.
	m_indices = new CDXMeshIndex[m_indexCount];
	if (!m_indices)
	{
		_ERROR("%s - Failed to initialize %d indices", __FUNCTION__, m_indexCount);
		return false;
	}

	ComputeSphere(m_sphere.m_vertices, m_sphere.m_indices, 1.0f, 10, 10, dotColor);


	float i = 0;
	for (unsigned int j = 0; j < m_vertCount; j++)
	{
		m_primitive[j].Position = XMVectorSet(sin(i) * RADIUS, cos(i) * RADIUS, 0, 0);
		m_primitive[j].Color = ringColor;
		
		i += RADIUS_STEP;
	}

	for (unsigned int j = 0; j < m_vertCount; j++)
	{
		m_indices[j] = j;
	}

	// Set up the description of the static vertex buffer.
	vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexBufferDesc.ByteWidth = sizeof(ColoredPrimitive) * m_vertCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the vertex data.
	vertexData.pSysMem = m_primitive;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	// Now create the vertex buffer.
	result = pDevice->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBuffer);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create vertex buffer 1", __FUNCTION__);
		return false;
	}

	vertexData.pSysMem = &m_sphere.m_vertices.at(0);
	vertexBufferDesc.ByteWidth = sizeof(ColoredPrimitive) * m_sphere.m_vertices.size();

	// Now create the vertex buffer.
	result = pDevice->CreateBuffer(&vertexBufferDesc, &vertexData, &m_sphere.m_vertexBuffer);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create vertex buffer 2", __FUNCTION__);
		return false;
	}

	// Set up the description of the static index buffer.
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(CDXMeshIndex) * m_indexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the index data.
	indexData.pSysMem = m_indices;
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	// Create the index buffer.
	result = pDevice->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create index buffer 1", __FUNCTION__);
		return false;
	}

	indexData.pSysMem = &m_sphere.m_indices.at(0);
	indexBufferDesc.ByteWidth = sizeof(CDXMeshIndex) * m_sphere.m_indices.size();

	// Create the index buffer.
	result = pDevice->CreateBuffer(&indexBufferDesc, &indexData, &m_sphere.m_indexBuffer);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create index buffer 2", __FUNCTION__);
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC polygonLayout[2];
	polygonLayout[0].SemanticName = "POSITION";
	polygonLayout[0].SemanticIndex = 0;
	polygonLayout[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	polygonLayout[0].InputSlot = 0;
	polygonLayout[0].AlignedByteOffset = 0;
	polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[0].InstanceDataStepRate = 0;

	polygonLayout[1].SemanticName = "COLOR";
	polygonLayout[1].SemanticIndex = 0;
	polygonLayout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	polygonLayout[1].InputSlot = 0;
	polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[1].InstanceDataStepRate = 0;

	// Get a count of the elements in the layout.
	unsigned int numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

	// BrushVShader
	if (!factory->CreateVertexShader(device, vertexShader, precompiledVertexShader, polygonLayout, numElements, m_vertexShader, m_layout))
	{
		return false;
	}

	if (!factory->CreatePixelShader(device, pixelShader, precompiledPixelShader, m_pixelShader))
	{
		return false;
	}

	// Setup the raster description which will determine how and what polygons will be drawn.
	D3D11_RASTERIZER_DESC rasterDesc;
	rasterDesc.AntialiasedLineEnable = true;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = -2000;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = true;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	// Create the rasterizer state from the description we just filled out.
	result = pDevice->CreateRasterizerState(&rasterDesc, &m_solidState);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create rasterizer state", __FUNCTION__);
		return false;
	}

	return true;
}

void CDXBrushMesh::ComputeSphere(std::vector<CDXMesh::ColoredPrimitive>& vertices, std::vector<CDXMeshIndex> & indices, float radius, int sliceCount, int stackCount, DirectX::XMVECTOR color)
{
	CDXMesh::ColoredPrimitive vertex;
	vertex.Position = XMVectorSet(0, radius, 0, 0);
	vertex.Color = color;
	vertices.push_back(vertex);

	float phiStep = XM_PI / stackCount;
	float thetaStep = XM_2PI / sliceCount;

	for (int i = 1; i <= stackCount - 1; i++) {
		float phi = i * phiStep;
		for (int j = 0; j <= sliceCount; j++) {
			float theta = j * thetaStep;
			XMVECTOR p = XMVectorSet(
				(radius*sin(phi)*cos(theta)),
				(radius*cos(phi)),
				(radius*sin(phi)*sin(theta)),
				0
			);

			//p = XMVector3Rotate(p, XMQuaternionRotationRollPitchYaw(0, 0, XM_PI));

			vertex.Position = p;
			vertices.push_back(vertex);
		}
	}

	vertex.Position = XMVectorSet(0, -radius, 0, 0);
	vertices.push_back(vertex);

	for (int i = 1; i <= sliceCount; i++) {
		indices.push_back(0);
		indices.push_back(i + 1);
		indices.push_back(i);
	}
	int baseIndex = 1;
	int ringVertexCount = sliceCount + 1;
	for (int i = 0; i < stackCount - 2; i++) {
		for (int j = 0; j < sliceCount; j++) {
			indices.push_back(baseIndex + i * ringVertexCount + j);
			indices.push_back(baseIndex + i * ringVertexCount + j + 1);
			indices.push_back(baseIndex + (i + 1)*ringVertexCount + j);

			indices.push_back(baseIndex + (i + 1)*ringVertexCount + j);
			indices.push_back(baseIndex + i * ringVertexCount + j + 1);
			indices.push_back(baseIndex + (i + 1)*ringVertexCount + j + 1);
		}
	}
	int southPoleIndex = vertices.size() - 1;
	baseIndex = southPoleIndex - ringVertexCount;
	for (int i = 0; i < sliceCount; i++) {
		indices.push_back(southPoleIndex);
		indices.push_back(baseIndex + i);
		indices.push_back(baseIndex + i + 1);
	}

	/*
	vertices.clear();
	indices.clear();

	int tessellation = sliceCount;
	float diameter = radius * 2.0f;

	if (tessellation < 3)
		throw std::out_of_range("tesselation parameter out of range");

	size_t verticalSegments = tessellation;
	size_t horizontalSegments = tessellation * 2;

	// Create rings of vertices at progressively higher latitudes.
	for (size_t i = 0; i <= verticalSegments; i++)
	{
		//float v = 1 - float(i) / verticalSegments;

		float latitude = (i * XM_PI / verticalSegments) - XM_PIDIV2;
		float dy, dxz;

		XMScalarSinCos(&dy, &dxz, latitude);

		// Create a single ring of vertices at this latitude.
		for (size_t j = 0; j <= horizontalSegments; j++)
		{
			//float u = float(j) / horizontalSegments;

			float longitude = j * XM_2PI / horizontalSegments;
			float dx, dz;

			XMScalarSinCos(&dx, &dz, longitude);

			dx *= dxz;
			dz *= dxz;

			XMVECTOR normal = XMVectorSet(dx, dy, dz, 0);
			//XMVECTOR textureCoordinate = XMVectorSet(u, v, 0, 0);

			CDXMesh::ColoredPrimitive prim;
			prim.Position = XMVectorScale(normal, radius);
			prim.Color = color;
			vertices.push_back(prim);
		}
	}

	// Fill the index buffer with triangles joining each pair of latitude rings.
	size_t stride = horizontalSegments + 1;

	for (size_t i = 0; i < verticalSegments; i++)
	{
		for (size_t j = 0; j <= horizontalSegments; j++)
		{
			size_t nextI = i + 1;
			size_t nextJ = (j + 1) % stride;

			indices.push_back(i * stride + j);
			indices.push_back(nextI * stride + j);
			indices.push_back(i * stride + nextJ);

			indices.push_back(i * stride + nextJ);
			indices.push_back(nextI * stride + j);
			indices.push_back(nextI * stride + nextJ);
		}
	}*/
}

void CDXBrushMesh::Render(CDXD3DDevice * pDevice, CDXShader * shader)
{
	unsigned int stride = sizeof(ColoredPrimitive);
	unsigned int offset = 0;

	auto pDeviceContext = pDevice->GetDeviceContext();

	pDeviceContext->IASetInputLayout(m_layout.Get());
	pDeviceContext->RSSetState(m_solidState.Get());
	pDeviceContext->VSSetShader(m_vertexShader.Get(), NULL, 0);
	pDeviceContext->PSSetShader(m_pixelShader.Get(), NULL, 0);
	pDeviceContext->GSSetShader(nullptr, nullptr, 0);

	CDXShader::TransformBuffer xform;

	ID3D11Buffer* vertexBuffer[] = { nullptr };
	if (m_drawRadius)
	{
		xform.transform = XMMatrixTranspose(GetTransform());
		shader->VSSetTransformBuffer(pDevice, xform);

		vertexBuffer[0] = m_vertexBuffer.Get();
		pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffer, &stride, &offset);
		pDeviceContext->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
		pDeviceContext->IASetPrimitiveTopology(m_dashed ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST : D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
		pDeviceContext->DrawIndexed(m_indexCount, 0, 0);
	}

	if (m_drawPoint)
	{
		xform.transform = XMMatrixTranspose(GetSphereTransform());
		shader->VSSetTransformBuffer(pDevice, xform);

		vertexBuffer[0] = m_sphere.m_vertexBuffer.Get();
		pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffer, &stride, &offset);
		pDeviceContext->IASetIndexBuffer(m_sphere.m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
		pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		pDeviceContext->DrawIndexed(m_sphere.m_indices.size(), 0, 0);
	}
	
}

bool CDXBrushMesh::Pick(CDXRayInfo & rayInfo, CDXPickInfo & pickInfo)
{
	return false;
}

bool CDXBrushTranslator::Pick(CDXPickInfo & pickInfo, CDXMesh * mesh, bool isMirror)
{
	auto brush = isMirror ? m_brushMeshMirror : m_brushMesh;

	if (isMirror && !m_brush->IsMirror()) {
		brush->SetVisible(false);
		return false;
	}

	if (mesh->IsEditable())
	{
		if (pickInfo.isHit)
		{
			double scale = m_brush->GetProperty(CDXBrush::BrushProperty::kBrushProperty_Radius, CDXBrush::BrushPropertyValue::kBrushPropertyValue_Value);

			auto n2 = pickInfo.normal;
			auto n1 = XMVectorSet(0, 0, 1, 0);
			auto rotation = XMQuaternionRotationMatrix(XMMatrixRotationAxis(XMVector3Cross(n1, n2), acos(XMVectorGetX(XMVector3Dot(n1, n2)))));
			auto offset = XMVectorMultiplyAdd(XMVectorReplicate(0.05), n2, pickInfo.origin);

			CDXMatrix transform1 = XMMatrixAffineTransformation(XMVectorReplicate(scale), XMVectorZero(), rotation, offset);
			CDXMatrix transform2 = XMMatrixAffineTransformation(XMVectorReplicate(1.0f/16.0f), XMVectorZero(), rotation, offset);

			brush->SetTransform(transform1);
			brush->SetSphereTransform(transform2);
			brush->SetVisible(true);
			return true;
		}
	}

	brush->SetVisible(false);
	return false;
}

bool CDXBrushTranslator::Mirror() const
{
	return true;
}