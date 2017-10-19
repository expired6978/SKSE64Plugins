#ifdef FIXME

#ifndef __CDXSCENE__
#define __CDXSCENE__

#pragma once

#include "CDXCamera.h"
#include "CDXBrush.h"
#include <vector>

class CDXMesh;
class CDXShader;
class CDXPicker;
class CDXBrush;
class CDXMaskAddBrush;
class CDXInflateBrush;

typedef D3DXMATRIXA16			CDXMatrix16;
typedef D3DXMATRIX				CDXMatrix;
typedef std::vector<CDXMesh*>	CDXMeshList;

class CDXScene
{
public:
	CDXScene();
	~CDXScene();

	virtual void Setup(LPDIRECT3DDEVICE9 pDevice);
	virtual void Release();

	virtual void Render(LPDIRECT3DDEVICE9 pDevice);

	virtual void Begin(LPDIRECT3DDEVICE9 pDevice);
	virtual void End(LPDIRECT3DDEVICE9 pDevice);

	UInt32 GetNumMeshes() { return m_meshes.size(); }
	CDXMesh * GetNthMesh(UInt32 i) { return m_meshes.at(i); }

	void AddMesh(CDXMesh * mesh);
	bool Pick(int x, int y, CDXPicker & picker);

	CDXShader	* GetShader() { return m_shader; }
	void SetVisible(bool visible) { m_visible = visible; }
	bool IsVisible() const { return m_visible; }

	UInt32 GetWidth() const { return m_height; }
	UInt32 GetHeight() const { return m_height; }

protected:
	bool m_visible;

	UInt32	m_width;
	UInt32	m_height;
	
	CDXShader * m_shader;	

	IDirect3DVertexDeclaration9* m_pMeshDecl;
	IDirect3DStateBlock9* m_pStateBlock;

	CDXMeshList m_meshes;
};

#endif

#endif