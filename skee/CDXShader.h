#ifndef __CDXSHADER__
#define __CDXSHADER__

#pragma once

#include "CDXTypes.h"
#include <sstream>

class CDXD3DDevice;
class CDXMaterial;
class CDXMesh;

class CDXShader
{
public:
	CDXShader();
	bool Initialize(const CDXInitParams & initParams);

	struct VertexBuffer
	{
		DirectX::XMMATRIX world;
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX projection;
		DirectX::XMVECTOR viewport;
	};

	struct TransformBuffer
	{
		DirectX::XMMATRIX transform;
	};

	struct MaterialBuffer
	{
		DirectX::XMFLOAT4 wireColor;
		DirectX::XMFLOAT4 tintColor;
		int hasNormal = 0;
		int hasSpecular = 0;
		int hasDetailMap = 0;
		int hasTintMask = 0;
		float alphaThreshold = 0.0f;
		float padding1 = 0.0f;
		float padding2 = 0.0f;
		float padding3 = 0.0f;
	};

	void RenderShader(CDXD3DDevice * device, CDXMaterial * material);

	bool VSSetShaderBuffer(CDXD3DDevice * device, VertexBuffer & params);
	bool VSSetTransformBuffer(CDXD3DDevice * device, TransformBuffer & transform);
	bool PSSetMaterialBuffers(CDXD3DDevice * device, MaterialBuffer & material);

protected:
	void OutputShaderErrorMessage(ID3D10Blob* errorMessage, std::stringstream & output);

	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_wvShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_wpShader;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> m_wgShader;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthSS;
	Microsoft::WRL::ComPtr<ID3D11BlendState> m_wireBlendState;

	UINT m_baseRef;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_baseSS;
#if 0
	struct WireShaders
	{
		ID3D11VertexShader * vs;
		ID3D11GeometryShader* gs;
		ID3D11PixelShader* ps;
	};
	WireShaders m_ws;
#endif
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_layout;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampleState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_solidState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_wireState;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_matrixBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_transformBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_materialBuffer;
};

#endif
