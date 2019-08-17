#include "CDXNifPixelShaderCache.h"
#include "skse64/GameStreams.h"
#include "FileUtils.h"

#include <stdio.h>

Microsoft::WRL::ComPtr<ID3D11PixelShader> CDXNifPixelShaderCache::GetShader(CDXD3DDevice* device, const std::string & name)
{
	std::string xform = name;
	std::transform(xform.begin(), xform.end(), xform.begin(), ::tolower);

	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader = __super::GetShader(device, name);
	if (shader)
	{
		return shader;
	}

	ShaderFileData shaderData;

	char shaderPath[MAX_PATH];
	sprintf_s(shaderPath, MAX_PATH, "SKSE/Plugins/NiOverride/Shaders/%s.fx", name.c_str());

	std::vector<char> vsb;
	BSResourceNiBinaryStream vs(shaderPath);
	if (!vs.IsValid()) {
		_ERROR("%s - Failed to read %s", __FUNCTION__, shaderPath);
		return false;
	}
	BSFileUtil::ReadAll(&vs, vsb);
	shaderData.pSourceName = shaderPath;
	shaderData.pSrcData = &vsb.at(0);
	shaderData.SrcDataSize = vsb.size();

	shader = CompileShader(device, name, shaderData);
	if (shader)
	{
#ifndef _DEBUG
		insert_or_assign(name, shader);
#endif
		return shader;
	}

	return nullptr;
}
