#pragma once

#include <unordered_map>
#include <string>
#include <wrl/client.h>
#include <d3d11.h>
#include <sstream>

#include "CDXTypes.h"

class CDXD3DDevice;

class CDXPixelShaderCache : protected std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11PixelShader>>
{
public:
	virtual Microsoft::WRL::ComPtr<ID3D11PixelShader> GetShader(CDXD3DDevice* device, const std::string & name);
	virtual Microsoft::WRL::ComPtr<ID3D11PixelShader> CompileShader(CDXD3DDevice* device, const std::string & name, const ShaderFileData & fileData);

private:
	void OutputShaderErrorMessage(Microsoft::WRL::ComPtr<ID3DBlob> & errorMessage, std::stringstream & output);
};