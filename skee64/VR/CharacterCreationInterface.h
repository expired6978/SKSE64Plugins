#pragma once

#include "IPluginInterface.h"
#include "VRMenuOptionsPolicy.h"

#include <RE/B/BSTEvent.h>
#include <RE/M/MenuOpenCloseEvent.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <mutex>
#include <vector>
namespace RE { class GFxMovie; class GFxValue; }

class CharacterCreationInterface final :
	public ICharacterCreationInterface,
	public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	skee_u32 GetVersion() override { return kCurrentPluginVersion; }
	void Revert() override;

	bool IsActive() override;
	State GetState() override;
	FinishResult FinishWithDefaultName() override;
	FinishResult FinishWithName(const char* name) override;
	skee_u64 GetCapabilities() override;
	NameResult SetName(const char* name) override;
	skee_u32 GetName(char* buffer, skee_u32 capacity) override;
	skee_u32 GetFilter(char* buffer, skee_u32 capacity) override;
	NameResult SetFilter(const char* text) override;
	bool Subscribe(ChangeCallback callback, void* context) override;
	void Unsubscribe(ChangeCallback callback, void* context) override;
	skee_u32 GetView() override;
	bool SetView(skee_u32 view) override;
	void PublishText(std::string name, std::string filter);
	void RegisterMovie(RE::GFxMovie* movie, RE::GFxValue* root);

	RE::BSEventNotifyControl ProcessEvent(
		const RE::MenuOpenCloseEvent* event,
		RE::BSTEventSource<RE::MenuOpenCloseEvent>* source) override;

	void ObserveCurrentState();
    void BeginNewGame();
    void BeginMainMenuNewGame();
    void OnSaveLoading();
    void CancelConfiguredName();

private:
	static constexpr std::uint32_t kReadyProbeAttempts = 8;

	void QueueReadyProbe(std::uint32_t attempt = 1);
	void ProbeReady(std::uint32_t attempt);
	FinishResult QueueFinish(std::string name, bool useCurrentName);
	NameResult QueueName(std::string name, bool configured = false);
    void ApplyConfiguredName();
	void FinishOnGameThread(std::string name, bool useCurrentName, std::uint64_t generation);

	std::atomic<State> state_{ kInactive };
	std::atomic<std::uint64_t> sessionGeneration_{ 0 };
    SKEE::VR::MenuOptionsPolicy::NameStartIntent nameStartIntent_;
	struct Listener { ChangeCallback callback; void* context; };
	std::mutex snapshotMutex_;
	std::string name_, filter_;
	unsigned publishedView_{};
	std::vector<Listener> listeners_;
	void Notify(Change change) noexcept;
};
