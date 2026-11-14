#include "../skee64/DeferredTaskSubmission.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <latch>
#include <set>
#include <vector>
#include <stdexcept>
#include <thread>

namespace
{
using skee::DeferredTaskSubmission;

void Require(bool condition, const char* message)
{
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}

struct Task
{
	int& runs;
	int& disposals;

	void Run() { ++runs; }
	void Dispose()
	{
		++disposals;
		delete this;
	}
};

void CheckLockOrder(bool useFixedDispatch)
{
	std::mutex queue;
	std::recursive_mutex actorUpdates;
	DeferredTaskSubmission submissions;
	std::latch queueHeld(1);
	std::latch enqueueAttempted(1);
	std::latch actorProbeDone(1);
	std::atomic<bool> inverseOrder = false;
	bool drainEnteredActorUpdates = false;
	std::function<void()> queued;
	int runs = 0;
	int disposals = 0;

	std::jthread drain([&] {
		std::lock_guard held(queue);
		queueHeld.count_down();
		enqueueAttempted.wait();
		// Probe the exact second edge without hanging the negative control.
		const std::unique_lock actorLock(actorUpdates, std::try_to_lock);
		drainEnteredActorUpdates = actorLock.owns_lock();
		inverseOrder = !drainEnteredActorUpdates;
		actorProbeDone.count_down();
	});

	queueHeld.wait();
	const auto enqueue = [&](auto run) {
		enqueueAttempted.count_down();
		actorProbeDone.wait();
		if (!inverseOrder) {
			std::lock_guard held(queue);
			queued = std::move(run);
		}
	};
	const auto notify = [&] {
		DeferredTaskSubmission::SubmitOwned(new Task{runs, disposals}, enqueue);
	};

	if (useFixedDispatch) {
		// A reentrant observer must leave submission to the outer dispatch.
		submissions.WhileLocked(actorUpdates, [&] {
			submissions.WhileLocked(actorUpdates, notify);
		});
	} else {
		const std::lock_guard actorLock(actorUpdates);
		notify();
	}
	drain.join();

	if (!useFixedDispatch) {
		Require(inverseOrder && !queued && disposals == 1, "Old lock order did not expose the cycle");
		return;
	}

	Require(!inverseOrder && drainEnteredActorUpdates && queued, "Drain could not enter actor updates before enqueue");
	Require(runs == 0 && disposals == 0, "Queued delegate lost its lifetime");
	queued();
	queued = {};
	Require(runs == 1 && disposals == 1, "Delegate did not run and dispose exactly once");
}

void CheckThrowingFlushObserver()
{
	std::recursive_mutex actorUpdates;
	DeferredTaskSubmission submissions;
	int runs = 0;
	int disposals = 0;
	std::set<Task*> dyeUpdates{new Task{runs, disposals}};
	std::vector<std::function<void()>> queued;
	const auto flush = [&](bool observerThrows) {
		submissions.WhileLocked(actorUpdates, [&] {
			while (!dyeUpdates.empty()) {
				auto it = dyeUpdates.begin();
				auto* task = *it;
				dyeUpdates.erase(it);
				DeferredTaskSubmission::SubmitOwned(task, [&](auto run) { queued.emplace_back(std::move(run)); });
			}
			if (observerThrows) {
				throw std::runtime_error("observer failed");
			}
		});
	};

	bool threw = false;
	try {
		flush(true);
	} catch (const std::runtime_error&) {
		threw = true;
	}
	Require(threw && dyeUpdates.empty() && queued.size() == 1, "Throwing flush observer lost staged dye work");
	flush(false);
	Require(queued.size() == 1 && runs == 0 && disposals == 0, "Flush retry lost or duplicated the transferred task");
	queued.front()();
	queued.clear();
	Require(runs == 1 && disposals == 1, "Preserved dye update did not run and dispose once");

	bool submitted = false;
	DeferredTaskSubmission::SubmitOwned(new Task{runs, disposals}, [&](auto) { submitted = true; });
	Require(submitted && runs == 1 && disposals == 2, "Unwind left a stale batch or leaked a discarded callback");
}

void CheckConcurrentPublisherOrder()
{
	std::mutex queue;
	std::recursive_mutex actorUpdates;
	DeferredTaskSubmission submissions;
	std::latch queueHeld(1);
	std::latch publisherAtQueue(1);
	std::vector<int> order;

	std::jthread drain([&] {
		std::lock_guard held(queue);
		queueHeld.count_down();
		publisherAtQueue.wait();
		// Simulate a drain callback producing a second batch while the
		// first publisher is blocked on this queue mutex.
		submissions.WhileLocked(actorUpdates, [&] {
			DeferredTaskSubmission::Submit([&] {
				std::lock_guard takeQueue(queue);
				order.push_back(3);
			});
			DeferredTaskSubmission::Submit([&] {
				std::lock_guard takeQueue(queue);
				order.push_back(4);
			});
		});
	});
	queueHeld.wait();
	submissions.WhileLocked(actorUpdates, [&] {
		DeferredTaskSubmission::Submit([&] {
			publisherAtQueue.count_down();
			std::lock_guard takeQueue(queue);
			order.push_back(1);
		});
		DeferredTaskSubmission::Submit([&] {
			std::lock_guard takeQueue(queue);
			order.push_back(2);
		});
	});
	drain.join();
	Require(order == std::vector<int>{1, 2, 3, 4}, "Concurrent producers reordered or interleaved submission batches");
}

void CheckPublisherFailure()
{
	std::recursive_mutex actorUpdates;
	DeferredTaskSubmission submissions;
	int calls = 0;
	bool threw = false;
	try {
		submissions.WhileLocked(actorUpdates, [&] {
			DeferredTaskSubmission::Submit([] { throw std::runtime_error("enqueue failed"); });
			DeferredTaskSubmission::Submit([&] { ++calls; });
		});
	} catch (const std::runtime_error&) {
		threw = true;
	}
	submissions.WhileLocked(actorUpdates, [&] {
		DeferredTaskSubmission::Submit([&] { ++calls; });
	});
	Require(threw && calls == 2, "An enqueue failure stranded unrelated work or the publisher role");
}
}

int main()
{
	CheckLockOrder(false);
	CheckLockOrder(true);
	CheckThrowingFlushObserver();
	CheckConcurrentPublisherOrder();
	CheckPublisherFailure();
	std::cout << "PASS: lock order, nested dispatch, flush retry, concurrent FIFO, publisher failure, delegate lifetime\n";
}
