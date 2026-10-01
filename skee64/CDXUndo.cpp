#include "CDXUndo.h"
#include <cstdint>

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

std::int32_t CDXUndoStack::Push(CDXUndoCommandPtr action)
{
	CDXUndoStack::iterator actionIt;
	std::int32_t maxState = (std::int32_t)size() - 1;
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

std::int32_t CDXUndoStack::Undo(bool doUpdate)
{
	if(m_index > -1) {
		if(doUpdate)
			at(m_index)->Undo();
		m_index--;
		return m_index;
	} 
	return -1;
}

std::int32_t CDXUndoStack::Redo(bool doUpdate)
{
	std::int32_t maxState = (std::int32_t)size() - 1;
	if(m_index < maxState) {
		m_index++;
		if(doUpdate) 
			at(m_index)->Redo();
		return m_index;
	}
	return -1;
}

std::int32_t CDXUndoStack::GoTo(std::int32_t index, bool doUpdate)
{
	std::int32_t result = -1;
	std::int32_t amount = index - m_index;

	if (amount == 0)
		return m_index;

	for (std::uint32_t i = 0; i < abs(amount); i++) {
		result = amount < 0 ? Undo(doUpdate) : Redo(doUpdate);
	}
	
	return result;
}
