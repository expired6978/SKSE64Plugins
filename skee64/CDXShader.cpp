#include "CDXD3DDevice.h"
#include "CDXShader.h"
#include "CDXShaderCompile.h"
#include "CDXMaterial.h"

#include <sstream>

using namespace DirectX;

#define USE_GEOMETRY_WIREFRAME

CDXShader::CDXShader()
{

}

bool CDXShader::Initialize(const CDXInitParams & initParams)
{
	HRESULT result;
	D3D11_INPUT_ELEMENT_DESC polygonLayout[4];
	unsigned int numElements;
	D3D11_SAMPLER_DESC samplerDesc;
	D3D11_BUFFER_DESC bufferDesc;

	auto pDevice = initParams.device->GetDevice();

	// Create the vertex input layout description.
	// This setup needs to match the VertexType stucture in the ModelClass and in the shader.
	polygonLayout[0].SemanticName = "POSITION";
	polygonLayout[0].SemanticIndex = 0;
	polygonLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polygonLayout[0].InputSlot = 0;
	polygonLayout[0].AlignedByteOffset = 0;
	polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[0].InstanceDataStepRate = 0;

	polygonLayout[1].SemanticName = "TEXCOORD";
	polygonLayout[1].SemanticIndex = 0;
	polygonLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	polygonLayout[1].InputSlot = 0;
	polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[1].InstanceDataStepRate = 0;

	polygonLayout[2].SemanticName = "NORMAL";
	polygonLayout[2].SemanticIndex = 0;
	polygonLayout[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polygonLayout[2].InputSlot = 0;
	polygonLayout[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[2].InstanceDataStepRate = 0;

	polygonLayout[3].SemanticName = "COLOR";
	polygonLayout[3].SemanticIndex = 0;
	polygonLayout[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	polygonLayout[3].InputSlot = 0;
	polygonLayout[3].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[3].InstanceDataStepRate = 0;

	// Get a count of the elements in the layout.
	numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

	// LightVertexShader
	if (!initParams.factory->CreateVertexShader(initParams.device, initParams.vShader[0], initParams.vShader[1], polygonLayout, numElements, m_vertexShader, m_layout))
	{
		return false;
	}

	// LightPixelShader
	if (!initParams.factory->CreatePixelShader(initParams.device, initParams.pShader[0], initParams.pShader[1], m_pixelShader))
	{
		return false;
	}

	// WireframeVertexShader
	if (!initParams.factory->CreateVertexShader(initParams.device, initParams.wvShader[0], initParams.wvShader[1], polygonLayout, numElements, m_wvShader, m_layout))
	{
		return false;
	}

	// WireframeGeometryShader
	if (!initParams.factory->CreateGeometryShader(initParams.device, initParams.wgShader[0], initParams.wgShader[1], m_wgShader))
	{
		return false;
	}

	// WireframePixelShader
	if (!initParams.factory->CreatePixelShader(initParams.device, initParams.wpShader[0], initParams.wpShader[1], m_wpShader))
	{
		return false;
	}

	// Create a texture sampler state description.
	samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 16;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.BorderColor[0] = 0;
	samplerDesc.BorderColor[1] = 0;
	samplerDesc.BorderColor[2] = 0;
	samplerDesc.BorderColor[3] = 0;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create the texture sampler state.
	result = pDevice->CreateSamplerState(&samplerDesc, m_sampleState.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create sampler state", __FUNCTION__);
		return false;
	}

	// Setup the description of the matrix dynamic constant buffer that is in the vertex shader.
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;
	bufferDesc.ByteWidth = sizeof(VertexBuffer);

	// Create the matrix constant buffer pointer so we can access the vertex shader constant buffer from within this class.
	result = pDevice->CreateBuffer(&bufferDesc, NULL, m_matrixBuffer.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create matrix buffer", __FUNCTION__);
		return false;
	}

	bufferDesc.ByteWidth = sizeof(TransformBuffer);

	// Create the matrix constant buffer pointer so we can access the vertex shader constant buffer from within this class.
	result = pDevice->CreateBuffer(&bufferDesc, NULL, m_transformBuffer.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create transform buffer", __FUNCTION__);
		return false;
	}

	// Setup the description of the light dynamic constant buffer that is in the pixel shader.
	// Note that ByteWidth always needs to be a multiple of 16 if using D3D11_BIND_CONSTANT_BUFFER or CreateBuffer will fail.
	bufferDesc.ByteWidth = sizeof(MaterialBuffer);

	// Create the constant buffer pointer so we can access the vertex shader constant buffer from within this class.
	result = pDevice->CreateBuffer(&bufferDesc, NULL, m_materialBuffer.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create material buffer", __FUNCTION__);
		return false;
	}

	// Setup the raster description which will determine how and what polygons will be drawn.
	D3D11_RASTERIZER_DESC rasterDesc;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	// Create the rasterizer state from the description we just filled out.
	result = pDevice->CreateRasterizerState(&rasterDesc, m_solidState.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create solid rasterizer state", __FUNCTION__);
		return false;
	}

#ifdef USE_GEOMETRY_WIREFRAME
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.FrontCounterClockwise = FALSE;
	rasterDesc.DepthBias = D3D11_DEFAULT_DEPTH_BIAS;
	rasterDesc.DepthBiasClamp = D3D11_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterDesc.SlopeScaledDepthBias = D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterDesc.DepthClipEnable = TRUE;
	rasterDesc.ScissorEnable = FALSE;
	rasterDesc.MultisampleEnable = TRUE;
	rasterDesc.AntialiasedLineEnable = TRUE;
#else
	rasterDesc.MultisampleEnable = true;
	rasterDesc.AntialiasedLineEnable = true;
	rasterDesc.DepthClipEnable = false;
	rasterDesc.DepthBias = -1000;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
#endif


	result = pDevice->CreateRasterizerState(&rasterDesc, m_wireState.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create wire rasterizer state", __FUNCTION__);
		return false;
	}

#ifdef USE_GEOMETRY_WIREFRAME
	D3D11_DEPTH_STENCIL_DESC depthDesc;
	depthDesc.DepthEnable = true;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

	depthDesc.StencilEnable = FALSE;
	depthDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	depthDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

	// Stencil operations if pixel is front-facing.
	depthDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depthDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing.
	depthDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	depthDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	pDevice->CreateDepthStencilState(&depthDesc, m_depthSS.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create depth stencil state", __FUNCTION__);
		return false;
	}

	D3D11_BLEND_DESC blendStateDescription;
	// Clear the blend state description.
	ZeroMemory(&blendStateDescription, sizeof(D3D11_BLEND_DESC));

	// Create an alpha enabled blend state description.
	blendStateDescription.RenderTarget[0].BlendEnable = TRUE;
	blendStateDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendStateDescription.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendStateDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendStateDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	blendStateDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_DEST_ALPHA;
	blendStateDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendStateDescription.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	// Create the blend state using the description.
	result = pDevice->CreateBlendState(&blendStateDescription, m_wireBlendState.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create wire blend state", __FUNCTION__);
		return false;
	}
#endif

	return true;
}

bool CDXShader::VSSetShaderBuffer(CDXD3DDevice * device, VertexBuffer & params)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	VertexBuffer* dataPtr;

	auto pDeviceContext = device->GetDeviceContext();

	// Lock the matrix constant buffer so it can be written to.
	result = pDeviceContext->Map(m_matrixBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to map matrix buffer", __FUNCTION__);
		return false;
	}

	// Get a pointer to the data in the constant buffer.
	dataPtr = (VertexBuffer*)mappedResource.pData;

	// Transpose the matrices to prepare them for the shader.
	params.world = XMMatrixTranspose(params.world);
	params.view = XMMatrixTranspose(params.view);
	params.projection = XMMatrixTranspose(params.projection);

	*dataPtr = params;

	// Unlock the matrix constant buffer.
	pDeviceContext->Unmap(m_matrixBuffer.Get(), 0);

	ID3D11Buffer* buffers[] = { m_matrixBuffer.Get() };
	pDeviceContext->VSSetConstantBuffers(0, 1, buffers);
	pDeviceContext->GSSetConstantBuffers(0, 1, buffers);
	return true;
}

bool CDXShader::VSSetTransformBuffer(CDXD3DDevice * device, TransformBuffer & params)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	TransformBuffer* dataPtr;

	auto pDeviceContext = device->GetDeviceContext();

	// Lock the matrix constant buffer so it can be written to.
	result = pDeviceContext->Map(m_transformBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to map transform buffer", __FUNCTION__);
		return false;
	}

	// Get a pointer to the data in the constant buffer.
	dataPtr = (TransformBuffer*)mappedResource.pData;

	*dataPtr = params;

	// Unlock the matrix constant buffer.
	pDeviceContext->Unmap(m_transformBuffer.Get(), 0);

	ID3D11Buffer* buffers[] = { m_transformBuffer.Get() };
	pDeviceContext->VSSetConstantBuffers(1, 1, buffers);
	return true;
}

bool CDXShader::PSSetMaterialBuffers(CDXD3DDevice * device, MaterialBuffer & params)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	MaterialBuffer* dataPtr;

	auto pDeviceContext = device->GetDeviceContext();

	// Lock the light constant buffer so it can be written to.
	result = pDeviceContext->Map(m_materialBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to map material buffer", __FUNCTION__);
		return false;
	}

	// Get a pointer to the data in the constant buffer.
	dataPtr = (MaterialBuffer*)mappedResource.pData;

	// Copy the lighting variables into the constant buffer.
	*dataPtr = params;

	// Unlock the constant buffer.
	pDeviceContext->Unmap(m_materialBuffer.Get(), 0);

	ID3D11Buffer* buffers[] = { m_materialBuffer.Get() };
	pDeviceContext->PSSetConstantBuffers(0, 1, buffers);
	return true;
}

