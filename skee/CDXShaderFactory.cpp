#include "CDXShaderFactory.h"
#include "CDXD3DDevice.h"
#include "CDXShaderCompile.h"

#include <sstream>


bool CDXShaderFactory::CreatePixelShader(CDXD3DDevice * device, CDXShaderFile * sourceFile, CDXShaderFile * precompiledFile, Microsoft::WRL::ComPtr<ID3D11PixelShader> & pixelShader)
{
	Microsoft::WRL::ComPtr<ID3D11Device> pDevice = device->GetDevice();
	Microsoft::WRL::ComPtr<ID3DBlob> errorMessage;
	Microsoft::WRL::ComPtr<ID3DBlob> shaderBuffer;

	// Compile the pixel shader code.
	HRESULT result = CompileShaderFromData(sourceFile->GetBuffer(), sourceFile->GetBufferSize(), sourceFile->GetSourceName(), sourceFile->GetEntryPoint(), "ps_5_0", &shaderBuffer, &errorMessage);
	if (SUCCEEDED(result))
	{
		if (!shaderBuffer)
		{
			_ERROR("%s - Failed to acquire pixel shader buffer", __FUNCTION__);
			return false;
		}

		// Create the vertex shader from the buffer.
		result = pDevice->CreatePixelShader(shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(), NULL, pixelShader.ReleaseAndGetAddressOf());
		if (FAILED(result))
		{
			_ERROR("%s - Failed to create pixel shader", __FUNCTION__);
			return false;
		}
	}
	else if (result == E_NOINTERFACE && precompiledFile && precompiledFile->GetBufferSize())
	{
		// Create the vertex shader from the buffer.
		result = pDevice->CreatePixelShader(precompiledFile->GetBuffer(), precompiledFile->GetBufferSize(), NULL, pixelShader.ReleaseAndGetAddressOf());
		if (FAILED(result))
		{
			_ERROR("%s - Failed to create pixel shader", __FUNCTION__);
			return false;
		}
	}
	else
	{
		// If the shader failed to compile it should have writen something to the error message.
		std::stringstream errors;
		if (errorMessage)
		{
			OutputShaderErrorMessage(errorMessage, errors);
		}

		_ERROR("%s - Failed to compile pixel shader:\n%s", __FUNCTION__, errors.str().c_str());
		return false;
	}

	return true;
}

bool CDXShaderFactory::CreateVertexShader(CDXD3DDevice * device, CDXShaderFile * sourceFile, CDXShaderFile * precompiledFile, D3D11_INPUT_ELEMENT_DESC * polygonLayout, int numElements, Microsoft::WRL::ComPtr<ID3D11VertexShader> & vertexShader, Microsoft::WRL::ComPtr<ID3D11InputLayout> & layout)
{
	Microsoft::WRL::ComPtr<ID3D11Device> pDevice = device->GetDevice();
	Microsoft::WRL::ComPtr<ID3DBlob> errorMessage;
	Microsoft::WRL::ComPtr<ID3DBlob> shaderBuffer;

	std::stringstream errors;
	
	// Compile the pixel shader code.
	HRESULT result = CompileShaderFromData(sourceFile->GetBuffer(), sourceFile->GetBufferSize(), sourceFile->GetSourceName(), sourceFile->GetEntryPoint(), "vs_5_0", &shaderBuffer, &errorMessage);
	if (SUCCEEDED(result))
	{
		if (!shaderBuffer)
		{
			_ERROR("%s - Failed to acquire vertex shader buffer", __FUNCTION__);
			return false;
		}

		// Create the vertex shader from the buffer.
		result = pDevice->CreateVertexShader(shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(), NULL, vertexShader.ReleaseAndGetAddressOf());
		if (FAILED(result))
		{
			_ERROR("%s - Failed to create vertex shader", __FUNCTION__);
			return false;
		}

		// Create the vertex input layout.
		result = pDevice->CreateInputLayout(polygonLayout, numElements, shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(), &layout);
		if (FAILED(result))
		{
			_ERROR("%s - Failed to create input layout for vertex shader", __FUNCTION__);
			return false;
		}
	}
	else if (result == E_NOINTERFACE && precompiledFile && precompiledFile->GetBufferSize())
	{
		// Create the vertex shader from the buffer.
		result = pDevice->CreateVertexShader(precompiledFile->GetBuffer(), precompiledFile->GetBufferSize(), NULL, vertexShader.ReleaseAndGetAddressOf());
		if (FAILED(result))
		{
			_ERROR("%s - Failed to create vertex shader", __FUNCTION__);
			return false;
		}

		// Create the vertex input layout.
		result = pDevice->CreateInputLayout(polygonLayout, numElements, precompiledFile->GetBuffer(), precompiledFile->GetBufferSize(), layout.ReleaseAndGetAddressOf());
		if (FAILED(result))
		{
			_ERROR("%s - Failed to create input layout for vertex shader", __FUNCTION__);
			return false;
		}
	}
	else
	{
		// If the shader failed to compile it should have writen something to the error message.
		if (errorMessage)
		{
			OutputShaderErrorMessage(errorMessage, errors);
		}

		_ERROR("%s - Failed to compile vertex shader:\n%s", __FUNCTION__, errors.str().c_str());
		return false;
	}

	return true;
}

