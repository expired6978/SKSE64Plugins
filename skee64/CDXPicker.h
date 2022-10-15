#ifndef __CDXPICKER__
#define __CDXPICKER__

#pragma once

#include "CDXMesh.h"

class CDXRayInfo
{
public:
	CDXVec origin;
	CDXVec direction;
	CDXVec point;

	CDXRayInfo()
	{
		Clear();
	}

	void Clear()
	{
		origin = DirectX::XMVectorZero();
		direction = DirectX::XMVectorZero();
		point = DirectX::XMVectorZero();
	}
};

class CDXPickInfo
{
public:
	CDXVec		origin;
	CDXVec		normal;
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
		origin = DirectX::XMVectorZero();
		normal = DirectX::XMVectorZero();
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
