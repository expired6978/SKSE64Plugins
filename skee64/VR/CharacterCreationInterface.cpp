#include "CharacterCreationInterface.h"
#include "CharacterNameUpdate.h"
#include "RaceSexMenuFaceView.h"
#include "AvatarLighting.h"
#include "MenuConfiguration.h"
#include "VRMenuOptionsPolicy.h"
#include "VRSessionLeasePolicy.h"

#include "SKSE/API.h"
#include "SKSE/Interfaces.h"

#include <RE/P/PlayerCharacter.h>
#include <RE/G/GFxMovieView.h>
#include <RE/G/GFxValue.h>
#if defined(ENABLE_SKYRIM_VR)
#include <RE/G/GameSettingCollection.h>
#include <RE/F/FunctionArguments.h>
#include <RE/S/Setting.h>
#include <RE/V/VirtualMachine.h>
#endif
#include <RE/R/RaceSexMenu.h>
#include <RE/M/MainMenu.h>
#include <RE/U/UI.h>
#include <RE/U/UIMessageQueue.h>

#include <cstring>
#include <string_view>
#include <utility>
#include <algorithm>
#include <RE/G/GFxFunctionHandler.h>

namespace
{
	class PublishMenuText final : public RE::GFxFunctionHandler
	{
	public:
		explicit PublishMenuText(CharacterCreationInterface* owner) : owner_(owner) {}
		void Call(Params& args) override
		{
			if (args.argCount != 2 || !args.args[0].IsString() || !args.args[1].IsString()) return;
			const auto* name = args.args[0].GetString();
			const auto* filter = args.args[1].GetString();
			if (!name || !filter || std::strlen(name) > 255 || std::strlen(filter) > 255) return;
			try { owner_->PublishText(name, filter); } catch (...) { }
		}
	private:
		CharacterCreationInterface* owner_;
	};
	skee_u32 CopySnapshot(const std::string& text, char* buffer, skee_u32 capacity)
	{
		const auto required = static_cast<skee_u32>(text.size()+1);
		if (buffer && capacity >= required) std::memcpy(buffer, text.c_str(), required);
		else if (buffer && capacity) buffer[0] = 0;
		return required;
	}
	constexpr auto kDefaultCharacterName = "Prisoner";
	constexpr std::size_t kMaximumCharacterNameBytes = 255;

	bool IsRaceSexMenuEvent(const RE::MenuOpenCloseEvent* a_event)
	{
		const auto* menuName = a_event ? a_event->menuName.c_str() : nullptr;
		return menuName && std::string_view{ menuName } == RE::RaceSexMenu::MENU_NAME;
	}
}

void CharacterCreationInterface::Revert()
{
	nameStartIntent_.Revert();
	SKEE::FaceView::Restore();
	SKEE::AvatarLighting::Reset();
	sessionGeneration_.fetch_add(1);
	state_.store(kInactive);
	{ std::scoped_lock lock(snapshotMutex_); name_.clear(); filter_.clear(); }
	Notify(kStateChanged);
}

