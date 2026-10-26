#ifndef __CDXNIFMESH__
#define __CDXNIFMESH__

#pragma once

#include "CDXEditableMesh.h"
#include "CDXMaterial.h"

#include <RE/B/BSTriShape.h>
#include <RE/N/NiGeometry.h>
#include <RE/N/NiGeometryData.h>
#include <RE/N/NiSmartPointer.h>

class CDXScene;
class CDXShader;
class CDXD3DDevice;

class CDXNifMesh : public CDXEditableMesh
{
public:
	CDXNifMesh();
	virtual ~CDXNifMesh();

	virtual const char* GetName() const override { return ""; }
	virtual RE::NiGeometry* GetLegacyGeometry() { return nullptr; }
	virtual RE::BSTriShape* GetGeometry() { return nullptr; }
	virtual bool IsMorphable() const
	{
		std::lock_guard guard(m_dataMutex);
		return m_morphable;
	}

	// Sculpt arrays are private CPU copies, not renderer allocations. Inherit
	// mesh-owned access/flush synchronization, without the global renderer lock.

protected:
	bool m_morphable;
};

class CDXLegacyNifMesh : public CDXNifMesh
{
public:
	CDXLegacyNifMesh();
	virtual ~CDXLegacyNifMesh();

	static CDXLegacyNifMesh * Create(CDXD3DDevice * pDevice, RE::NiGeometry * geometry);
	virtual const char* GetName() const override;
	virtual RE::NiGeometry* GetLegacyGeometry() override
	{
		std::lock_guard guard(m_dataMutex);
		return m_geometry.get();
	}

private:
	RE::NiPointer<RE::NiGeometry> m_geometry;
};

class CDXBSTriShapeMesh : public CDXNifMesh
{
public:
	CDXBSTriShapeMesh();
	virtual ~CDXBSTriShapeMesh();

	static CDXBSTriShapeMesh * Create(CDXD3DDevice * pDevice, RE::BSTriShape * geometry);

	virtual const char* GetName() const override;
	virtual RE::BSTriShape* GetGeometry() override
	{
		std::lock_guard guard(m_dataMutex);
		return m_geometry.get();
	}

private:
	RE::NiPointer<RE::BSTriShape> m_geometry;
};

#endif
