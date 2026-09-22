#pragma once

#include <SKSE/Interfaces.h>

// CommonLibSSE-NG 8 no longer exposes its task-delegate implementation as a
// subclassing API. RaceMenu only needs a tiny ownership contract before its
// tasks are wrapped in TaskInterface's public std::function overloads.
class SKEETaskDelegate
{
public:
	virtual void Run() = 0;
	virtual void Dispose() = 0;

protected:
	~SKEETaskDelegate() = default;
};

// Route plugin tasks through CommonLib's public function overload: run and
// dispose the plugin-owned task when the game thread executes it.
template <class T>
inline void SKEE_AddTask(const SKSE::TaskInterface* a_iface, T* a_task)
{
	a_iface->AddTask([a_task] { a_task->Run(); a_task->Dispose(); });
}

template <class T>
inline void SKEE_AddUITask(const SKSE::TaskInterface* a_iface, T* a_task)
{
	a_iface->AddUITask([a_task] { a_task->Run(); a_task->Dispose(); });
}