skee_u32 CharacterCreationInterface::GetName(char* buffer, skee_u32 capacity)
{ std::scoped_lock lock(snapshotMutex_); return CopySnapshot(name_, buffer, capacity); }
skee_u64 CharacterCreationInterface::GetCapabilities()
{
	return kCapabilityFinish | kCapabilitySetName | kCapabilityNotifications |
		(REL::Module::IsVR() ? kCapabilityTextSnapshot | kCapabilitySetFilter : 0) |
		(SKEE::FaceView::Supported() ? kCapabilityView : 0);
}
skee_u32 CharacterCreationInterface::GetView() { return SKEE::FaceView::Current(); }
bool CharacterCreationInterface::SetView(skee_u32 view) { return GetState() == kReady && SKEE::FaceView::Request(view); }
skee_u32 CharacterCreationInterface::GetFilter(char* buffer, skee_u32 capacity)
{ std::scoped_lock lock(snapshotMutex_); return CopySnapshot(filter_, buffer, capacity); }
bool CharacterCreationInterface::Subscribe(ChangeCallback callback, void* context)
{
	if (!callback) return false;
	try {
		std::scoped_lock lock(snapshotMutex_);
		for (const auto& item : listeners_) if (item.callback == callback && item.context == context) return true;
		if (listeners_.size() >= 64) return false;
		listeners_.push_back({callback, context}); return true;
	} catch (...) { return false; }
}
void CharacterCreationInterface::Unsubscribe(ChangeCallback callback, void* context)
{
	std::scoped_lock lock(snapshotMutex_);
	std::erase_if(listeners_, [&](const auto& item) { return item.callback == callback && item.context == context; });
}
void CharacterCreationInterface::Notify(Change change) noexcept
{
	try {
		std::vector<Listener> copy;
		{ std::scoped_lock lock(snapshotMutex_); copy = listeners_; }
		for (const auto& item : copy) try { item.callback(change, item.context); } catch (...) { }
	} catch (...) { }
}
void CharacterCreationInterface::PublishText(std::string name, std::string filter)
{
	bool nameChanged, filterChanged, viewChanged;
	{
		std::scoped_lock lock(snapshotMutex_);
		nameChanged = name_ != name; filterChanged = filter_ != filter;
		viewChanged = publishedView_ != GetView(); publishedView_ = GetView();
		name_ = std::move(name); filter_ = std::move(filter);
	}
	if (nameChanged) Notify(kNameChanged);
	if (filterChanged) Notify(kFilterChanged);
	if (viewChanged) Notify(kViewChanged);
}
void CharacterCreationInterface::RegisterMovie(RE::GFxMovie* movie, RE::GFxValue* root)
{
	RE::GPtr<PublishMenuText> handler{ new PublishMenuText(this) };
	RE::GFxValue function;
	movie->CreateFunction(&function, handler.get());
	root->SetMember("PublishMenuText", function);
}
ICharacterCreationInterface::NameResult CharacterCreationInterface::SetFilter(const char* text)
{
	if (!REL::Module::IsVR()) return kNameNotReady;
	if (!text || std::strlen(text) > 255) return kNameInvalid;
	const auto generation = sessionGeneration_.load();
	if (GetState() == kInactive) return kNameNotActive;
	if (GetState() != kReady) return kNameNotReady;
	auto* tasks = SKSE::GetTaskInterface();
	if (!tasks) return kNameTaskInterfaceUnavailable;
	try {
		tasks->AddTask([this, generation, filter = std::string(text)] {
			if (sessionGeneration_.load() != generation || GetState() != kReady) return;
			auto* ui = RE::UI::GetSingleton();
			auto menu = ui ? ui->GetMenu<RE::RaceSexMenu>() : RE::GPtr<RE::RaceSexMenu>{};
			if (!menu || !menu->uiMovie || !REL::Module::IsVR()) return;
			RE::GFxValue argument;
			menu->uiMovie->CreateString(&argument, filter.c_str());
			menu->uiMovie->Invoke("_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.SetVRFilterText", nullptr, &argument, 1);
		});
	} catch (...) { return kNameTaskInterfaceUnavailable; }
	return kNameQueued;
}

bool CharacterCreationInterface::IsActive()
{
	return GetState() != kInactive;
}

ICharacterCreationInterface::State CharacterCreationInterface::GetState()
{
	return state_.load();
}

ICharacterCreationInterface::FinishResult CharacterCreationInterface::FinishWithDefaultName()
{
	return QueueFinish({}, true);
}

ICharacterCreationInterface::FinishResult CharacterCreationInterface::FinishWithName(const char* a_name)
{
	if (!a_name) {
		return kFinishInvalidName;
	}

	const auto length = std::strlen(a_name);
	if (length == 0 || length > kMaximumCharacterNameBytes) {
		return kFinishInvalidName;
	}

	try {
		return QueueFinish(std::string{ a_name, length }, false);
	} catch (...) {
		// The public plugin ABI must not let allocation failures escape into the
		// calling automation plugin.
		return kFinishInvalidName;
	}
}

ICharacterCreationInterface::NameResult CharacterCreationInterface::SetName(const char* a_name)
{
	if (!a_name) {
		return kNameInvalid;
	}
	const auto length = std::strlen(a_name);
	if (length == 0 || length > kMaximumCharacterNameBytes) {
		return kNameInvalid;
	}
	try {
		return QueueName(std::string{ a_name, length });
	} catch (...) {
		return kNameInvalid;
	}
}

RE::BSEventNotifyControl CharacterCreationInterface::ProcessEvent(
	const RE::MenuOpenCloseEvent* a_event,
	RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
	if (a_event && a_event->opening && a_event->menuName == RE::MainMenu::MENU_NAME) {
		CancelConfiguredName();
		sessionGeneration_.fetch_add(1);
		return RE::BSEventNotifyControl::kContinue;
	}
	if (!IsRaceSexMenuEvent(a_event)) {
		return RE::BSEventNotifyControl::kContinue;
	}

	sessionGeneration_.fetch_add(1);
	if (a_event->opening) {
		state_.store(kOpening);
		Notify(kStateChanged);
		QueueReadyProbe();
	} else {
		// Never transfer an abandoned first-creation request to a later reopen.
		nameStartIntent_.Consume();
		SKEE::FaceView::Restore();
		SKEE::AvatarLighting::Reset();
		state_.store(kInactive);
		{ std::scoped_lock lock(snapshotMutex_); filter_.clear(); }
		Notify(kStateChanged);
		SKSE::log::info("Character-creation automation state: inactive");
	}

	return RE::BSEventNotifyControl::kContinue;
}

