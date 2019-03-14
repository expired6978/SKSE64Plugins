#include "CDXShaderCompile.h"

#include <d3dcompiler.h>

typedef HRESULT (*_D3DCompile)(LPCVOID pSrcData,SIZE_T SrcDataSize, LPCSTR pSourceName, const D3D_SHADER_MACRO* pDefines, ID3DInclude* pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2, ID3DBlob** ppCode, ID3DBlob** ppErrorMsgs);

HRESULT CompileShaderFromData(LPCVOID pSrcData, _In_ SIZE_T SrcDataSize, _In_opt_ LPCSTR pSourceName, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob, _Outptr_ ID3DBlob** errorBlob)
{
	if (!pSrcData || !entryPoint || !profile || !blob)
		return E_INVALIDARG;

	*blob = nullptr;

	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	flags |= D3DCOMPILE_DEBUG;
#endif

	const D3D_SHADER_MACRO defines[] =
	{
		NULL, NULL
	};

	char name[MAX_PATH];
	const char * versions[] = {
		"43",
		"42",
		"44",
		"45",
		"46",
		"47",
		"46e"
	};

	HMODULE d3dcompiler = 0;
	for (UInt32 i = 0; i < sizeof(versions) / sizeof(const char*); ++i)
	{
		_snprintf_s(name, MAX_PATH, "d3dcompiler_%s.dll", versions[i]);
		d3dcompiler = LoadLibrary(name);
		if (d3dcompiler)
			break;
	}

	if (!d3dcompiler) {
		_ERROR("%s - Failed to find d3dcompiler module", __FUNCTION__);
		return E_NOINTERFACE;
	}

	_D3DCompile compile = (_D3DCompile)GetProcAddress(d3dcompiler, "D3DCompile");
	if (!compile) {
		_ERROR("%s - Failed to find D3DCompile function", __FUNCTION__);
		return E_NOINTERFACE;
	}

	ID3DBlob* shaderBlob = nullptr;
	HRESULT hr = compile(pSrcData, SrcDataSize, pSourceName, defines, nullptr, entryPoint, profile, flags, 0, &shaderBlob, errorBlob);
	if (FAILED(hr))
	{
		if (shaderBlob)
			shaderBlob->Release();

		return hr;
	}

	*blob = shaderBlob;

	return hr;
}

HRESULT CompileShader(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob, _Outptr_ ID3DBlob** errorBlob)
{
	if (!srcFile || !entryPoint || !profile || !blob)
		return E_INVALIDARG;

	*blob = nullptr;

	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	flags |= D3DCOMPILE_DEBUG;
#endif

	const D3D_SHADER_MACRO defines[] =
	{
		NULL, NULL
	};

	ID3DBlob* shaderBlob = nullptr;
	HRESULT hr = D3DCompileFromFile(srcFile, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entryPoint, profile,
		flags, 0, &shaderBlob, errorBlob);
	if (FAILED(hr))
	{
		if (shaderBlob)
			shaderBlob->Release();

		return hr;
	}

	*blob = shaderBlob;

	return hr;
}