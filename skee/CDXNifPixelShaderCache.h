#pragma once

#include "CDXPixelShaderCache.h"
#include "CDXShaderFactory.h"

class CDXTexturePixelShaderFile : public CDXShaderFile
{
public:
	CDXTexturePixelShaderFile(const std::string & shaderName);

	virtual void* GetBuffer() const;
	virtual size_t GetBufferSize() const;
	virtual const char* GetSourceName() const;
	virtual const char* GetEntryPoint() const;

private:
	std::string			m_entryPoint;
	std::string			m_sourceName;
	std::vector<char>	m_fileBuffer;
};

class CDXNifPixelShaderCache : public CDXPixelShaderCache
{
public:
	explicit CDXNifPixelShaderCache(CDXShaderFactory * factory) : CDXPixelShaderCache(factory) { }

	virtual Microsoft::WRL::ComPtr<ID3D11PixelShader> GetShader(CDXD3DDevice* device, const std::string & name) override;
};