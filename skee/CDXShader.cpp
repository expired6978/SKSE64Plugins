#include "CDXD3DDevice.h"
#include "CDXShader.h"
#include "CDXShaderCompile.h"
#include "CDXMaterial.h"

#include <sstream>

using namespace DirectX;

CDXShader::CDXShader()
{
	m_vertexShader = nullptr;
	m_pixelShader = nullptr;
	m_wireShader = nullptr;
	m_layout = nullptr;
	m_sampleState = nullptr;
	m_matrixBuffer = nullptr;
	m_transformBuffer = nullptr;
	m_materialBuffer = nullptr;
	m_solidState = nullptr;
	m_wireState = nullptr;
}

void CDXShader::Release()
{
	// Release the light constant buffer.
	if (m_transformBuffer)
	{
		m_transformBuffer->Release();
		m_transformBuffer = nullptr;
	}

	if (m_materialBuffer)
	{
		m_materialBuffer->Release();
		m_materialBuffer = nullptr;
	}

	// Release the matrix constant buffer.
	if (m_matrixBuffer)
	{
		m_matrixBuffer->Release();
		m_matrixBuffer = nullptr;
	}

	// Release the sampler state.
	if (m_sampleState)
	{
		m_sampleState->Release();
		m_sampleState = nullptr;
	}

	// Release the layout.
	if (m_layout)
	{
		m_layout->Release();
		m_layout = nullptr;
	}

	// Release the pixel shader.
	if (m_pixelShader)
	{
		m_pixelShader->Release();
		m_pixelShader = nullptr;
	}

	// Release the vertex shader.
	if (m_vertexShader)
	{
		m_vertexShader->Release();
		m_vertexShader = nullptr;
	}

	if (m_solidState)
	{
		m_solidState->Release();
		m_solidState = nullptr;
	}
	if (m_wireState)
	{
		m_wireState->Release();
		m_wireState = nullptr;
	}
}

bool CDXShader::Initialize(const CDXInitParams & initParams)
{
	HRESULT result;
	ID3DBlob* errorMessage;
	ID3DBlob* vertexShaderBuffer;
	ID3DBlob* pixelShaderBuffer;
	D3D11_INPUT_ELEMENT_DESC polygonLayout[4];
	unsigned int numElements;
	D3D11_SAMPLER_DESC samplerDesc;
	D3D11_BUFFER_DESC bufferDesc;

	auto pDevice = initParams.device->GetDevice();

	// Initialize the pointers this function will use to null.
	errorMessage = 0;
	vertexShaderBuffer = 0;
	pixelShaderBuffer = 0;

	std::stringstream errors;

	// Compile the vertex shader code.
	result = CompileShaderFromData(initParams.vertexShader.pSrcData, initParams.vertexShader.SrcDataSize, initParams.vertexShader.pSourceName, "LightVertexShader", "vs_5_0", &vertexShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		// If the shader failed to compile it should have writen something to the error message.
		if (errorMessage)
		{
			OutputShaderErrorMessage(errorMessage, errors);
		}
		_ERROR("%s - Failed to compile vertex shader %s", __FUNCTION__, errors.str().c_str());
		return false;
	}

	// Create the vertex shader from the buffer.
	result = pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &m_vertexShader);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create vertex shader", __FUNCTION__);
		return false;
	}


	// Compile the pixel shader code.
	result = CompileShaderFromData(initParams.pixelShader.pSrcData, initParams.pixelShader.SrcDataSize, initParams.pixelShader.pSourceName, "LightPixelShader", "ps_5_0", &pixelShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		// If the shader failed to compile it should have writen something to the error message.
		if (errorMessage)
		{
			OutputShaderErrorMessage(errorMessage, errors);
		}
		_ERROR("%s - Failed to compile pixel shader %s", __FUNCTION__, errors.str().c_str());
		return false;
	}

	// Create the vertex shader from the buffer.
	result = pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &m_pixelShader);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create pixel shader", __FUNCTION__);
		return false;
	}

	// Compile the pixel shader code.
	result = CompileShaderFromData(initParams.pixelShader.pSrcData, initParams.pixelShader.SrcDataSize, initParams.pixelShader.pSourceName, "WireframePixelShader", "ps_5_0", &pixelShaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		// If the shader failed to compile it should have writen something to the error message.
		if (errorMessage)
		{
			OutputShaderErrorMessage(errorMessage, errors);
		}

		_ERROR("%s - Failed to compile wireframe shader %s", __FUNCTION__, errors.str().c_str());
		return false;
	}

	// Create the vertex shader from the buffer.
	result = pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &m_wireShader);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create wireframe shader", __FUNCTION__);
		return false;
	}
