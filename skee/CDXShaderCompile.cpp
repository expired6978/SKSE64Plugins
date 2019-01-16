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

	HMODULE d3dcompiler = GetModuleHandle("d3dcompiler_43.dll"); // Try to find the compiler matching the DirectX version Skyrim SE is built on
	if (!d3dcompiler) {
		d3dcompiler = GetModuleHandle("d3dcompiler_42.dll"); // Alt that seems to also be shipped with SE
	}

	_D3DCompile compile = (_D3DCompile)GetProcAddress(d3dcompiler, "D3DCompile");
	if (!compile) {
		_ERROR("Failed to find d3dcompiler");
		return ERROR_API_UNAVAILABLE;
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