#include "CDXPixelShaderCache.h"
#include "CDXD3DDevice.h"
#include "CDXShaderCompile.h"

Microsoft::WRL::ComPtr<ID3D11PixelShader> CDXPixelShaderCache::CompileShader(CDXD3DDevice * device, const std::string & name, const ShaderFileData & fileData)
{
	Microsoft::WRL::ComPtr<ID3D11Device> pDevice = device->GetDevice();
	Microsoft::WRL::ComPtr<ID3DBlob> errorMessage;
	Microsoft::WRL::ComPtr<ID3DBlob> shaderBuffer;

	std::stringstream errors;

	// Compile the pixel shader code.
	HRESULT result = CompileShaderFromData(fileData.pSrcData, fileData.SrcDataSize, fileData.pSourceName, name.c_str(), "ps_5_0", &shaderBuffer, &errorMessage);
	if (FAILED(result))
	{
		// If the shader failed to compile it should have writen something to the error message.
		if (errorMessage)
		{
			OutputShaderErrorMessage(errorMessage, errors);
		}

		_ERROR("%s - Failed to create pixel shader:\n%s", __FUNCTION__, errors.str().c_str());
		return nullptr;
	}

	if (!shaderBuffer)
	{
		_ERROR("%s - Failed to acquire pixel shader buffer", __FUNCTION__);
		return nullptr;
	}

	// Create the vertex shader from the buffer.
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
	result = pDevice->CreatePixelShader(shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(), NULL, &pixelShader);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create pixel shader", __FUNCTION__);
		return nullptr;
	}

	return pixelShader;
}

void CDXPixelShaderCache::OutputShaderErrorMessage(Microsoft::WRL::ComPtr<ID3DBlob> & errorMessage, std::stringstream & output)
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

Microsoft::WRL::ComPtr<ID3D11PixelShader> CDXPixelShaderCache::GetShader(CDXD3DDevice * device, const std::string & name)
{
	auto it = find(name);
	if (it != end())
	{
		return it->second;
	}

	return nullptr;
}

