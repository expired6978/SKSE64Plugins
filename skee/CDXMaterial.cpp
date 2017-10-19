#include "CDXMaterial.h"

#ifdef FIXME

CDXMaterial::CDXMaterial()
{
	m_diffuseTexture = NULL;
	m_specularColor = D3DXVECTOR3(1.0f, 1.0f, 1.0f);
	m_diffuseColor = D3DXVECTOR3(1.0f, 1.0f, 1.0f);
	m_ambientColor = D3DXVECTOR3(1.0f, 1.0f, 1.0f);
	m_alphaThreshold = 0;
	m_shaderFlags1 = 0;
	m_shaderFlags2 = 0;
	SetAlphaBlending(false);
	SetSrcBlendMode(ALPHA_SRCALPHA);
	SetDestBlendMode(ALPHA_INVSRCALPHA);
	SetTestMode(TEST_ALWAYS);
	m_wireframe = false;
}

CDXMaterial::~CDXMaterial()
{
	
}

void CDXMaterial::Release()
{
	SetDiffuseTexture(NULL);
}

void CDXMaterial::SetDiffuseTexture(LPDIRECT3DBASETEXTURE9 texture)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	if(m_diffuseTexture)
		m_diffuseTexture->Release();
	m_diffuseTexture = texture;
	if(m_diffuseTexture)
		m_diffuseTexture->AddRef();
}

void CDXMaterial::SetDiffuseColor(D3DXVECTOR3 color)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	m_diffuseColor = color;
}
void CDXMaterial::SetSpecularColor(D3DXVECTOR3 color)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	m_specularColor = color;
}
void CDXMaterial::SetAmbientColor(D3DXVECTOR3 color)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	m_ambientColor = color;
}
void CDXMaterial::SetWireframeColor(D3DXVECTOR3 color)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	m_wireframeColor = color;
}

D3DXVECTOR3 & CDXMaterial::GetDiffuseColor()
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return m_diffuseColor;
}
D3DXVECTOR3 & CDXMaterial::GetSpecularColor()
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return m_specularColor;
}
D3DXVECTOR3 & CDXMaterial::GetAmbientColor()
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return m_ambientColor;
}
D3DXVECTOR3 & CDXMaterial::GetWireframeColor()
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	return m_wireframeColor;
}

void CDXMaterial::SetShaderFlags1(UInt32 flags)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	m_shaderFlags1 = flags;
}
void CDXMaterial::SetShaderFlags2(UInt32 flags)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	m_shaderFlags2 = flags;
}

void CDXMaterial::SetFlags(UInt16 flags)
{
#ifdef CDX_MUTEX
	std::lock_guard<std::mutex> guard(m_mutex);
#endif
	m_uFlags = flags;
}

void CDXMaterial::SetAlphaBlending(bool bAlpha)
{
	SetBit(bAlpha, ALPHA_BLEND_MASK);
}

bool CDXMaterial::GetAlphaBlending() const
{
	return GetBit(ALPHA_BLEND_MASK);
}

void CDXMaterial::SetSrcBlendMode(AlphaFunction eSrcBlend) 
{ 
	SetField((UInt16)(eSrcBlend), SRC_BLEND_MASK, SRC_BLEND_POS);
}

CDXMaterial::AlphaFunction CDXMaterial::GetSrcBlendMode() const
{ 
	return (CDXMaterial::AlphaFunction)GetField(SRC_BLEND_MASK, SRC_BLEND_POS);
}

void CDXMaterial::SetDestBlendMode(AlphaFunction eDestBlend)
{ 
	SetField((UInt16)(eDestBlend), DEST_BLEND_MASK, DEST_BLEND_POS);
}

CDXMaterial::AlphaFunction CDXMaterial::GetDestBlendMode() const
{ 
	return (CDXMaterial::AlphaFunction)GetField(DEST_BLEND_MASK, DEST_BLEND_POS);
}

void CDXMaterial::SetAlphaTesting(bool bAlpha)
{
	SetBit(bAlpha, TEST_ENABLE_MASK);
}
//---------------------------------------------------------------------------
bool CDXMaterial::GetAlphaTesting() const
{
	return GetBit(TEST_ENABLE_MASK);
}
//---------------------------------------------------------------------------
void CDXMaterial::SetTestMode(TestFunction eTestFunc)
{ 
	SetField((UInt16)(eTestFunc), TEST_FUNC_MASK, TEST_FUNC_POS);
}
//---------------------------------------------------------------------------
CDXMaterial::TestFunction CDXMaterial::GetTestMode() const
{ 
	return (CDXMaterial::TestFunction) GetField(TEST_FUNC_MASK, TEST_FUNC_POS);
}

#endif