void CharacterCreationInterface::ObserveCurrentState()
{
	auto* ui = RE::UI::GetSingleton();
	if (!ui || !ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME)) {
		state_.store(kInactive);
		return;
	}

	state_.store(kOpening);
	QueueReadyProbe();
}

void CharacterCreationInterface::QueueReadyProbe(std::uint32_t a_attempt)
{
	if (auto* tasks = SKSE::GetTaskInterface()) {
		try {
			tasks->AddTask([this, a_attempt] { ProbeReady(a_attempt); });
		} catch (...) {
			SKSE::log::error("Could not queue character-creation readiness probe {}", a_attempt);
		}
	}
}

void CharacterCreationInterface::ProbeReady(std::uint32_t a_attempt)
{
	if (state_.load() != kOpening) {
		return;
	}

	auto* ui = RE::UI::GetSingleton();
	auto menu = ui ? ui->GetMenu<RE::RaceSexMenu>() : RE::GPtr<RE::RaceSexMenu>{};
	if (ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME) && menu && menu->uiMovie) {
#if defined(ENABLE_SKYRIM_VR)
		// VR names are accepted inside the menu, not after this confirmation.
		// Keep the stock confirmation callback and both of its actions intact.
		auto* settings = RE::GameSettingCollection::GetSingleton();
		auto* confirmation = settings ? settings->GetSetting("sRSMConfirm") : nullptr;
		if (confirmation && confirmation->GetType() == RE::Setting::Type::kString) {
			// SKSE's setter owns the engine-heap allocation and managed-string flag.
			// CommonLib Setting::SetString uses the CRT allocator instead.
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result;
			if (!vm || !vm->DispatchStaticCall("Game", "SetGameSettingString",
				RE::MakeFunctionArguments(RE::BSFixedString("sRSMConfirm"), RE::BSFixedString("Exit Character Creation?")), result)) {
				SKSE::log::warn("RaceMenu VR exit confirmation update could not be dispatched");
			}
		} else {
			SKSE::log::warn("RaceMenu VR exit confirmation setting unavailable");
		}
#endif
		state_.store(kReady);
		ApplyConfiguredName();
		Notify(kStateChanged);
		SKSE::log::info("Character-creation automation state: ready");
		return;
	}

	if (!ui || !ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME)) {
		state_.store(kInactive);
		return;
	}

	if (a_attempt < kReadyProbeAttempts) {
		QueueReadyProbe(a_attempt + 1);
	} else {
		SKSE::log::warn(
			"Character-creation automation remained in opening state after {} readiness probes",
			a_attempt);
	}
}

ICharacterCreationInterface::FinishResult CharacterCreationInterface::QueueFinish(
	std::string a_name,
	bool a_useCurrentName)
{
	const auto generation = sessionGeneration_.load();
	auto expected = kReady;
	if (!state_.compare_exchange_strong(expected, kFinishing)) {
		switch (expected) {
		case kInactive:
			return kFinishNotActive;
		case kOpening:
			return kFinishNotReady;
		case kFinishing:
			return kFinishAlreadyPending;
		case kReady:
		default:
			return kFinishNotReady;
		}
	}
	if (!SKEE::VR::SessionLease::IsCurrent(
			generation, sessionGeneration_.load(), state_.load(), kFinishing)) {
		// A close/reopen may have completed between observing Ready and claiming
		// Finishing. Release only the state that this request actually claimed.
		auto finishing = kFinishing;
		state_.compare_exchange_strong(finishing, kReady);
		return kFinishNotReady;
	}

	auto* tasks = SKSE::GetTaskInterface();
	if (!tasks) {
		if (sessionGeneration_.load() == generation) {
			auto finishing = kFinishing;
			state_.compare_exchange_strong(finishing, kReady);
		}
		return kFinishTaskInterfaceUnavailable;
	}

	try {
		tasks->AddTask([this, generation, name = std::move(a_name), a_useCurrentName]() mutable {
			if (!SKEE::VR::SessionLease::IsCurrent(
					generation, sessionGeneration_.load(), state_.load(), kFinishing)) {
				return;
			}
			FinishOnGameThread(std::move(name), a_useCurrentName, generation);
		});
	} catch (...) {
		if (sessionGeneration_.load() == generation) {
			auto finishing = kFinishing;
			state_.compare_exchange_strong(finishing, kReady);
		}
		return kFinishTaskInterfaceUnavailable;
	}
	Notify(kStateChanged);
	return kFinishQueued;
}

void CharacterCreationInterface::CancelConfiguredName()
{
    nameStartIntent_.Cancel();
}

void CharacterCreationInterface::BeginNewGame()
{
    nameStartIntent_.EngineNewGame();
    // SKSE's new-game notification can precede or follow the menu-open probe.
    ApplyConfiguredName();
}

