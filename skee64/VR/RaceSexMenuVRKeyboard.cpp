#include "RaceSexMenuVRKeyboard.h"
#include "RaceSexMenuVRKeyboardPolicy.h"
#include "VRRuntime.h"

#if defined(ENABLE_SKYRIM_VR)
#include "RE/B/BSOpenVR.h"
#include "RE/G/GFxFunctionHandler.h"
#include "RE/G/GFxMovieView.h"
#include "RE/G/GFxValue.h"
#include "RE/R/RaceSexMenu.h"
#include "RE/U/UI.h"
#include <array>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>

namespace
{
	using Clock = std::chrono::steady_clock;
	constexpr auto kKeyboardTimeout = std::chrono::minutes(5);
	constexpr std::uint32_t kMaximumTextBytes = 255;
	using SKEE::VR::KeyboardPolicy::IsValidUTF8;
	struct Session
	{
		RE::GFxMovie* owner{}; // identity only; never dereferenced or retained as a GFx object
		vr::IVROverlay* overlay{};
		vr::VROverlayHandle_t handle{ vr::k_ulOverlayHandleInvalid };
		std::uint32_t id{};
		bool ownsKeyboard{};
		bool streamed{};
		std::string status{ "idle" };
		std::string text;
		std::string error;
		Clock::time_point deadline;
	};
	Session g_session;
	std::uint32_t g_nextId{};
	std::mutex g_keyboardMutex;

	bool IsCurrentMovie(RE::GFxMovie* a_movie)
	{
		auto* ui = RE::UI::GetSingleton();
		auto menu = ui ? ui->GetMenu<RE::RaceSexMenu>() : nullptr;
		return menu && menu->uiMovie.get() == a_movie;
	}

	void ReleaseOverlay()
	{
		// Only a successful ShowKeyboardForOverlay gives us dismissal ownership.
		if (g_session.overlay && g_session.handle != vr::k_ulOverlayHandleInvalid) {
			if (g_session.ownsKeyboard) g_session.overlay->HideKeyboard();
			g_session.overlay->DestroyOverlay(g_session.handle);
		}
		g_session.ownsKeyboard = false;
		g_session.handle = vr::k_ulOverlayHandleInvalid;
		g_session.overlay = nullptr;
	}

	void Finish(const char* a_status, const char* a_error = "")
	{
		ReleaseOverlay();
		g_session.status = a_status;
		g_session.error = a_error;
		SKSE::log::info("RaceMenu VR keyboard {}: {} ({})", g_session.id, a_status, a_error);
	}

	bool RefreshBufferedDraft()
	{
		using SKEE::VR::KeyboardPolicy::BufferResult;
		const auto result = SKEE::VR::KeyboardPolicy::ReadBufferedDraft(g_session.streamed, g_session.text,
			[](char* text, std::uint32_t capacity) { return g_session.overlay->GetKeyboardText(text, capacity); });
		if (result == BufferResult::TooLong || result == BufferResult::Invalid) {
			Finish("unavailable", result == BufferResult::TooLong ? "text_exceeds_byte_budget" : "invalid_utf8");
			return false;
		}
		return true;
	}

	void Poll()
	{
		if (g_session.status != "pending") return;
		if (!IsCurrentMovie(g_session.owner)) {
			Finish("cancelled", "menu_closed");
			return;
		}
		if (Clock::now() >= g_session.deadline) {
			Finish("cancelled", "timeout");
			return;
		}
		// Consume only our overlay's queue, never the game's global event stream.
		vr::VREvent_t event{};
		for (unsigned i = 0; i < 32 && g_session.overlay->PollNextOverlayEvent(g_session.handle, &event, sizeof(event)); ++i) {
			if (event.eventType == vr::VREvent_KeyboardCharInput || event.eventType == vr::VREvent_KeyboardDone) {
                if (event.data.keyboard.uUserValue && event.data.keyboard.uUserValue != g_session.id) continue;
                if (event.eventType == vr::VREvent_KeyboardCharInput) {
                    const auto& input = event.data.keyboard.cNewInput;
                    const auto* end = static_cast<const char*>(std::memchr(input, '\0', sizeof(input)));
                    const std::string_view chunk(input, end ? static_cast<std::size_t>(end - input) : sizeof(input));
                    // Keep the session's backend contract fixed. OCU's full
                    // buffer remains authoritative even if an event has text.
                    if (g_session.streamed && !chunk.empty()) {
                        const auto edit = SKEE::VR::KeyboardPolicy::EditDraft(g_session.text, chunk);
                        if (edit == SKEE::VR::KeyboardPolicy::EditResult::Cancel) {
                            Finish("cancelled"); return;
                        }
                        if (edit == SKEE::VR::KeyboardPolicy::EditResult::Invalid) {
                            Finish("unavailable", "invalid_character_input"); return;
                        }
                        g_session.error = edit == SKEE::VR::KeyboardPolicy::EditResult::TooLong ? "text_exceeds_byte_budget" : "";
                    }
                    continue;
                } else if (g_session.streamed) {
                    // GetKeyboardText is not authoritative in minimal mode;
                    // some SteamVR versions return only the last entered key.
                    Finish("accepted"); return;
                }
				// Read final authoritative buffer before releasing the keyboard.
				if (!RefreshBufferedDraft()) return;
				Finish("accepted"); return;
			}
			if (event.eventType == vr::VREvent_KeyboardClosed) {
				Finish("cancelled");
				return;
			}
		}
		// OCU's buffered mode need not dispatch a CharInput event for each edit.
		// The existing UI timer polls this only while our own session is pending.
		RefreshBufferedDraft();
	}

