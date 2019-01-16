#ifndef __CDXRESETSELECTION__
#define __CDXRESETSELECTION__

#pragma once

#include "CDXUndo.h"
#include "CDXEditableMesh.h"

class CDXResetMask : public CDXUndoCommand
{
public:
	CDXResetMask::CDXResetMask(CDXMesh * mesh);
	virtual ~CDXResetMask();

	virtual UndoType GetUndoType();
	virtual void Redo();
	virtual void Undo();

protected:
	CDXMesh		* m_mesh;
	CDXMaskMap	m_previous;
	CDXMaskMap	m_current;
};

#endif