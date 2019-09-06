#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <sstream>
#include <wrl/client.h>
#include <vector>
#include <memory>

using namespace DirectX;

class CDXPixelShaderCache;
class CDXD3DDevice;
class CDXShaderFile;
class CDXShaderFactory;

class CDXTextureRenderer
{
public:
	struct VertexType
	{
		XMFLOAT3 position;
		XMFLOAT2 texture;
	};

	enum class TextureType
	{
		Normal = 0,
		Mask,
		Color,
		Unknown
	};

	struct LayerData
	{
		union Mode
		{
			struct BlendData
			{
				UInt32		dummy;
				TextureType	type;
			} blendData;
			XMINT2 data;
		} blending;
		XMFLOAT4 maskColor;
	};

	struct ConstantBufferType
	{
		XMMATRIX	world;
		XMMATRIX	view;
		XMMATRIX	projection;
		LayerData	layerData;
	};

	CDXTextureRenderer();
	virtual ~CDXTextureRenderer() { }

	virtual bool Initialize(CDXD3DDevice * device, CDXShaderFactory * factory, CDXShaderFile * sourceFile, CDXShaderFile * precompiledFile, CDXPixelShaderCache * cache);
	virtual void Render(CDXD3DDevice * pDevice, bool clear = true);
	virtual void Release();
	virtual bool SetTexture(CDXD3DDevice * device, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture, DXGI_FORMAT target);

	bool UpdateVertexBuffer(CDXD3DDevice * device);
	bool UpdateConstantBuffer(CDXD3DDevice * device);
	bool UpdateStructuredBuffer(CDXD3DDevice * device, const LayerData & layerData);

	int GetWidth() const { return m_bitmapWidth; }
	int GetHeight() const { return m_bitmapHeight; }
	Microsoft::WRL::ComPtr<ID3D11Texture2D> GetTexture() { return m_renderTargetTexture; }
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetResourceView() { return m_shaderResourceView; };

	void AddLayer(const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> & texture, const TextureType & type, const std::string & technique, const XMFLOAT4 & maskColor);

protected:
	bool InitializeVertices(CDXD3DDevice * device);
	bool InitializeVertexShader(CDXD3DDevice * device, CDXShaderFactory * factory, CDXShaderFile * sourceFile, CDXShaderFile * precompiledFile);

	CDXPixelShaderCache * m_shaderCache;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	m_source;

	struct ResourceData
	{
		LayerData											m_metadata;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	m_resource;
		std::string											m_shader;
	};

	std::vector<ResourceData>							m_resources;

	Microsoft::WRL::ComPtr<ID3D11Buffer>				m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>				m_indexBuffer;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>			m_vertexShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>			m_layout;
	Microsoft::WRL::ComPtr<ID3D11Buffer>				m_constantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>				m_structuredBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	m_structuredBufferView;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>			m_sampleState;
	Microsoft::WRL::ComPtr<ID3D11BlendState>			m_alphaEnableBlendingState;

	Microsoft::WRL::ComPtr<ID3D11Texture2D>				m_renderTargetTexture;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>		m_renderTargetView;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	m_shaderResourceView;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>				m_intermediateTexture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	m_intermediateResourceView;

	int m_vertexCount;
	int m_indexCount;
	int m_bitmapWidth;
	int m_bitmapHeight;
};