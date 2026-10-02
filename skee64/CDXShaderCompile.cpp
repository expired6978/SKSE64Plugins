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

	// Thread-safe function-local initialisation performs one loader acquisition,
	// not one per shader/reinitialisation. Retain the successful module reference
	// for the process lifetime: returned code/diagnostic blobs may outlive this
	// call and still need the DLL's implementation. Availability is fixed on first
	// use; a failed lookup keeps the existing E_NOINTERFACE/precompiled fallback.
	static const _D3DCompile compile = []() -> _D3DCompile {
		constexpr const char* versions[] = {
			"d3dcompiler_47.dll", "d3dcompiler_46e.dll", "d3dcompiler_45.dll",
			"d3dcompiler_44.dll", "d3dcompiler_43.dll", "d3dcompiler_42.dll"
		};
		REX::W32::HMODULE d3dcompiler = nullptr;
		for (const auto* name : versions) {
			d3dcompiler = REX::W32::LoadLibraryA(name);
			if (d3dcompiler)
				break;
		}
		if (!d3dcompiler) {
			SKSE::log::error("CompileShaderFromData - Failed to find d3dcompiler module");
			return nullptr;
		}
		// DLL export names are ABI strings, not C++ namespace-qualified symbols.
		const auto function = reinterpret_cast<_D3DCompile>(REX::W32::GetProcAddress(d3dcompiler, "D3DCompile"));
		if (!function) {
			SKSE::log::error("CompileShaderFromData - Failed to find D3DCompile function");
			REX::W32::FreeLibrary(d3dcompiler);
		}
		return function;
	}();
	if (!compile) {
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
