#ifndef __CDXUNDO__
#define __CDXUNDO__

#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <utility>

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

// Editor-owner state. Callers must serialize editing and UI history publication
// on the editor thread; this class does not acquire engine/renderer locks.
class CDXUndoStack
{
private:
	struct Revision {};
	struct Identity {};
	struct Entry { CDXUndoCommandPtr command; std::shared_ptr<Identity> identity; };
	std::vector<Entry> m_actions;
	std::shared_ptr<Revision> m_revision = std::make_shared<Revision>();
public:
	// Weak identities neither keep stroke/mesh ownership alive nor permit ABA on
	// a reused numeric slot. Structural history changes retire the revision;
	// ordinary appends and undo/redo leave earlier queued actions valid.
	class Ticket
	{
		friend class CDXUndoStack;
		std::int32_t index = -1;
		std::weak_ptr<Revision> revision;
		std::weak_ptr<Identity> identity;
	};
	CDXUndoStack();
	Ticket Capture(CDXUndoCommand* command, std::int32_t index) const;
	bool IsCurrent(const Ticket& ticket) const;

	std::int32_t Undo(bool doUpdate);
	std::int32_t Redo(bool doUpdate);
	std::int32_t GoTo(std::int32_t index, bool doUpdate);
	std::int32_t Push(CDXUndoCommandPtr action);
	std::int32_t GetIndex() const { return m_index; }

	std::uint32_t GetLimit() const { return m_maxStack; }

	void Release();

protected:
	std::int32_t	m_index;
	std::uint32_t	m_maxStack;
};

extern CDXUndoStack	g_undoStack;

#endif