bool CDXShaderFactory::CreateGeometryShader(CDXD3DDevice * device, CDXShaderFile * sourceFile, CDXShaderFile * precompiledFile, Microsoft::WRL::ComPtr<ID3D11GeometryShader> & geometryShader)
{
	Microsoft::WRL::ComPtr<ID3D11Device> pDevice = device->GetDevice();
	Microsoft::WRL::ComPtr<ID3DBlob> errorMessage;
	Microsoft::WRL::ComPtr<ID3DBlob> shaderBuffer;

	// Compile the pixel shader code.
	HRESULT result = CompileShaderFromData(sourceFile->GetBuffer(), sourceFile->GetBufferSize(), sourceFile->GetSourceName(), sourceFile->GetEntryPoint(), "gs_5_0", &shaderBuffer, &errorMessage);
	if (SUCCEEDED(result))
	{
		if (!shaderBuffer)
		{
			_ERROR("%s - Failed to acquire geometry shader buffer", __FUNCTION__);
			return false;
		}

		// Create the vertex shader from the buffer.
		result = pDevice->CreateGeometryShader(shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(), NULL, geometryShader.ReleaseAndGetAddressOf());
		if (FAILED(result))
		{
			_ERROR("%s - Failed to create geometry shader", __FUNCTION__);
			return false;
		}
	}
	else if (result == E_NOINTERFACE && precompiledFile && precompiledFile->GetBufferSize())
	{
		// Create the vertex shader from the buffer.
		result = pDevice->CreateGeometryShader(precompiledFile->GetBuffer(), precompiledFile->GetBufferSize(), NULL, geometryShader.ReleaseAndGetAddressOf());
		if (FAILED(result))
		{
			_ERROR("%s - Failed to create geometry shader", __FUNCTION__);
			return false;
		}
	}
	else
	{
		// If the shader failed to compile it should have writen something to the error message.
		std::stringstream errors;
		if (errorMessage)
		{
			OutputShaderErrorMessage(errorMessage, errors);
		}

		_ERROR("%s - Failed to compile geometry shader:\n%s", __FUNCTION__, errors.str().c_str());
		return false;
	}

	return true;
}

void CDXShaderFactory::OutputShaderErrorMessage(Microsoft::WRL::ComPtr<ID3DBlob> & errorMessage, std::stringstream & output)
{
	// Get a pointer to the error message text buffer.
	char* compileErrors = (char*)(errorMessage->GetBufferPointer());

	// Get the length of the message.
	size_t bufferSize = errorMessage->GetBufferSize();

	// Write out the error message.
	for (size_t i = 0; i < bufferSize; i++)
	{
		output << compileErrors[i];
	}

	errorMessage = nullptr;
}