#ifdef FIXME

#ifndef __CDXPICKER__
#define __CDXPICKER__

#pragma once

#include "CDXMesh.h"

class CDXRayInfo
{
public:
	CDXVec3 origin;
	CDXVec3 direction;
	CDXVec3 point;

	CDXRayInfo()
	{
		Clear();
	}

	void Clear()
	{
		origin = CDXVec3(0, 0, 0);
		direction = CDXVec3(0, 0, 0);
		point = CDXVec3(0, 0, 0);
	}
};

class CDXPickInfo
{
public:
	CDXVec3		origin;
	CDXVec3		normal;
	CDXRayInfo	ray;
	float		dist;
	bool		isHit;

	CDXPickInfo()
	{
		Clear();
	}

	void Clear()
	{
		dist = FLT_MAX;
		origin = CDXVec3(0, 0, 0);
		normal = CDXVec3(0, 0, 0);
		isHit = false;
	}
};

class CDXPicker
{
public:
	virtual bool Pick(CDXPickInfo & pickInfo, CDXMesh * mesh, bool isMirror) = 0;
	virtual bool Mirror() const = 0;
};

#endif

#endif