#ifdef FIXME

#ifndef __CDXSHADER__
#define __CDXSHADER__

#pragma once

#include <d3dx9.h>

class CDXShader
{
public:
	CDXShader();
	void CreateEffect(ID3D11Device * pDevice);
	void Release();

	D3DXHANDLE	m_hAmbient;
	D3DXHANDLE	m_hDiffuse;
	D3DXHANDLE	m_hSpecular;
	D3DXHANDLE	m_hOpacity;
	D3DXHANDLE	m_hSpecularPower;
	D3DXHANDLE	m_hLightColor;
	D3DXHANDLE	m_hLightPosition;
	D3DXHANDLE	m_hCameraPosition;
	D3DXHANDLE	m_hTexture;
	D3DXHANDLE	m_hTime;
	D3DXHANDLE	m_hWorld;
	D3DXHANDLE	m_hWorldViewProjection;
	D3DXHANDLE	m_hTransform;
	D3DXHANDLE	m_hWireframeColor;

	ID3DXEffect	* GetEffect() { return m_pEffect; }

protected:
	ID3DXEffect		* m_pEffect;
};

#endif

#endif