#if 0
	{
		// Compile the vertex shader code.
		result = CompileShader(L"./light.gs", "VS", "vs_5_0", &vertexShaderBuffer, &errorMessage);
		if (FAILED(result))
		{
			// If the shader failed to compile it should have writen something to the error message.
			if (errorMessage)
			{
				OutputShaderErrorMessage(errorMessage, errors);
			}
			return false;
		}

		// Create the vertex shader from the buffer.
		result = pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), NULL, &m_ws.vs);
		if (FAILED(result))
		{
			return false;
		}


		// Compile the pixel shader code.
		result = CompileShader(L"./light.gs", "PSSolidWire", "ps_5_0", &pixelShaderBuffer, &errorMessage);
		if (FAILED(result))
		{
			// If the shader failed to compile it should have writen something to the error message.
			if (errorMessage)
			{
				OutputShaderErrorMessage(errorMessage, errors);
			}

			return false;
		}

		// Create the vertex shader from the buffer.
		result = pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &m_ws.ps);
		if (FAILED(result))
		{
			return false;
		}

		// Compile the pixel shader code.
		result = CompileShader(L"./light.gs", "GSSolidWire", "gs_5_0", &pixelShaderBuffer, &errorMessage);
		if (FAILED(result))
		{
			// If the shader failed to compile it should have writen something to the error message.
			if (errorMessage)
			{
				OutputShaderErrorMessage(errorMessage, errors);
			}

			return false;
		}

		// Create the vertex shader from the buffer.
		result = pDevice->CreateGeometryShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), NULL, &m_ws.gs);
		if (FAILED(result))
		{
			return false;
		}
	}
#endif

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

	// Create the vertex input layout.
	result = pDevice->CreateInputLayout(polygonLayout, numElements, vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), &m_layout);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create input layout", __FUNCTION__);
		return false;
	}

	// Release the vertex shader buffer and pixel shader buffer since they are no longer needed.
	vertexShaderBuffer->Release();
	vertexShaderBuffer = 0;

	pixelShaderBuffer->Release();
	pixelShaderBuffer = 0;

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
	result = pDevice->CreateSamplerState(&samplerDesc, &m_sampleState);
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
	result = pDevice->CreateBuffer(&bufferDesc, NULL, &m_matrixBuffer);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create matrix buffer", __FUNCTION__);
		return false;
	}

	bufferDesc.ByteWidth = sizeof(TransformBuffer);

	// Create the matrix constant buffer pointer so we can access the vertex shader constant buffer from within this class.
	result = pDevice->CreateBuffer(&bufferDesc, NULL, &m_transformBuffer);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create transform buffer", __FUNCTION__);
		return false;
	}

	// Setup the description of the light dynamic constant buffer that is in the pixel shader.
	// Note that ByteWidth always needs to be a multiple of 16 if using D3D11_BIND_CONSTANT_BUFFER or CreateBuffer will fail.
	bufferDesc.ByteWidth = sizeof(MaterialBuffer);

	// Create the constant buffer pointer so we can access the vertex shader constant buffer from within this class.
	result = pDevice->CreateBuffer(&bufferDesc, NULL, &m_materialBuffer);
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
	result = pDevice->CreateRasterizerState(&rasterDesc, &m_solidState);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create solid rasterizer state", __FUNCTION__);
		return false;
	}

	rasterDesc.MultisampleEnable = true;
	rasterDesc.AntialiasedLineEnable = true;
	rasterDesc.DepthClipEnable = false;
	rasterDesc.DepthBias = -1000;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
	result = pDevice->CreateRasterizerState(&rasterDesc, &m_wireState);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create wire rasterizer state", __FUNCTION__);
		return false;
	}

	return true;
}

