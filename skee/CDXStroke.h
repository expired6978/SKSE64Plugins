#ifdef FIXME

#ifndef __CDXSTROKE__
#define __CDXSTROKE__

#pragma once

#include "CDXUndo.h"
#include "CDXEditableMesh.h"
#include "CDXPicker.h"

class CDXBrush;

class CDXStroke : public CDXUndoCommand
{
public:
	CDXStroke(CDXBrush * brush, CDXEditableMesh * mesh);

	class Info
	{
	public:
		CDXMeshIndex	index;
		double			strength;
		double			falloff;
	};


	enum StrokeType {
		kStrokeType_None = 0,
		kStrokeType_Mask_Add,
		kStrokeType_Mask_Subtract,
		kStrokeType_Inflate,
		kStrokeType_Deflate,
		kStrokeType_Move,
		kStrokeType_Smooth
	};

	virtual UndoType GetUndoType() = 0;
	virtual StrokeType GetStrokeType() = 0;
	virtual void Begin(CDXPickInfo & pickInfo) = 0;
	virtual void Update(Info * pickInfo) = 0;
	virtual void End() = 0;
	virtual void Apply(SInt32 i) = 0;
	virtual UInt32 Length() = 0;

	CDXVec3 GetOrigin() { return m_origin; }
	void SetMirror(bool m) { m_mirror = m; }
	bool IsMirror() const { return m_mirror; }
	CDXEditableMesh	* GetMesh() { return m_mesh; }

protected:
	CDXVec3				m_origin;
	CDXEditableMesh		* m_mesh;
	CDXBrush			* m_brush;
	bool				m_mirror;
};

typedef std::shared_ptr<CDXStroke> CDXStrokePtr;
typedef std::vector<CDXStrokePtr> CDXStrokeList;

class CDXBasicStroke : public CDXStroke
{
public:
	CDXBasicStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXStroke(brush, mesh) { };

	virtual UndoType GetUndoType();
	virtual void Begin(CDXPickInfo & pickInfo);
	virtual void End() { };
	virtual void Apply(SInt32 i) { };
};

class CDXBasicHitStroke : public CDXBasicStroke
{
public:
	CDXBasicHitStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXBasicStroke(brush, mesh) { };

	virtual void Undo();
	virtual void Redo();
	virtual UInt32 Length() { return m_current.size(); }

protected:
	CDXVectorMap m_current;
};

class CDXMaskAddStroke : public CDXBasicStroke
{
public:
	CDXMaskAddStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXBasicStroke(brush, mesh) { }
	~CDXMaskAddStroke();

	virtual StrokeType GetStrokeType();
	virtual void Update(Info * strokeInfo);
	virtual void Undo();
	virtual void Redo();
	virtual UInt32 Length() { return m_current.size(); }

protected:
	CDXMaskMap m_previous;
	CDXMaskMap m_current;
};

class CDXMaskSubtractStroke : public CDXMaskAddStroke
{
public:
	CDXMaskSubtractStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXMaskAddStroke(brush, mesh) { }

	virtual StrokeType GetStrokeType();
	virtual void Update(Info * strokeInfo);
};

class CDXInflateStroke : public CDXBasicHitStroke
{
public:
	CDXInflateStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXBasicHitStroke(brush, mesh) { }
	~CDXInflateStroke();

	class InflateInfo : public CDXStroke::Info
	{
	public:
		CDXVec3 normal;
	};

	virtual StrokeType GetStrokeType();
	virtual void Update(Info * strokeInfo);
};

class CDXDeflateStroke : public CDXInflateStroke
{
public:
	CDXDeflateStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXInflateStroke(brush, mesh) { }

	virtual StrokeType GetStrokeType();
	virtual void Update(Info * strokeInfo);
};

class CDXSmoothStroke : public CDXBasicHitStroke
{
public:
	CDXSmoothStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXBasicHitStroke(brush, mesh) { }
	~CDXSmoothStroke();

	virtual StrokeType GetStrokeType();
	virtual void Update(Info * strokeInfo);
};


class CDXMoveStroke : public CDXBasicStroke
{
public:
	CDXMoveStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXBasicStroke(brush, mesh) { }
	~CDXMoveStroke();

	class MoveInfo : public CDXStroke::Info
	{
	public:
		CDXVec3 offset;
	};

	virtual void Begin(CDXPickInfo & pickInfo);
	virtual StrokeType GetStrokeType();
	virtual void Update(Info * strokeInfo);
	virtual void End();
	virtual void Undo();
	virtual void Redo();
	virtual UInt32 Length() { return m_current.size(); }

	CDXRayInfo & GetRayInfo() { return m_rayInfo; }

	void SetHitIndices(CDXHitIndexMap & indices) { m_hitIndices = indices; }
	CDXHitIndexMap & GetHitIndices() { return m_hitIndices; }

protected:
	CDXRayInfo		m_rayInfo;
	CDXHitIndexMap	m_hitIndices;
	CDXVectorMap	m_previous;
	CDXVectorMap	m_current;
};

#endif

#endif