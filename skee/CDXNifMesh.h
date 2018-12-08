#ifdef FIXME

#ifndef __CDXNIFMESH__
#define __CDXNIFMESH__

#pragma once

#include "CDXEditableMesh.h"
#include "CDXMaterial.h"

#include <d3dx9.h>

class NiGeometry;
class CDXScene;
class CDXShader;

class CDXNifMesh : public CDXEditableMesh
{
public:
	CDXNifMesh();
	~CDXNifMesh();

	static CDXNifMesh * Create(ID3D11Device * pDevice, NiGeometry * geometry);

	virtual void Pass(ID3D11Device * pDevice, UInt32 iPass, CDXShader * shader);

	NiGeometry * GetNifGeometry();
	bool IsMorphable();

private:
	bool		m_morphable;
	NiGeometry	* m_geometry;
};

#endif

#endif