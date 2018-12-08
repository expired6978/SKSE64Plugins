#ifdef FIXME

#ifndef __CDXMATERIAL__
#define __CDXMATERIAL__

#pragma once

#include <mutex>

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
	D3DBLEND_ONE,
	D3DBLEND_ZERO,
	D3DBLEND_SRCCOLOR,
	D3DBLEND_INVSRCCOLOR,
	D3DBLEND_DESTCOLOR,
	D3DBLEND_INVDESTCOLOR,
	D3DBLEND_SRCALPHA,
	D3DBLEND_INVSRCALPHA,
	D3DBLEND_DESTALPHA,
	D3DBLEND_INVDESTALPHA,
	D3DBLEND_SRCALPHASAT
};

static UInt32 mappedTestFunctions[] = {
	D3DCMP_ALWAYS,
	D3DCMP_LESS,
	D3DCMP_EQUAL,
	D3DCMP_LESSEQUAL,
	D3DCMP_GREATER,
	D3DCMP_NOTEQUAL,
	D3DCMP_GREATEREQUAL,
	D3DCMP_NEVER,
};

class CDXMaterial
{
public:
	CDXMaterial();
	~CDXMaterial();

	void Release();

	void SetDiffuseTexture(LPDIRECT3DBASETEXTURE9 texture);
	LPDIRECT3DBASETEXTURE9 GetDiffuseTexture() const { return m_diffuseTexture; }

	void SetDiffuseColor(DirectX::XMFLOAT3 color);
	void SetSpecularColor(DirectX::XMFLOAT3 color);
	void SetAmbientColor(DirectX::XMFLOAT3 color);
	void SetWireframeColor(DirectX::XMFLOAT3 color);

	DirectX::XMFLOAT3 & GetDiffuseColor();
	DirectX::XMFLOAT3 & GetSpecularColor();
	DirectX::XMFLOAT3 & GetAmbientColor();
	DirectX::XMFLOAT3 & GetWireframeColor();

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

protected:
	LPDIRECT3DBASETEXTURE9 m_diffuseTexture;

	DirectX::XMFLOAT3	m_specularColor;
	DirectX::XMFLOAT3	m_diffuseColor;
	DirectX::XMFLOAT3	m_ambientColor;
	DirectX::XMFLOAT3 m_wireframeColor;
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

#endif