void CDXShader::OutputShaderErrorMessage(ID3D10Blob* errorMessage, std::stringstream & output)
{
	char* compileErrors;
	size_t bufferSize, i;

	// Get a pointer to the error message text buffer.
	compileErrors = (char*)(errorMessage->GetBufferPointer());

	// Get the length of the message.
	bufferSize = errorMessage->GetBufferSize();

	// Write out the error message.
	for (i = 0; i<bufferSize; i++)
	{
		output << compileErrors[i];
	}

	// Release the error message.
	errorMessage->Release();
	errorMessage = 0;
}


bool CDXShader::VSSetShaderBuffer(CDXD3DDevice * device, VertexBuffer & params)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	VertexBuffer* dataPtr;

	auto pDeviceContext = device->GetDeviceContext();

	// Lock the matrix constant buffer so it can be written to.
	result = pDeviceContext->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
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
	pDeviceContext->Unmap(m_matrixBuffer, 0);
	pDeviceContext->VSSetConstantBuffers(0, 1, &m_matrixBuffer);
	return true;
}

bool CDXShader::VSSetTransformBuffer(CDXD3DDevice * device, TransformBuffer & params)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	TransformBuffer* dataPtr;

	auto pDeviceContext = device->GetDeviceContext();

	// Lock the matrix constant buffer so it can be written to.
	result = pDeviceContext->Map(m_transformBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to map transform buffer", __FUNCTION__);
		return false;
	}

	// Get a pointer to the data in the constant buffer.
	dataPtr = (TransformBuffer*)mappedResource.pData;

	*dataPtr = params;

	// Unlock the matrix constant buffer.
	pDeviceContext->Unmap(m_transformBuffer, 0);
	pDeviceContext->VSSetConstantBuffers(1, 1, &m_transformBuffer);
	return true;
}

bool CDXShader::PSSetMaterialBuffers(CDXD3DDevice * device, MaterialBuffer & params)
{
	HRESULT result;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	MaterialBuffer* dataPtr;

	auto pDeviceContext = device->GetDeviceContext();

	// Lock the light constant buffer so it can be written to.
	result = pDeviceContext->Map(m_materialBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
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
	pDeviceContext->Unmap(m_materialBuffer, 0);
	pDeviceContext->PSSetConstantBuffers(0, 1, &m_materialBuffer);
	return true;
}

void CDXShader::RenderShader(CDXD3DDevice * device, CDXMaterial * material)
{
	auto pDeviceContext = device->GetDeviceContext();
	// Set the vertex input layout.
	pDeviceContext->IASetInputLayout(m_layout);

	// Set the vertex and pixel shaders that will be used to render this triangle.
	
	CDXShader::MaterialBuffer mat;

	ID3D11RasterizerState * state = m_solidState;
	ID3D11VertexShader * vshader = m_vertexShader;
	ID3D11PixelShader * pshader = m_pixelShader;
	ID3D11BlendState * blendingState = material->GetBlendingState(device);

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
		state = m_wireState;
		pshader = m_wireShader;
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
	}

	PSSetMaterialBuffers(device, mat);

	pDeviceContext->OMSetBlendState(blendingState, blendFactor, 0xffffffff);

	pDeviceContext->RSSetState(state);

	Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;
	pDeviceContext->VSGetShader(&vs, nullptr, nullptr);
	Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
	pDeviceContext->PSGetShader(&ps, nullptr, nullptr);

	if(vs.Get() != vshader) pDeviceContext->VSSetShader(vshader, nullptr, 0);
	if(ps.Get() != pshader) pDeviceContext->PSSetShader(pshader, nullptr, 0);

	// Set shader texture resource in the pixel shader.
	pDeviceContext->PSSetShaderResources(0, 5, material->GetTextures());

	// Set the sampler state in the pixel shader.
	pDeviceContext->PSSetSamplers(0, 1, &m_sampleState);
}