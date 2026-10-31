#pragma once

// Project-local vtable-layout declarations for the SKSE runtime's delay-functor
// system. CommonLibSSE-NG only *forward-declares* these types (see
// SKSE/Impl/Stubs.h:3 `class SKSEDelayFunctorManager;`), but the SKSE runtime
// hands us a live reference to the real manager, so we declare just enough of
// the virtual layout to call into it and to inherit from — no reimplementation.
//
// The vtable slot order below matches the legacy SKSE classes
// (skse64/PapyrusObjects.h + skse64/PapyrusDelayFunctors.h), so virtual dispatch
// through these declarations reaches the correct runtime methods.
//
// `RE::BSScript::Variable` is the CommonLib equivalent of the legacy Papyrus
// `VMValue`; it provides SetBool/SetInt/SetFloat/SetString/... for the result
// a latent script stack receives.

#include "RE/M/MemoryManager.h"  // TES_HEAP_REDEFINE_NEW()
#include "RE/V/Variable.h"       // RE::BSScript::Variable (== legacy VMValue)
#include "SKSE/Interfaces.h"     // SKSE::SerializationInterface, ObjectInterface
#include <type_traits>
#include <utility>

namespace SKSE
{

// Tag type for the serialization constructor (legacy PapyrusObjects.h:17).
struct SerializationTag {};

// Base interface for a heap-allocated, co-save-serializable SKSE object.
class ISKSEObject
{
public:
	virtual ~ISKSEObject() = default;

	virtual const char* ClassName() const = 0;
	virtual std::uint32_t ClassVersion() const = 0;

	virtual bool Save(SKSE::SerializationInterface* a_intfc) = 0;
	virtual bool Load(SKSE::SerializationInterface* a_intfc, std::uint32_t a_version) = 0;
};

// A functor that is deferred and later run on the game thread.
class ISKSEDelayFunctor : public ISKSEObject
{
public:
	// Runs the deferred work. Set a_result to the value returned to any waiting
	// latent script stack (e.g. a_result.SetBool(...)).
	virtual void Run(RE::BSScript::Variable& a_result) = 0;

	// Return true to have the manager re-queue this functor after a_delayMS ms.
	virtual bool ShouldReschedule(std::int32_t& a_delayMS) = 0;

	// Return true to resume the latent Papyrus stack identified by a_stackId
	// with the value produced by Run().
	virtual bool ShouldResumeStack(std::uint32_t& a_stackId) = 0;
};

// Make the string-view literal available for TES_HEAP_REDEFINE_NEW().
using namespace std::literals;

// A delay functor that resumes a latent Papyrus stack. Declared as a full class
// (not vtable-only) so concrete functors can inherit from it and use StackId().
class LatentSKSEDelayFunctor : public ISKSEDelayFunctor
{
public:
	explicit LatentSKSEDelayFunctor(std::uint32_t a_stackId) : stackId_(a_stackId) {}
	explicit LatentSKSEDelayFunctor(SerializationTag) : stackId_(0) {}

	virtual ~LatentSKSEDelayFunctor() = default;

	// Re-declared pure (matches legacy); concrete functors implement it.
	virtual void Run(RE::BSScript::Variable& a_result) = 0;

	bool ShouldReschedule(std::int32_t&) override { return false; }
	bool ShouldResumeStack(std::uint32_t& a_stackId) override { a_stackId = stackId_; return true; }

	bool Save(SKSE::SerializationInterface* a_intfc) override
	{
		return a_intfc->WriteRecordData(&stackId_, sizeof(stackId_));
	}
	bool Load(SKSE::SerializationInterface* a_intfc, std::uint32_t) override
	{
		return a_intfc->ReadRecordData(&stackId_, sizeof(stackId_)) > 0;
	}

	std::uint32_t StackId() const { return stackId_; }

	// Enqueued functors must live on the game/TES heap (SKSE deletes them with
	// the matching allocator).
	TES_HEAP_REDEFINE_NEW();

protected:
	std::uint32_t stackId_ = 0;
};

// Factory for an ISKSEObject (legacy PapyrusObjects.h). The object registry
// stores the vtable directly, so RegisterFactory may be handed a temporary.
class ISKSEObjectFactory
{
public:
	virtual ~ISKSEObjectFactory() = default;
	virtual ISKSEObject* Create() const = 0;
	virtual const char* ClassName() const = 0;
};

template <class T>
class ConcreteSKSEObjectFactory : public ISKSEObjectFactory
{
public:
	ISKSEObject* Create() const override
	{
		SerializationTag tag;
		return new T(tag);  // game heap via TES_HEAP_REDEFINE_NEW on the base
	}
	const char* ClassName() const override
	{
		SerializationTag tag;
		T tempInstance(tag);
		return tempInstance.ClassName();
	}
};

}

// CommonLib 7a60f4de forward-declares these two runtime-owned types at GLOBAL
// scope in SKSE/Impl/Stubs.h. ObjectInterface returns those exact types, not
// SKSE namespace classes. Complete only their used virtual slots here; never
// construct, copy, delete or access data members through these bridge views.
// Official SKSE PapyrusDelayFunctors.h / PapyrusObjects.h define these slots
// without a virtual destructor. Enqueue and RegisterFactory are non-const.
// This pin-specific bridge does not claim the runtime classes' full layout.
class SKSEDelayFunctorManager
{
public:
	// Slot 0; transfers ownership of the game-heap functor to the runtime.
	virtual void Enqueue(SKSE::ISKSEDelayFunctor* a_func, std::int32_t a_delayMS = 0) = 0;
};

class SKSEObjectRegistry
{
public:
	virtual void RegisterFactory(SKSE::ISKSEObjectFactory* a_factory) = 0;
	virtual const SKSE::ISKSEObjectFactory* GetFactoryByName(const char* a_name) const = 0;
};

// Reject a dependency whose ObjectInterface binds a different namespace/API.
static_assert(std::is_same_v<decltype(std::declval<const SKSE::ObjectInterface&>().GetDelayFunctorManager()), SKSEDelayFunctorManager&>);
static_assert(std::is_same_v<decltype(std::declval<const SKSE::ObjectInterface&>().GetObjectRegistry()), SKSEObjectRegistry&>);
