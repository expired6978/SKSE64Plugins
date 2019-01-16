#ifndef __CDXBRUSH__
#define __CDXBRUSH__

#pragma once

#include "CDXPicker.h"
#include "CDXMesh.h"
#include "CDXUndo.h"
#include "CDXEditableMesh.h"
#include "CDXStroke.h"

#include <vector>
#include <set>

class CDXBrush
{
public:
	CDXBrush();

	enum BrushType {
		kBrushType_None = -1,
		kBrushType_Mask_Add = 0,
		kBrushType_Mask_Subtract,
		kBrushType_Inflate,
		kBrushType_Deflate,
		kBrushType_Move,
		kBrushType_Smooth,
		kBrushTypes
	};

	enum BrushProperty
	{
		kBrushProperty_Radius = 0,
		kBrushProperty_Strength,
		kBrushProperty_Falloff,
		kBrushProperties
	};

	enum BrushPropertyValue
	{
		kBrushPropertyValue_Value = 0,
		kBrushPropertyValue_Min,
		kBrushPropertyValue_Max,
		kBrushPropertyValue_Interval,
		kBrushPropertyValues
	};

	static void InitGlobals();

	virtual BrushType GetType() { return kBrushType_None; }

	double GetProperty(BrushProperty prop, BrushPropertyValue val) { return m_property[prop][val]; }
	void SetProperty(BrushProperty prop, BrushPropertyValue val, double newRadius) { m_property[prop][val] = newRadius; }
	
	void SetMirror(bool m) { m_mirror = m; }
	bool IsMirror() const { return m_mirror; }

	float CalculateFalloff(float & dist);

	virtual CDXStrokePtr CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh) = 0;
	virtual CDXHitIndexMap GetHitIndices(CDXPickInfo & pickInfo, CDXEditableMesh * mesh) = 0;
	virtual bool FilterVertex(CDXEditableMesh * mesh, CDXMeshVert * pVertices, CDXMeshIndex i) = 0;
	virtual bool BeginStroke(CDXPickInfo & pickInfo, CDXEditableMesh * mesh, bool isMirror) = 0;
	virtual bool UpdateStroke(CDXPickInfo & pickInfo, CDXEditableMesh * mesh, bool isMirror) = 0;
	virtual void EndStroke() = 0;

protected:
	CDXStrokeList	m_strokes;
	double			m_property[kBrushProperties][kBrushPropertyValues];
	bool			m_mirror;
};

class CDXBasicBrush : public CDXBrush
{
public:
	virtual CDXHitIndexMap GetHitIndices(CDXPickInfo & pickInfo, CDXEditableMesh * mesh);
	virtual bool FilterVertex(CDXEditableMesh * mesh, CDXMeshVert* pVertices, CDXMeshIndex i) { return false; }
	virtual void EndStroke();
};

class CDXBasicHitBrush : public CDXBasicBrush
{
public:
	virtual CDXHitIndexMap GetHitIndices(CDXPickInfo & pickInfo, CDXEditableMesh * mesh);
	virtual bool BeginStroke(CDXPickInfo & pickInfo, CDXEditableMesh * mesh, bool isMirror);
	virtual bool FilterVertex(CDXEditableMesh * mesh, CDXMeshVert * pVertices, CDXMeshIndex i);
};

class CDXBrushPicker : public CDXPicker
{
public:
	CDXBrushPicker(CDXBrush * brush) : m_brush(brush), m_mirror(true) { }
	virtual bool Pick(CDXPickInfo & pickInfo, CDXMesh * mesh, bool isMirror) = 0;
	virtual bool Mirror() const { return m_mirror; }

	void SetMirror(bool m) { m_mirror = m; }

	CDXBrush	* GetBrush() { return m_brush; }

protected:
	bool		m_mirror;
	CDXBrush	* m_brush;
};

class CDXBrushPickerBegin : public CDXBrushPicker
{
public:
	CDXBrushPickerBegin(CDXBrush * brush) : CDXBrushPicker(brush) { }
	virtual bool Pick(CDXPickInfo & pickInfo, CDXMesh * mesh, bool isMirror);
};

class CDXBrushPickerUpdate : public CDXBrushPicker
{
public:
	CDXBrushPickerUpdate(CDXBrush * brush) : CDXBrushPicker(brush) { }
	virtual bool Pick(CDXPickInfo & pickInfo, CDXMesh * mesh, bool isMirror);
};

class CDXMaskAddBrush : public CDXBasicHitBrush
{
public:
	CDXMaskAddBrush();
	virtual BrushType GetType() { return kBrushType_Mask_Add; }
	virtual CDXStrokePtr CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh);
	virtual bool UpdateStroke(CDXPickInfo & pickInfo, CDXEditableMesh * mesh, bool isMirror);
};

class CDXMaskSubtractBrush : public CDXMaskAddBrush
{
public:
	CDXMaskSubtractBrush();

	virtual BrushType GetType() { return kBrushType_Mask_Subtract; }
	virtual CDXStrokePtr CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh);
	virtual bool FilterVertex(CDXEditableMesh * mesh, CDXMeshVert * pVertices, CDXMeshIndex i);
};

class CDXInflateBrush : public CDXBasicHitBrush
{
public:
	CDXInflateBrush();
	virtual BrushType GetType() { return kBrushType_Inflate; }
	virtual CDXStrokePtr CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh);
	virtual bool UpdateStroke(CDXPickInfo & pickInfo, CDXEditableMesh * mesh, bool isMirror);
};

class CDXDeflateBrush : public CDXInflateBrush
{
public:
	CDXDeflateBrush();
	virtual BrushType GetType() { return kBrushType_Deflate; }
	virtual CDXStrokePtr CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh);
};

class CDXSmoothBrush : public CDXBasicHitBrush
{
public:
	CDXSmoothBrush();
	virtual BrushType GetType() { return kBrushType_Smooth; }
	virtual CDXStrokePtr CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh);
	virtual bool UpdateStroke(CDXPickInfo & pickInfo, CDXEditableMesh * mesh, bool isMirror);
	virtual bool FilterVertex(CDXEditableMesh * mesh, CDXMeshVert * pVertices, CDXMeshIndex i);
};

class CDXMoveBrush : public CDXBasicHitBrush
{
public:
	CDXMoveBrush();
	virtual BrushType GetType() { return kBrushType_Move; }
	virtual CDXStrokePtr CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh);
	virtual bool BeginStroke(CDXPickInfo & pickInfo, CDXEditableMesh * mesh, bool isMirror);
	virtual bool UpdateStroke(CDXPickInfo & pickInfo, CDXEditableMesh * mesh, bool isMirror);
};

#endif
