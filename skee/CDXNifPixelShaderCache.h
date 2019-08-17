#pragma once

#include "CDXPixelShaderCache.h"

class CDXNifPixelShaderCache : public CDXPixelShaderCache
{
public:
	virtual Microsoft::WRL::ComPtr<ID3D11PixelShader> GetShader(CDXD3DDevice* device, const std::string & name) override;
};