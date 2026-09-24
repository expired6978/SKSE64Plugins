#ifndef __CDXNIFCOMMANDS__
#define __CDXNIFCOMMANDS__

#pragma once

#include "CDXStroke.h"
#include "CDXResetMask.h"
#include "CDXUndo.h"

#include <RE/B/BSTriShape.h>
#include <RE/N/NiAVObject.h>
#include <RE/N/NiSmartPointer.h>
#include <cstdint>

class CDXNifMesh;

class UIDelegate;

class CDXNifMaskAddStroke : public CDXMaskAddStroke
{
public:
	CDXNifMaskAddStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXMaskAddStroke(brush, mesh) { }

	virtual void Apply(std::int32_t i);
};

class CDXNifMaskSubtractStroke : public CDXMaskSubtractStroke
{
public:
	CDXNifMaskSubtractStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXMaskSubtractStroke(brush, mesh) { }

	virtual void Apply(std::int32_t i);
};

class CDXNifInflateStroke : public CDXInflateStroke
{
public:
	CDXNifInflateStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXInflateStroke(brush, mesh) { }

	virtual void Apply(std::int32_t i);
	virtual void Undo();
	virtual void Redo();
};

class CDXNifDeflateStroke : public CDXDeflateStroke
{
public:
	CDXNifDeflateStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXDeflateStroke(brush, mesh) { }

	virtual void Apply(std::int32_t i);
	virtual void Undo();
	virtual void Redo();
};

class CDXNifSmoothStroke : public CDXSmoothStroke
{
public:
	CDXNifSmoothStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXSmoothStroke(brush, mesh) { }

	virtual void Apply(std::int32_t i);
	virtual void Undo();
	virtual void Redo();
};

class CDXNifMoveStroke : public CDXMoveStroke
{
public:
	CDXNifMoveStroke(CDXBrush * brush, CDXEditableMesh * mesh) : CDXMoveStroke(brush, mesh) { }

	virtual void Apply(std::int32_t i);
	virtual void Undo();
	virtual void Redo();
};

class CDXNifResetMask : public CDXResetMask
{
public:
	CDXNifResetMask(CDXMesh * mesh) : CDXResetMask(mesh) { }

	virtual void Apply(std::int32_t i);
};

class CDXNifResetSculpt : public CDXUndoCommand
{
public:
	CDXNifResetSculpt(CDXNifMesh * mesh);
	~CDXNifResetSculpt();

	virtual UndoType GetUndoType();
	virtual void Undo();
	virtual void Redo();
	virtual void Apply(std::int32_t i);

	std::uint32_t Length() const { return m_current.size(); }

private:
	CDXNifMesh * m_mesh;
	CDXVectorMap	m_current;
};

class CDXNifImportGeometry : public CDXUndoCommand
{
public:
	CDXNifImportGeometry(CDXNifMesh * mesh, RE::NiAVObject * source);
	~CDXNifImportGeometry();

	virtual UndoType GetUndoType();
	virtual void Undo();
	virtual void Redo();
	virtual void Apply(std::int32_t i);

	std::uint32_t Length() const { return m_current.size(); }

private:
	CDXNifMesh		* m_mesh;
	CDXVectorMap	m_current;
};

class CRGNTaskUpdateModel : public SKEETaskDelegate
{
public:
	CRGNTaskUpdateModel(RE::BSTriShape * geometry);

	virtual void Run();
	virtual void Dispose();

private:
	RE::NiPointer<RE::BSTriShape> m_geometry;
};

class CRGNUITaskAddStroke : public SKEETaskDelegate
{
public:
	CRGNUITaskAddStroke(CDXStroke * stroke, RE::BSTriShape * geometry, std::int32_t i);

	virtual void Run();
	virtual void Dispose();

private:
	std::uint64_t m_editorGeneration;
	std::uint32_t m_undoType, m_strokeType, m_vertices;
	bool m_mirror;
	std::int32_t	m_id;
	RE::NiPointer<RE::BSTriShape> m_geometry;
};

class CRGNUITaskStandardCommand : public SKEETaskDelegate
{
public:
	CRGNUITaskStandardCommand(CDXUndoCommand * cmd, RE::BSTriShape * geometry, std::int32_t i);

	virtual void Run();
	virtual void Dispose();

private:
	std::uint64_t m_editorGeneration;
	std::uint32_t m_undoType;
	std::int32_t	m_id;
	RE::NiPointer<RE::BSTriShape> m_geometry;
};

#endif
