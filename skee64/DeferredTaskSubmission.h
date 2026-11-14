#pragma once

#include <exception>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <utility>

namespace skee
{
// SKSE can execute a task under its queue lock. Order submissions under the
// actor-update mutex, then let one publisher hand them off outside that mutex.
class DeferredTaskSubmission
{
public:
	template <class Mutex, class Callback>
	void WhileLocked(Mutex& mutex, Callback&& callback)
	{
		// Reentrant callbacks use the outer batch and keep their place in it.
		if (s_pending) {
			std::lock_guard lock(mutex);
			callback();
			return;
		}

		std::exception_ptr failure;
		bool publish = false;
		{
			std::lock_guard lock(mutex);
			Submissions pending;
			{
				Batch batch(pending);
				try {
					callback();
				} catch (...) {
					failure = std::current_exception();
				}
			}

			// A callback can throw after work has already changed hands.
			// Splice that work into the FIFO before releasing the actor lock.
			m_ready.splice(m_ready.end(), pending);
			if (!m_publishing && !m_ready.empty()) {
				m_publishing = true;
				publish = true;
			}
		}

		if (publish) {
			Publish(mutex, failure);
		}
		if (failure) {
			std::rethrow_exception(failure);
		}
	}

	static void Submit(std::function<void()> submit)
	{
		if (s_pending) {
			s_pending->push_back(std::move(submit));
		} else {
			submit();
		}
	}

	// SKSE needs a copyable callback. Own the delegate even if its callback
	// is rejected by the queue or discarded before it runs.
	template <class Task, class Enqueue>
	static void SubmitOwned(Task* delegate, Enqueue enqueue)
	{
		auto task = std::shared_ptr<Task>(delegate, [](Task* owned) { owned->Dispose(); });
		Submit([enqueue = std::move(enqueue), task = std::move(task)]() mutable {
			enqueue([task = std::move(task)] { task->Run(); });
		});
	}

private:
	using Submissions = std::list<std::function<void()>>;

	class Batch
	{
	public:
		explicit Batch(Submissions& pending) { s_pending = &pending; }
		~Batch() { s_pending = nullptr; }
		Batch(const Batch&) = delete;
		Batch& operator=(const Batch&) = delete;
	};

	template <class Mutex>
	void Publish(Mutex& mutex, std::exception_ptr& failure)
	{
		for (;;) {
			Submissions ready;
			{
				std::lock_guard lock(mutex);
				if (m_ready.empty()) {
					m_publishing = false;
					return;
				}
				ready.swap(m_ready);
			}

			// A queue-drain callback may produce more work while we wait on
			// SKSE. It appends to m_ready and returns without waiting for us.
			for (const auto& submit : ready) {
				try {
					submit();
				} catch (...) {
					if (!failure) {
						failure = std::current_exception();
					}
				}
			}
		}
	}

	inline static thread_local Submissions* s_pending = nullptr;
	Submissions m_ready;
	bool m_publishing = false;
};
}
