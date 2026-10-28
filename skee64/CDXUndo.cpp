#include "CDXUndo.h"
#include <cstdint>
#include <cstdlib>

CDXUndoStack	g_undoStack;

CDXUndoStack::CDXUndoStack()
{
	m_index = -1;
	m_maxStack = 512;
}

void CDXUndoStack::Release()
{
	// Teardown retires tickets without allocating or keeping any command alive.
	m_index = -1;
	m_actions.clear();
	m_revision.reset();
}

std::int32_t CDXUndoStack::Push(CDXUndoCommandPtr action)
{
	if (!action) return -1;
	Entry entry{std::move(action), std::make_shared<Identity>()};
	std::int32_t maxState = static_cast<std::int32_t>(m_actions.size()) - 1;
	const bool structural = m_index != maxState || m_actions.size() == m_maxStack;
	// Reserve and allocate before trimming; exceptions must not discard history
	// without also retiring its queued publication tickets.
	m_actions.reserve(m_actions.size() + 1);
	auto nextRevision = (structural || !m_revision) ? std::make_shared<Revision>() : m_revision;
	if(m_index != maxState) { // Not at the end, erase everything from now til the end
		m_actions.erase(m_actions.begin() + (m_index + 1), m_actions.end());
		m_index++;
	} else if(m_actions.size() == m_maxStack) { // Stack is full
		m_actions.erase(m_actions.begin());
	} else
		m_index++;

	m_actions.push_back(std::move(entry));
	m_revision = std::move(nextRevision);
	return m_index;
}

CDXUndoStack::Ticket CDXUndoStack::Capture(CDXUndoCommand* command, std::int32_t index) const
{
	Ticket ticket;
	if (index >= 0 && static_cast<std::size_t>(index) < m_actions.size() &&
		m_actions[index].command.get() == command) {
		ticket.index = index;
		ticket.revision = m_revision;
		ticket.identity = m_actions[index].identity;
	}
	return ticket;
}

bool CDXUndoStack::IsCurrent(const Ticket& ticket) const
{
	if (ticket.index < 0 || static_cast<std::size_t>(ticket.index) >= m_actions.size()) return false;
	const auto revision = ticket.revision.lock();
	const auto identity = ticket.identity.lock();
	return revision && revision == m_revision && identity && identity == m_actions[ticket.index].identity;
}

std::int32_t CDXUndoStack::Undo(bool doUpdate)
{
	if(m_index > -1) {
		if(doUpdate)
			m_actions.at(m_index).command->Undo();
		m_index--;
		return m_index;
	} 
	return -1;
}

std::int32_t CDXUndoStack::Redo(bool doUpdate)
{
	std::int32_t maxState = static_cast<std::int32_t>(m_actions.size()) - 1;
	if(m_index < maxState) {
		m_index++;
		if(doUpdate) 
			m_actions.at(m_index).command->Redo();
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

	for (std::uint32_t i = 0; i < std::abs(amount); i++) {
		result = amount < 0 ? Undo(doUpdate) : Redo(doUpdate);
	}
	
	return result;
}
