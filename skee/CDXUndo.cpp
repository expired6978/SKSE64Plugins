#ifdef FIXME

#include "CDXUndo.h"

CDXUndoStack	g_undoStack;

CDXUndoStack::CDXUndoStack()
{
	m_index = -1;
	m_maxStack = 512;
}

void CDXUndoStack::Release()
{
	m_index = -1;
	clear();
}

SInt32 CDXUndoStack::Push(CDXUndoCommandPtr action)
{
	CDXUndoStack::iterator actionIt;
	int maxState = size() - 1;
	if(m_index != maxState) { // Not at the end, erase everything from now til the end
		erase(begin() + (m_index + 1), end());
		m_index++;
	} else if(size() == m_maxStack) { // Stack is full
		erase(begin());
	} else
		m_index++;

	push_back(action);
	return m_index;
}

SInt32 CDXUndoStack::Undo(bool doUpdate)
{
	if(m_index > -1) {
		if(doUpdate)
			at(m_index)->Undo();
		m_index--;
		return m_index;
	} 
	return -1;
}

SInt32 CDXUndoStack::Redo(bool doUpdate)
{
	SInt32 maxState = size() - 1;
	if(m_index < maxState) {
		m_index++;
		if(doUpdate) 
			at(m_index)->Redo();
		return m_index;
	}
	return -1;
}

SInt32 CDXUndoStack::GoTo(SInt32 index, bool doUpdate)
{
	SInt32 result = -1;
	SInt32 amount = index - m_index;

	if (amount == 0)
		return m_index;

	for (UInt32 i = 0; i < abs(amount); i++) {
		result = amount < 0 ? Undo(doUpdate) : Redo(doUpdate);
	}
	
	return result;
}

#endif