void CDXShader::RenderShader(CDXD3DDevice * device, const std::shared_ptr<CDXMaterial>& material)
{
	auto pDeviceContext = device->GetDeviceContext();
	// Set the vertex input layout.
	pDeviceContext->IASetInputLayout(m_layout.Get());

	// Set the vertex and pixel shaders that will be used to render this triangle.
#ifdef USE_GEOMETRY_WIREFRAME
	if (!m_baseSS) {
		pDeviceContext->OMGetDepthStencilState(&m_baseSS, &m_baseRef);
	}
#endif
	
	CDXShader::MaterialBuffer mat;

	ID3D11RasterizerState* state = m_solidState.Get();
	ID3D11VertexShader* vshader = m_vertexShader.Get();
	ID3D11PixelShader* pshader = m_pixelShader.Get();
	ID3D11GeometryShader* gshader = nullptr;
	ID3D11BlendState * blendingState = material->GetBlendingState(device).Get();

	float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	if (material->IsWireframe())
	{
		mat.hasNormal = false;
		mat.hasSpecular = false;
		mat.hasDetailMap = false;
		mat.hasTintMask = false;
		mat.tintColor = XMFLOAT4(0, 0, 0, 0);
		mat.wireColor = material->GetWireframeColor();
		mat.alphaThreshold = material->GetAlphaBlending() ? 0.0f : material->GetAlphaThreshold() / 255.0f;
		state = m_wireState.Get();
		pshader = m_wpShader.Get();
#ifdef USE_GEOMETRY_WIREFRAME
		vshader = m_wvShader.Get();
		gshader = m_wgShader.Get();
#endif

		if (m_wireBlendState)
		{
			blendingState = m_wireBlendState.Get();
		}
		

		if (m_depthSS)
		{
			pDeviceContext->OMSetDepthStencilState(m_depthSS.Get(), 0);
		}
	}
	else
	{
		mat.hasNormal = material->HasNormal();
		mat.hasSpecular = material->HasSpecular();
		mat.hasDetailMap = material->HasDetail();
		mat.hasTintMask = material->HasTintMask();
		mat.tintColor = material->GetTintColor();
		mat.wireColor = XMFLOAT4(0, 0, 0, 0);
		mat.alphaThreshold = material->GetAlphaBlending() ? 0.0f : material->GetAlphaThreshold() / 255.0f;

#ifdef USE_GEOMETRY_WIREFRAME
		pDeviceContext->OMSetDepthStencilState(m_baseSS.Get(), m_baseRef);
#endif
	}

	PSSetMaterialBuffers(device, mat);

	pDeviceContext->OMSetBlendState(blendingState, blendFactor, 0xffffffff);

	pDeviceContext->RSSetState(state);

	pDeviceContext->VSSetShader(vshader, nullptr, 0);
	pDeviceContext->PSSetShader(pshader, nullptr, 0);
	pDeviceContext->GSSetShader(gshader, nullptr, 0);

	// Set shader texture resource in the pixel shader.

	auto textures = material->GetTextures();
	ID3D11ShaderResourceView* resources[] = {
		textures[0].Get(),
		textures[1].Get(),
		textures[2].Get(),
		textures[3].Get(),
		textures[4].Get()
	};
	pDeviceContext->PSSetShaderResources(0, 5, resources);

	// Set the sampler state in the pixel shader.

	ID3D11SamplerState * samplers[] = { m_sampleState.Get() };
	pDeviceContext->PSSetSamplers(0, 1, samplers);
}