#ifndef __CDXMATERIAL__
#define __CDXMATERIAL__

#pragma once

#include "CDXTypes.h"

#include <mutex>
#include <d3d11_3.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#define DeclareFlags(type) \
	private: \
	type m_uFlags; \
	void SetField(type uVal, type uMask, type uPos) \
		{ \
		m_uFlags = (m_uFlags & ~uMask) | (uVal << uPos); \
		} \
		type GetField(type uMask, type uPos) const \
		{ \
		return (m_uFlags & uMask) >> uPos; \
		} \
		void SetBit(bool bVal, type uMask) \
		{ \
		if (bVal) \
			{ \
			m_uFlags |= uMask; \
			} \
			else \
			{ \
			m_uFlags &= ~uMask; \
			} \
		}\
		bool GetBit(type uMask) const \
		{ \
		return (m_uFlags & uMask) != 0; \
		}

static UInt32 mappedAlphaFunctions[] = {
	D3D11_BLEND_ONE,
	D3D11_BLEND_ZERO,
	D3D11_BLEND_SRC_COLOR,
	D3D11_BLEND_INV_SRC_COLOR,
	D3D11_BLEND_DEST_COLOR,
	D3D11_BLEND_INV_DEST_COLOR,
	D3D11_BLEND_SRC_ALPHA,
	D3D11_BLEND_INV_SRC_ALPHA,
	D3D11_BLEND_DEST_ALPHA,
	D3D11_BLEND_INV_DEST_ALPHA,
	D3D11_BLEND_SRC_ALPHA_SAT
};

static UInt32 mappedTestFunctions[] = {
	D3D11_COMPARISON_ALWAYS,
	D3D11_COMPARISON_LESS,
	D3D11_COMPARISON_EQUAL,
	D3D11_COMPARISON_LESS_EQUAL,
	D3D11_COMPARISON_GREATER,
	D3D11_COMPARISON_NOT_EQUAL,
	D3D11_COMPARISON_GREATER_EQUAL,
	D3D11_COMPARISON_NEVER,
};

class CDXD3DDevice;
class CDXShader;

class CDXMaterial
{
public:
	CDXMaterial();
	~CDXMaterial();

	void SetTexture(int index, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>* GetTextures() { return m_pTextures; }

	Microsoft::WRL::ComPtr<ID3D11BlendState> GetBlendingState(CDXD3DDevice * device);

	void SetWireframeColor(DirectX::XMFLOAT4 color);
	DirectX::XMFLOAT4 & GetWireframeColor();

	void SetTintColor(DirectX::XMFLOAT4 color);
	DirectX::XMFLOAT4 & GetTintColor();

	UInt32 GetShaderFlags1() const { return m_shaderFlags1; }
	UInt32 GetShaderFlags2() const { return m_shaderFlags2; }

	void SetShaderFlags1(UInt32 flags);
	void SetShaderFlags2(UInt32 flags);

	void SetFlags(UInt16 flags);

	enum AlphaFunction
	{
		ALPHA_ONE,
		ALPHA_ZERO,
		ALPHA_SRCCOLOR,
		ALPHA_INVSRCCOLOR,
		ALPHA_DESTCOLOR,
		ALPHA_INVDESTCOLOR,
		ALPHA_SRCALPHA,
		ALPHA_INVSRCALPHA,
		ALPHA_DESTALPHA,
		ALPHA_INVDESTALPHA,
		ALPHA_SRCALPHASAT,
		ALPHA_MAX_MODES
	};

	D3D11_BLEND GetD3DBlendMode(AlphaFunction alphaFunc);

	enum
	{
		ALPHA_BLEND_MASK          = 0x0001,
		SRC_BLEND_MASK      = 0x001e,
		SRC_BLEND_POS       = 1,
		DEST_BLEND_MASK     = 0x01e0,
		DEST_BLEND_POS      = 5,
		TEST_ENABLE_MASK    = 0x0200,
		TEST_FUNC_MASK      = 0x1c00,
		TEST_FUNC_POS       = 10,
		ALPHA_NOSORTER_MASK = 0x2000
	};

	enum TestFunction
	{
		TEST_ALWAYS,
		TEST_LESS,
		TEST_EQUAL,
		TEST_LESSEQUAL,
		TEST_GREATER,
		TEST_NOTEQUAL,
		TEST_GREATEREQUAL,
		TEST_NEVER,
		TEST_MAX_MODES
	};

	void SetAlphaBlending(bool bAlpha);
	bool GetAlphaBlending() const;

	void SetSrcBlendMode(AlphaFunction eSrcBlend);
	AlphaFunction GetSrcBlendMode() const;

	void SetDestBlendMode(AlphaFunction eDestBlend);
	AlphaFunction GetDestBlendMode() const;

	void SetAlphaTesting(bool bAlpha);
	bool GetAlphaTesting() const;

	void SetTestMode(TestFunction eTestFunc);
	TestFunction GetTestMode() const;

	void SetAlphaThreshold(UInt8 thresh) { m_alphaThreshold = thresh; }
	UInt8 GetAlphaThreshold() const { return m_alphaThreshold; }

	bool IsWireframe() const { return m_wireframe; }
	void SetWireframe(bool w) { m_wireframe = w; }

	bool HasDiffuse() const { return m_pTextures[0] != nullptr; }
	bool HasNormal() const { return m_pTextures[1] != nullptr; }
	bool HasSpecular() const { return m_pTextures[2] != nullptr; }
	bool HasDetail() const { return m_pTextures[3] != nullptr; }
	bool HasTintMask() const { return m_pTextures[4] != nullptr; }

protected:
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pTextures[5];
	Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendingState;
	bool m_blendingDirty;

	DirectX::XMFLOAT4 m_wireframeColor;
	DirectX::XMFLOAT4 m_tintColor;
	DeclareFlags(UInt16);
	UInt32	m_shaderFlags1;
	UInt32	m_shaderFlags2;
	UInt8	m_alphaThreshold;
	bool	m_wireframe;

#ifdef CDX_MUTEX
	std::mutex	m_mutex;
#endif
};

#endif
