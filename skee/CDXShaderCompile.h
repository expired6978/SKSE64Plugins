#pragma once

#pragma comment(lib,"d3dcompiler.lib")

#include <DirectXTex.h>

HRESULT CompileShaderFromData(LPCVOID pSrcData, _In_ SIZE_T SrcDataSize, _In_opt_ LPCSTR pSourceName, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob, _Outptr_ ID3DBlob** errorBlob);
HRESULT CompileShader(_In_ LPCWSTR srcFile, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob, _Outptr_ ID3DBlob** errorBlob);