void CharacterCreationInterface::BeginMainMenuNewGame()
{
    nameStartIntent_.StartFromMenu();
    SKSE::log::info("Configured VR name: explicit main-menu New Game intent armed");
    // The creation menu will be opened by the original callback. Do not apply
    // to any old character/menu that happens to be present during transition.
}

void CharacterCreationInterface::OnSaveLoading()
{
    nameStartIntent_.SaveLoading();
    SKSE::log::info("Configured VR name: save-load transition, explicit-start intent retained={}",
        nameStartIntent_.Pending());
}

void CharacterCreationInterface::ApplyConfiguredName()
{
#if defined(ENABLE_SKYRIM_VR)
    if (!REL::Module::IsVR() || state_.load() != kReady) return;
    const auto& name = SKEE::MenuConfiguration::PlayerName();
    if (SKEE::VR::MenuOptionsPolicy::ApplyPlayerName(nameStartIntent_.Pending(),
        SKEE::MenuConfiguration::OverrideExistingPlayerName(), name)) {
        if (QueueName(name, true) != kNameQueued)
            SKSE::log::warn("Could not queue configured VR player name");
    }
#endif
}

ICharacterCreationInterface::NameResult CharacterCreationInterface::QueueName(std::string a_name, bool a_configured)
{
	const auto generation = sessionGeneration_.load();
	const auto current = state_.load();
	if (current == kInactive) {
		return kNameNotActive;
	}
	if (current != kReady) {
		return kNameNotReady;
	}
	auto* tasks = SKSE::GetTaskInterface();
	if (!tasks) {
		return kNameTaskInterfaceUnavailable;
	}
	try {
		tasks->AddTask([this, generation, name = std::move(a_name), a_configured] {
			if (sessionGeneration_.load() != generation || state_.load() != kReady) return;
            if (a_configured && !SKEE::VR::MenuOptionsPolicy::ApplyPlayerName(nameStartIntent_.Pending(),
                SKEE::MenuConfiguration::OverrideExistingPlayerName(), name)) return;
			auto* ui = RE::UI::GetSingleton();
			auto menu = ui ? ui->GetMenu<RE::RaceSexMenu>() : RE::GPtr<RE::RaceSexMenu>{};
			if (ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME) && menu) {
				if (SKEE::UpdateCharacterNameWithoutFinishing(name.c_str()) && menu->uiMovie && REL::Module::IsVR()) {
                    if (a_configured) {
                        nameStartIntent_.Consume();
                        SKSE::log::info("Configured VR name applied without finishing character creation");
                    }
					RE::GFxValue argument;
					menu->uiMovie->CreateString(&argument, name.c_str());
					if (!menu->uiMovie->Invoke("_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.SetNameText", nullptr, &argument, 1))
                        SKSE::log::warn("Player name updated but RaceMenu name label callback unavailable");
				}
			}
		});
	} catch (...) {
		return kNameTaskInterfaceUnavailable;
	}
	return kNameQueued;
}

void CharacterCreationInterface::FinishOnGameThread(
	std::string a_name,
	bool a_useCurrentName,
	std::uint64_t a_generation)
{
	if (!SKEE::VR::SessionLease::IsCurrent(
			a_generation, sessionGeneration_.load(), state_.load(), kFinishing)) {
		return;
	}
	auto* ui = RE::UI::GetSingleton();
	auto menu = ui ? ui->GetMenu<RE::RaceSexMenu>() : RE::GPtr<RE::RaceSexMenu>{};
	if (!ui || !ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME) || !menu || !menu->uiMovie) {
		state_.store(kInactive);
		SKSE::log::warn("Character-creation finish request lost its RaceSexMenu before execution");
		return;
	}

	if (a_useCurrentName) {
		try {
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				if (const auto* currentName = player->GetName(); currentName && *currentName) {
					a_name = currentName;
				}
			}
		} catch (...) {
			a_name.clear();
		}
		if (a_name.empty()) {
			a_name = kDefaultCharacterName;
		}
	}

	auto* messages = RE::UIMessageQueue::GetSingleton();
	if (!messages) {
		if (sessionGeneration_.load() == a_generation) {
			auto finishing = kFinishing;
			state_.compare_exchange_strong(finishing, kReady);
		}
		SKSE::log::error("Character-creation finish request could not access the UI message queue");
		return;
	}

	// ChangeName is the native RaceSexMenu path used after the naming prompt;
	// the queued hide then lets RaceSexMenu run its ordinary close lifecycle.
	menu->ChangeName(a_name.c_str());
	messages->AddMessage(RE::RaceSexMenu::MENU_NAME.data(), RE::UI_MESSAGE_TYPE::kHide, nullptr);
	SKSE::log::info("Queued normal RaceSexMenu completion for character name '{}'", a_name);
}
