#pragma once

#include <SKSE/Interfaces.h>

#include "DeferredTaskSubmission.h"

// TaskInterface::AddTask(TaskFn)/AddUITask(TaskFn) wrap the function in the
// interface's own delegate classes (Task/UITask, derived from
// SKSE::detail::TaskDelegate/UIDelegate_v1). Route plugin tasks through that
// overload, deferring submission while an actor-update batch is active.
template <class T>
inline void SKEE_AddTask(const SKSE::TaskInterface* a_iface, T* a_task)
{
	skee::DeferredTaskSubmission::SubmitOwned(a_task, [a_iface](auto run) { a_iface->AddTask(std::move(run)); });
}

template <class T>
inline void SKEE_AddUITask(const SKSE::TaskInterface* a_iface, T* a_task)
{
	skee::DeferredTaskSubmission::SubmitOwned(a_task, [a_iface](auto run) { a_iface->AddUITask(std::move(run)); });
}