	class KeyboardFunction final : public RE::GFxFunctionHandler
	{
	public:
		void Call(Params& a_params) override
		{
			if (!a_params.retVal) return;
			std::uint32_t id{};
			std::string status, text, error;
			{
				std::scoped_lock lock(g_keyboardMutex);
				const auto operation = reinterpret_cast<std::uintptr_t>(a_params.userData);
				if (!IsCurrentMovie(a_params.movie)) {
					status = "unavailable";
					error = "not_current_racemenu";
				} else if (operation == 0) {
					// Reconcile a stale/expired owner before testing busy. Closing the
					// old movie can make its onUnload cancellation no longer callable.
					if (g_session.status == "pending") Poll();
					if (g_session.status == "pending") {
						status = "unavailable";
						error = "keyboard_busy";
					} else if (a_params.argCount != 2 || !a_params.args[0].IsString() || !a_params.args[1].IsString()) {
						status = "unavailable";
						error = "invalid_arguments";
					} else {
						const std::string kind = a_params.args[0].GetString();
						const std::string initial = a_params.args[1].GetString();
						if ((kind != "filter" && kind != "name") || initial.size() > kMaximumTextBytes || !IsValidUTF8(initial)) {
							status = "unavailable";
							error = "invalid_text_request";
						} else {
							g_session = Session{};
							g_session.owner = a_params.movie;
							if (++g_nextId == 0) ++g_nextId;
							g_session.id = g_nextId;
							g_session.text = initial;
                            const auto runtime = SKEE::VR::ActiveRuntime();
                            g_session.streamed = SKEE::VR::UsesStreamedKeyboard(runtime);
							auto* openVR = RE::BSOpenVR::GetSingleton();
							// Use Skyrim's active proxy: SteamVR or OpenComposite/OCU.
							g_session.overlay = openVR ? RE::BSOpenVR::GetIVROverlayFromContext(&openVR->vrContext) : nullptr;
							if (!g_session.overlay) {
								Finish("unavailable", "overlay_unavailable");
							} else {
								const auto key = "RaceMenuNGVR2.TextEntry." + std::to_string(g_session.id);
								const auto create = g_session.overlay->CreateOverlay(key.c_str(), "RaceMenu text entry", &g_session.handle);
								if (create != vr::VROverlayError_None) {
									Finish("unavailable", "create_overlay_failed");
								} else {
									const auto shown = g_session.overlay->ShowKeyboardForOverlay(g_session.handle,
										vr::k_EGamepadTextInputModeNormal, vr::k_EGamepadTextInputLineModeSingleLine,
										kind == "name" ? "Character name" : "Filter RaceMenu options",
										kMaximumTextBytes, initial.c_str(), g_session.streamed, g_session.id);
									if (shown != vr::VROverlayError_None) {
										Finish("unavailable", "show_keyboard_failed");
									} else {
										g_session.ownsKeyboard = true;
										g_session.status = "pending";
										g_session.deadline = Clock::now() + kKeyboardTimeout;
                                        SKSE::log::info("RaceMenu VR keyboard {}: opened for {} ({}, runtime={})", g_session.id, kind,
                                            g_session.streamed ? "streamed" : "buffered", SKEE::VR::RuntimeName(runtime));
									}
								}
							}
							id = g_session.id; status = g_session.status; text = g_session.text; error = g_session.error;
						}
					}
				} else if (a_params.argCount != 1 || !a_params.args[0].IsNumber() ||
					g_session.owner != a_params.movie || a_params.args[0].GetNumber() != g_session.id) {
					status = "unavailable";
					error = "stale_request";
				} else {
					if (operation == 2 && g_session.status == "pending") Finish("cancelled");
					else Poll();
					id = g_session.id; status = g_session.status; text = g_session.text; error = g_session.error;
				}
			}
			// Construct and return in the calling movie, after releasing the state lock.
			a_params.movie->CreateObject(a_params.retVal);
			a_params.retVal->SetMember("id", RE::GFxValue{ static_cast<double>(id) });
			for (const auto& [name, value] : { std::pair{ "status", status }, std::pair{ "text", text }, std::pair{ "error", error } }) {
				RE::GFxValue string;
				a_params.movie->CreateString(&string, value.c_str());
				a_params.retVal->SetMember(name, string);
			}
		}
	};
}

namespace SKEE::VR
{
	void RegisterRaceSexMenuKeyboard(RE::GFxMovieView* a_view, RE::GFxValue* a_root)
	{
		static RE::GPtr<KeyboardFunction> handler{ new KeyboardFunction{} };
		constexpr const char* names[]{ "BeginVRTextEntry", "PollVRTextEntry", "CancelVRTextEntry" };
		for (std::uintptr_t i = 0; i < 3; ++i) {
			RE::GFxValue function;
			a_view->CreateFunction(&function, handler.get(), reinterpret_cast<void*>(i));
			a_root->SetMember(names[i], function);
		}
	}
	void CancelRaceSexMenuKeyboard()
	{
		std::scoped_lock lock(g_keyboardMutex);
		if (g_session.status == "pending") Finish("cancelled", "menu_closed");
		g_session.owner = nullptr;
	}
}
#else
namespace SKEE::VR
{
	void RegisterRaceSexMenuKeyboard(RE::GFxMovieView*, RE::GFxValue*) {}
	void CancelRaceSexMenuKeyboard() {}
}
#endif
