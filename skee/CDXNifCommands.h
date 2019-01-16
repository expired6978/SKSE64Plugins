#ifndef __CDXNIFCOMMANDS__
#define __CDXNIFCOMMANDS__

#pragma once

#include "CDXStroke.h"
#include "CDXResetMask.h"
#include "CDXUndo.h"

#include "skse64/GameThreads.h"
#include "skse64/Hooks_UI.h"

class CDXNifMesh;

class BSTriShape;
class UIDelegate;

class CDXNifMaskAddStroke : public CDXMaskAddStroke
{
public:
	CDXNifMaskAddStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXMaskAddStroke(brush, mesh) { }

	virtual void Apply(SInt32 i);
};

class CDXNifMaskSubtractStroke : public CDXMaskSubtractStroke
{
public:
	CDXNifMaskSubtractStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXMaskSubtractStroke(brush, mesh) { }

	virtual void Apply(SInt32 i);
};

class CDXNifInflateStroke : public CDXInflateStroke
{
public:
	CDXNifInflateStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXInflateStroke(brush, mesh) { }

	virtual void Apply(SInt32 i);
	virtual void Undo();
	virtual void Redo();
};

class CDXNifDeflateStroke : public CDXDeflateStroke
{
public:
	CDXNifDeflateStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXDeflateStroke(brush, mesh) { }

	virtual void Apply(SInt32 i);
	virtual void Undo();
	virtual void Redo();
};

class CDXNifSmoothStroke : public CDXSmoothStroke
{
public:
	CDXNifSmoothStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXSmoothStroke(brush, mesh) { }

	virtual void Apply(SInt32 i);
	virtual void Undo();
	virtual void Redo();
};


class CDXNifMoveStroke : public CDXMoveStroke
{
public:
	CDXNifMoveStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXMoveStroke(brush, mesh) { }

	virtual void Apply(SInt32 i);
	virtual void Undo();
	virtual void Redo();
};

class CDXNifResetMask : public CDXResetMask
{
public:
	CDXNifResetMask(CDXMesh * mesh) : CDXResetMask(mesh) { }

	virtual void Apply(SInt32 i);
};

class CDXNifResetSculpt : public CDXUndoCommand
{
public:
	CDXNifResetSculpt(CDXNifMesh * mesh);
	~CDXNifResetSculpt();

	virtual UndoType GetUndoType();
	virtual void Undo();
	virtual void Redo();
	virtual void Apply(SInt32 i);

	UInt32 Length() const { return m_current.size(); }

private:
	CDXNifMesh * m_mesh;
	CDXVectorMap	m_current;
};

class CDXNifImportGeometry : public CDXUndoCommand
{
public:
	CDXNifImportGeometry(CDXNifMesh * mesh, NiAVObject * source);
	~CDXNifImportGeometry();

	virtual UndoType GetUndoType();
	virtual void Undo();
	virtual void Redo();
	virtual void Apply(SInt32 i);

	UInt32 Length() const { return m_current.size(); }

private:
	CDXNifMesh		* m_mesh;
	CDXVectorMap	m_current;
};

class CRGNTaskUpdateModel : public TaskDelegate
{
public:
	CRGNTaskUpdateModel(BSTriShape * geometry);

	virtual void Run();
	virtual void Dispose();

private:
	BSTriShape * m_geometry;
};

class CRGNUITaskAddStroke : public UIDelegate_v1
{
public:
	CRGNUITaskAddStroke(CDXStroke * stroke, BSTriShape * geometry, SInt32 i);

	virtual void Run();
	virtual void Dispose();

private:
	CDXStroke * m_stroke;
	SInt32	m_id;
	BSTriShape * m_geometry;
};

class CRGNUITaskStandardCommand : public UIDelegate_v1
{
public:
	CRGNUITaskStandardCommand(CDXUndoCommand * cmd, BSTriShape * geometry, SInt32 i);

	virtual void Run();
	virtual void Dispose();

private:
	CDXUndoCommand * m_cmd;
	SInt32	m_id;
	BSTriShape * m_geometry;
};

#endif