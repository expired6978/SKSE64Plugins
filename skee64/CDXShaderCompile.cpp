#include "CDXShaderCompile.h"
#include <REX/W32/KERNEL32.h>

#include "REX/W32/D3DCOMPILER.h"
#include <cstdint>


typedef HRESULT (*_D3DCompile)(LPCVOID pSrcData,SIZE_T SrcDataSize, LPCSTR pSourceName, const REX::W32::D3D_SHADER_MACRO* pDefines, REX::W32::ID3DInclude* pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2, REX::W32::ID3DBlob** ppCode, REX::W32::ID3DBlob** ppErrorMsgs);

HRESULT CompileShaderFromData(LPCVOID pSrcData, _In_ SIZE_T SrcDataSize, _In_opt_ LPCSTR pSourceName, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ REX::W32::ID3DBlob** blob, _Outptr_ REX::W32::ID3DBlob** errorBlob)
{
	if (!pSrcData || !entryPoint || !profile || !blob)
		return E_INVALIDARG;

	*blob = nullptr;

	UINT flags = REX::W32::D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	flags |= REX::W32::D3DCOMPILE_DEBUG;
#endif
#ifndef _DEBUG
	flags |= REX::W32::D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	const REX::W32::D3D_SHADER_MACRO defines[] =
	{
		NULL, NULL
	};

	char name[REX::W32::MAX_PATH];
	const char * versions[] = {
		"47",
		"46e",
		"45",
		"44",
		"43",
		"42"
	};

	REX::W32::HMODULE d3dcompiler = 0;
	for (std::uint32_t i = 0; i < sizeof(versions) / sizeof(const char*); ++i)
	{
		_snprintf_s(name, REX::W32::MAX_PATH, "d3dcompiler_%s.dll", versions[i]);
		d3dcompiler = REX::W32::LoadLibraryA(name);
		if (d3dcompiler)
			break;
	}

	if (!d3dcompiler) {
		SKSE::log::error("{} - Failed to find d3dcompiler module", __FUNCTION__);
		return E_NOINTERFACE;
	}

	_D3DCompile compile = (_D3DCompile)REX::W32::GetProcAddress(d3dcompiler, "REX::W32::D3DCompile");
	if (!compile) {
		SKSE::log::error("{} - Failed to find REX::W32::D3DCompile function", __FUNCTION__);
		return E_NOINTERFACE;
	}

	REX::W32::ID3DBlob* shaderBlob = nullptr;
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

HRESULT CompileShader(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ REX::W32::ID3DBlob** blob, _Outptr_ REX::W32::ID3DBlob** errorBlob)
{
	if (!srcFile || !entryPoint || !profile || !blob)
		return E_INVALIDARG;

	*blob = nullptr;

	UINT flags = REX::W32::D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	flags |= REX::W32::D3DCOMPILE_DEBUG;
#endif

	const REX::W32::D3D_SHADER_MACRO defines[] =
	{
		NULL, NULL
	};

	REX::W32::ID3DBlob* shaderBlob = nullptr;
	HRESULT hr = D3DCompileFromFile(srcFile, defines, REX::W32::D3D_COMPILE_STANDARD_FILE_INCLUDE,
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
