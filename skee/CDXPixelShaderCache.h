#pragma once

#include <unordered_map>
#include <string>
#include <wrl/client.h>
#include <d3d11.h>
#include <sstream>

#include "CDXTypes.h"

class CDXD3DDevice;
class CDXShaderFactory;

class CDXPixelShaderCache : protected std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11PixelShader>>
{
public:
	explicit CDXPixelShaderCache(CDXShaderFactory * factory) : m_factory(factory) { }

	virtual Microsoft::WRL::ComPtr<ID3D11PixelShader> GetShader(CDXD3DDevice* device, const std::string & name);

protected:
	CDXShaderFactory * m_factory;
};