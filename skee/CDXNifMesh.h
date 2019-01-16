#ifndef __CDXNIFMESH__
#define __CDXNIFMESH__

#pragma once

#include "CDXEditableMesh.h"
#include "CDXMaterial.h"

#include "skse64/NiTypes.h"

class NiGeometry;
class BSTriShape;
class CDXScene;
class CDXShader;
class CDXD3DDevice;

class CDXNifMesh : public CDXEditableMesh
{
public:
	CDXNifMesh();
	virtual ~CDXNifMesh();

	virtual const char* GetName() const override { return ""; }
	virtual NiGeometry* GetLegacyGeometry() { return nullptr; }
	virtual BSTriShape* GetGeometry() { return nullptr; }
	virtual bool IsMorphable() const
	{
#ifdef CDX_MUTEX
		std::lock_guard<std::mutex> guard(m_mutex);
#endif
		return m_morphable;
	}

	virtual CDXMeshVert * LockVertices(const LockMode type = READ) override;
	virtual CDXMeshIndex * LockIndices() override;

	virtual void UnlockVertices(const LockMode type) override;
	virtual void UnlockIndices(bool write = false) override;

protected:
	bool m_morphable;
};

class CDXLegacyNifMesh : public CDXNifMesh
{
public:
	CDXLegacyNifMesh();
	virtual ~CDXLegacyNifMesh();

	static CDXLegacyNifMesh * Create(CDXD3DDevice * pDevice, NiGeometry * geometry);
	virtual const char* GetName() const override;
	virtual NiGeometry* GetLegacyGeometry() override
	{
#ifdef CDX_MUTEX
		std::lock_guard<std::mutex> guard(m_mutex);
#endif
		return m_geometry;
	}

private:
	NiPointer<NiGeometry> m_geometry;
};

class CDXBSTriShapeMesh : public CDXNifMesh
{
public:
	CDXBSTriShapeMesh();
	virtual ~CDXBSTriShapeMesh();

	static CDXBSTriShapeMesh * Create(CDXD3DDevice * pDevice, BSTriShape * geometry);

	virtual const char* GetName() const override;
	virtual BSTriShape* GetGeometry() override
	{
#ifdef CDX_MUTEX
		std::lock_guard<std::mutex> guard(m_mutex);
#endif
		return m_geometry;
	}

private:
	NiPointer<BSTriShape> m_geometry;
};

#endif