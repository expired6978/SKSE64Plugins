#pragma once

#include <SKSE/Interfaces.h>

// TaskInterface::AddTask(TaskFn)/AddUITask(TaskFn) wrap the function in the
// interface's own delegate classes (Task/UITask, derived from
// SKSE::detail::TaskDelegate/UIDelegate_v1). Route plugin tasks through that
// overload: run and dispose the task when the game thread executes it.
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
