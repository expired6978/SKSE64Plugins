#ifdef FIXME

#ifndef __CDXUNDO__
#define __CDXUNDO__

#pragma once

#include <vector>
#include <memory>

class CDXUndoCommand
{
public:
	virtual ~CDXUndoCommand() { };

	enum UndoType
	{
		kUndoType_None = 0,
		kUndoType_Stroke,
		kUndoType_ResetMask,
		kUndoType_ResetSculpt,
		kUndoType_Import
	};

	virtual UndoType GetUndoType() { return kUndoType_None; }
	virtual void Undo() { };
	virtual void Redo() { };
};

typedef std::shared_ptr<CDXUndoCommand> CDXUndoCommandPtr;

class CDXUndoStack : public std::vector<CDXUndoCommandPtr>
{
public:
	CDXUndoStack();

	SInt32 Undo(bool doUpdate);
	SInt32 Redo(bool doUpdate);
	SInt32 GoTo(SInt32 index, bool doUpdate);
	SInt32 Push(CDXUndoCommandPtr action);
	SInt32 GetIndex() const { return m_index; }

	UInt32 GetLimit() const { return m_maxStack; }

	void Release();

protected:
	SInt32	m_index;
	UInt32	m_maxStack;
};

extern CDXUndoStack	g_undoStack;

#endif

#endif