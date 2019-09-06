#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>

class CDXD3DDevice;

class CDXShaderFile
{
public:
	virtual void* GetBuffer() const = 0;
	virtual size_t GetBufferSize() const = 0;
	virtual const char* GetSourceName() const = 0;
	virtual const char* GetEntryPoint() const = 0;
};

class CDXShaderFactory
{
public:
	virtual bool CreateVertexShader(CDXD3DDevice * device, CDXShaderFile * sourceFile, CDXShaderFile * precompiledFile, D3D11_INPUT_ELEMENT_DESC * polygonLayout, int numElements, Microsoft::WRL::ComPtr<ID3D11VertexShader> & vertexShader, Microsoft::WRL::ComPtr<ID3D11InputLayout> & layout);
	virtual bool CreatePixelShader(CDXD3DDevice * device, CDXShaderFile * sourceFile, CDXShaderFile * precompiledFile, Microsoft::WRL::ComPtr<ID3D11PixelShader> & pixelShader);

private:
	void OutputShaderErrorMessage(Microsoft::WRL::ComPtr<ID3DBlob> & errorMessage, std::stringstream & output);
};