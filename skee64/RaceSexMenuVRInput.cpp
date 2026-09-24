#include "RaceSexMenuVRInput.h"
#include "SculptTrace.h"

#if defined(ENABLE_SKYRIM_VR)

#	include "RE/B/BSScaleformManager.h"
#	include "RE/B/BSInputDeviceManager.h"
#	include "RE/B/BSOpenVRControllerDevice.h"
#	include "RE/B/BSTriShape.h"
#	include "RE/B/ButtonEvent.h"
#	include "RE/G/GFxEvent.h"
#	include "RE/G/GFxFunctionHandler.h"
#	include "RE/G/GFxValue.h"
#	include "RE/G/GFxMovieView.h"
#	include "RE/I/InputEvent.h"
#	include "RE/I/INISettingCollection.h"
#	include "RE/M/MenuEventHandler.h"
#	include "RE/N/NiNode.h"
#	include "RE/P/PlayerCharacter.h"
#	include "RE/Offsets_VTABLE.h"
#	include "RE/R/RaceSexMenu.h"
#	include "RE/U/UI.h"
#	include "RE/U/UserEvents.h"
#	include "REL/Relocation.h"
#	include "SKSE/Trampoline.h"


#	include <array>
#	include <atomic>
#	include <chrono>
#	include <nlohmann/json.hpp>
#	include <mutex>
#	include <string>
#	include "BuildVersion.h"
#	include <cstring>
#	include <cstdint>
#	include <utility>
#	include <cmath>
#	include <DirectXPackedVector.h>
#	include "MenuBackgroundProjection.h"
#	include "MenuPolarPlacementPolicy.h"
#	include "MenuConfiguration.h"
#	include "VRMenuOptionsPolicy.h"
#	include "VRHookTransactionPolicy.h"
#	include "CooperativeRestorePolicy.h"
#	include "RaceSexMenuFaceView.h"
#	include "RaceSexMenuSwfPatch.h"

namespace RE::ScaleformEvent
{
	// Skyrim VR 1.4.15 engine endpoints, independently qualified against the exact
	// executable and retained full-memory dump. The engine owns the fixed event
	// pool and menu dispatcher; a stack GFxMouseEvent would bypass both.
	REL::Relocation<void**>                 g_scaleformGFxEventData{ REL::Offset(0x03013620) };
	REL::Relocation<QueueGFxMouseEvent_t>   QueueGFxMouseEvent{ REL::Offset(0x00F37680) };
	REL::Relocation<DispatchGFxEvent_t>     DispatchGFxEvent{ REL::Offset(0x00F209A0) };
}

namespace
{
	struct MouseCoords
	{
		std::uint64_t unknown;
		float         x;
		float         y;
	};
	static_assert(sizeof(MouseCoords) == 0x10);

	REL::Relocation<MouseCoords**> g_mouseCoords{ REL::Offset(0x02FEBC40) };

	constexpr auto kFirstVRInputDevice = std::to_underlying(RE::INPUT_DEVICE::kVivePrimary);
	constexpr auto kLastVRInputDevice = std::to_underlying(RE::INPUT_DEVICE::kWMRSecondary);
	constexpr std::uint32_t kScaleformPrimaryButton = 0;
	constexpr std::uintptr_t kRaceSexLoadMovieCallRva = 0x008DD500 + 0x66;
	constexpr const char* kVRRaceSexMovie = "VR/RaceSex_menu";
	constexpr std::array<std::uint8_t, 15> kQueueGFxMouseEventProlog{
		0x8B, 0x41, 0x0C, 0x4C, 0x8B, 0xD1, 0x83, 0xF8, 0x10, 0x0F, 0x83, 0x8B, 0x00, 0x00, 0x00
	};
	constexpr std::array<std::uint8_t, 10> kDispatchGFxEventProlog{
		0x40, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x30
	};
	using RaceSexMenuCanProcess_t = bool (*)(RE::MenuEventHandler*, RE::InputEvent*);
	using RaceSexMenuLoadMovie_t = bool (*)(
		RE::BSScaleformManager*,
		RE::IMenu*,
		RE::GPtr<RE::GFxMovieView>&,
		const char*,
		RE::GFxMovieView::ScaleModeType,
		float);

	RaceSexMenuCanProcess_t g_originalCanProcess{ nullptr };
	RaceSexMenuLoadMovie_t  g_originalLoadMovie{ nullptr };
	SKSE::Trampoline        g_loadMovieTrampoline{ "RaceSexMenu VR movie trampoline" };
	bool                    g_installed{ false };
	bool                    g_loggedMovieLoad{ false };
	bool                    g_raceSexMenuWorldTransformEnabled{ true };
	float                   g_raceSexMenuWorldScale{ 1.0F };
	float                   g_raceSexMenuWorldYaw{ 180.0F };
	float                   g_previousUIQuadScale{ 0.0F };
	float                   g_previousUIQuadRotation{ 0.0F };
	float                   g_appliedUIQuadScale{ 0.0F };
	float                   g_appliedUIQuadRotation{ 0.0F };
	RE::Setting*            g_uiQuadScaleSetting{ nullptr };
	RE::Setting*            g_uiQuadRotationSetting{ nullptr };
	bool                    g_uiQuadTransformOverridden{ false };
	RE::NiPointer<RE::NiNode>     g_raceSexMenuUINode;
	RE::NiMatrix3                 g_previousUINodeRotation;
	RE::NiMatrix3                 g_appliedUINodeRotation;
	RE::NiPointer<RE::NiNode>     g_raceSexMenuUINodeParent;
	RE::NiPointer<RE::BSTriShape> g_raceSexMenuUIQuadGeo;
	RE::NiMatrix3                 g_previousUIQuadGeoRotation;
	RE::NiMatrix3                 g_appliedUIQuadGeoRotation;
	RE::NiPointer<RE::NiNode>     g_raceSexMenuUIQuadGeoParent;
	bool                          g_uiNodeYawOverridden{ false };
	bool                          g_uiQuadGeoYawOverridden{ false };
	struct PolarObject
	{
		RE::NiTransform local, world, appliedLocal;
		bool applied{ false };
	};
	PolarObject g_polarNode{}, g_polarQuad{};
	RE::NiPoint3 g_polarU{}, g_polarV{}, g_polarUVOrigin{}, g_polarForward{};
	RE::NiPoint3 g_polarViewerAnchor{};
	unsigned g_polarAnchorView{~0U};
	bool g_polarCaptured{}, g_polarDescendant{};
	RE::NiPointer<RE::NiNode> g_polarNodeIdentity;
	RE::NiPointer<RE::BSTriShape> g_polarQuadIdentity;
	RE::NiPointer<RE::NiNode> g_polarNodeParentIdentity;
	RE::NiPointer<RE::NiNode> g_polarQuadParentIdentity;
	std::atomic<unsigned> g_polarState{0}; // 0 pending, 1 applied, 2 unavailable
	std::atomic<bool> g_polarQueued{false};
	std::atomic<std::uint64_t> g_polarGeneration{0};

	RE::NiMatrix3 Basis(const RE::NiPoint3& u, const RE::NiPoint3& v)
	{
		const auto n = u.Cross(v);
		RE::NiMatrix3 m;
		for (unsigned i=0; i<3; ++i) { m.entry[i][0]=u[i]; m.entry[i][1]=v[i]; m.entry[i][2]=n[i]; }
		return m;
	}
	bool Near(const RE::NiPoint3& a_left, const RE::NiPoint3& a_right)
	{
		for (unsigned i = 0; i < 3; ++i) {
			if (!SKEE::VR::CooperativeRestore::Near(a_left[i], a_right[i])) return false;
		}
		return true;
	}
	bool Near(const RE::NiMatrix3& a_left, const RE::NiMatrix3& a_right)
	{
		for (unsigned row = 0; row < 3; ++row) {
			for (unsigned column = 0; column < 3; ++column) {
				if (!SKEE::VR::CooperativeRestore::Near(
						a_left.entry[row][column], a_right.entry[row][column])) return false;
			}
		}
		return true;
	}
	bool Near(const RE::NiTransform& a_left, const RE::NiTransform& a_right)
	{
		return Near(a_left.rotate, a_right.rotate) &&
			Near(a_left.translate, a_right.translate) &&
			SKEE::VR::CooperativeRestore::Near(a_left.scale, a_right.scale);
	}
	bool CapturePolar(RE::NiNode* node, RE::BSTriShape* quad, RE::NiAVObject* hmd)
	{
		auto* data = quad->GetGeometryRuntimeData().rendererData;
		if (!data || !data->rawVertexData || quad->GetTrishapeRuntimeData().vertexCount != 4) return false;
		auto desc=data->vertexDesc;
		const auto stride=desc.GetSize(), p=desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_POSITION), t=desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0);
		if (!desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX) || !desc.HasFlag(RE::BSGraphics::Vertex::VF_UV) || stride>128 || p+12>stride || t+4>stride) return false;
		std::array<SKEE::MenuAppearance::SurfaceVertex,4> vertices{};
		for (unsigned i=0;i<4;++i) {
			float xyz[3]; std::uint16_t uv[2];
			std::memcpy(xyz,data->rawVertexData+i*stride+p,sizeof(xyz)); std::memcpy(uv,data->rawVertexData+i*stride+t,sizeof(uv));
			vertices[i]={xyz[0],xyz[1],xyz[2],DirectX::PackedVector::XMConvertHalfToFloat(uv[0]),DirectX::PackedVector::XMConvertHalfToFloat(uv[1])};
		}
		if (!SKEE::MenuAppearance::SurfaceAspect(vertices)) return false;
		RE::NiPoint3 u{},v{};
		for (unsigned i=1;i<4;++i) {
			const auto du=vertices[i].u-vertices[0].u,dv=vertices[i].v-vertices[0].v;
			RE::NiPoint3 d{static_cast<float>(vertices[i].x-vertices[0].x),static_cast<float>(vertices[i].y-vertices[0].y),static_cast<float>(vertices[i].z-vertices[0].z)};
			if (std::abs(dv)<1e-6 && std::abs(du)>1e-6) u=d/static_cast<float>(du);
			if (std::abs(du)<1e-6 && std::abs(dv)>1e-6) v=d/static_cast<float>(dv);
		}
		if (u.Length()<0.001F || v.Length()<0.001F) return false;
		const RE::NiPoint3 corner{static_cast<float>(vertices[0].x),static_cast<float>(vertices[0].y),static_cast<float>(vertices[0].z)};
		g_polarU=quad->world.rotate*(u*quad->world.scale); g_polarV=quad->world.rotate*(v*quad->world.scale);
		g_polarUVOrigin=quad->world.translate+quad->world.rotate*((corner-u*static_cast<float>(vertices[0].u)-v*static_cast<float>(vertices[0].v))*quad->world.scale);
		auto* player=RE::PlayerCharacter::GetSingleton(); auto* actor=player ? player->Get3D(false) : nullptr;
		auto* head=actor ? actor->GetObjectByName(RE::BSFixedString("NPC Head [Head]")) : nullptr;
		g_polarForward=(head ? head->world.translate : g_polarUVOrigin+g_polarU*0.5F+g_polarV*0.5F)-hmd->world.translate;
		g_polarForward.z=0;
		if (g_polarForward.Unitize()<0.001F) return false;
		g_polarNode={node->local,node->world}; g_polarQuad={quad->local,quad->world};
		g_polarNodeIdentity.reset(node); g_polarQuadIdentity.reset(quad);
		g_polarNodeParentIdentity.reset(node->parent); g_polarQuadParentIdentity.reset(quad->parent);
		g_polarDescendant=false;
		for (auto* a=quad->parent;a;a=a->parent) if (a==node) g_polarDescendant=true;
		g_polarCaptured=true;
		return true;
	}
	void ApplyPolarObject(RE::NiAVObject* object, PolarObject& base, const RE::NiMatrix3& rotation, const RE::NiPoint3& source, const RE::NiPoint3& target, float scale)
	{
		const auto& parent=object->parent->world;
		object->local.rotate=parent.rotate.Transpose()*rotation*base.world.rotate;
		object->local.scale=base.world.scale*scale/parent.scale;
		object->local.translate=parent.rotate.Transpose()*(target+rotation*((base.world.translate-source)*scale)-parent.translate)/parent.scale;
		RE::NiUpdateData update{0,RE::NiUpdateData::Flag::kDirty}; object->Update(update);
		base.appliedLocal = object->local;
		base.applied = true;
	}
	class PolarPlacementFunction final : public RE::GFxFunctionHandler
	{
		void Call(Params& args) override
		{
			if (args.userData) { if (args.retVal) args.retVal->SetNumber(g_polarState.load()); return; }
			if (args.argCount!=6 || !g_raceSexMenuWorldTransformEnabled) return;
			std::array<double,6> values{};
			for (unsigned i=0;i<6;++i) { if (!args.args[i].IsNumber()) return; values[i]=args.args[i].GetNumber(); if (!std::isfinite(values[i])) return; }
			if (std::abs(values[0])>85 || std::abs(values[1])>60 || values[2]<30 || values[2]>300 || values[3]<25 || values[3]>300) return;
			// Registered only on the RaceSex GFxMovieView; Params exposes its base type.
			const auto frame=static_cast<RE::GFxMovieView*>(args.movie)->GetVisibleFrameRect();
			if (!(frame.right>frame.left && frame.bottom>frame.top)) return;
			const float u=static_cast<float>((values[4]-frame.left)/(frame.right-frame.left)),v=static_cast<float>((values[5]-frame.top)/(frame.bottom-frame.top));
			if (u<0 || u>1 || v<0 || v>1) return;
			RE::GFxValue pickerVisible;
			const bool colorPicker = args.movie->GetVariable(&pickerVisible,
				"_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.colorField._visible") &&
				pickerVisible.IsBool() && pickerVisible.GetBool();
			const float height = SKEE::MenuConfiguration::PlacementHeight(colorPicker);
			const bool forceVertical = SKEE::MenuConfiguration::PlacementForceVertical();
			auto* tasks=SKSE::GetTaskInterface(); if (!tasks || g_polarQueued.exchange(true)) return;
			g_polarState.store(0);
			const auto generation=g_polarGeneration.load(); const auto identity=args.movie;
			tasks->AddTask([values,u,v,generation,identity,height,forceVertical] {
				g_polarQueued.store(false);
				auto* ui=RE::UI::GetSingleton(); auto menu=ui ? ui->GetMenu<RE::RaceSexMenu>() : RE::GPtr<RE::RaceSexMenu>{};
				if (generation!=g_polarGeneration.load() || !menu || menu->uiMovie.get()!=identity) return;
				auto* player=RE::PlayerCharacter::GetSingleton(); auto* nodes=player ? player->GetVRNodeData() : nullptr;
				auto* node=nodes ? nodes->uiNode.get() : nullptr; auto* quad=nodes ? nodes->InWorldUIQuadGeo.get() : nullptr; auto* hmd=nodes ? nodes->HmdNode.get() : nullptr;
				if (!node || !quad || !hmd || !node->parent || !quad->parent || node->parent->world.scale<=0 || quad->parent->world.scale<=0) return;
				if (!g_uiNodeYawOverridden) { SKEE::VR::ApplyRaceSexMenuWorldYaw(); if (!g_uiNodeYawOverridden) return; }
				if (g_polarCaptured && (g_polarNodeIdentity.get()!=node || g_polarQuadIdentity.get()!=quad)) { g_polarState.store(2); return; }
				if (!g_polarCaptured && !CapturePolar(node,quad,hmd)) { g_polarState.store(2); return; }
				const auto placement=SKEE::VR::MakeRaisedPolarPlacement(g_polarForward.x,g_polarForward.y,
					static_cast<float>(values[0]),static_cast<float>(values[1]),static_cast<float>(values[2]),height,forceVertical);
				const auto& frame=placement.frame;
				RE::NiPoint3 right{frame.right[0],frame.right[1],frame.right[2]},up{frame.up[0],frame.up[1],frame.up[2]};
				RE::NiPoint3 baseRight{g_polarForward.y,-g_polarForward.x,0};
				auto oldU=g_polarU,oldV=g_polarV; oldU.Unitize(); oldV.Unitize();
				if (oldU.Dot(baseRight)<0) right=-right; if (oldV.z<0) up=-up;
				const auto rotation=Basis(right,up)*Basis(oldU,oldV).Transpose();
				const auto source=g_polarUVOrigin+g_polarU*u+g_polarV*v;
				// Snapshot the viewer only on a real normal/face transition. Modal
				// placement changes must not chase physical headset movement either.
				const auto view=SKEE::FaceView::Current();
				if (g_polarAnchorView!=view) {
					g_polarViewerAnchor=hmd->world.translate;
					g_polarAnchorView=view;
				}
				const auto target=g_polarViewerAnchor+RE::NiPoint3{placement.offset[0],placement.offset[1],placement.offset[2]};
				const float scale=static_cast<float>(values[3])/100;
				ApplyPolarObject(node,g_polarNode,rotation,source,target,scale);
				if (!g_polarDescendant) ApplyPolarObject(quad,g_polarQuad,rotation,source,target,scale);
				g_polarState.store(1);
			});
			if (args.retVal) args.retVal->SetBoolean(true);
		}
	};

	// No worker, file flush,
	// additional engine hook, or retained GFx object is needed by this observer.
	using TraceClock = std::chrono::steady_clock;
	constexpr std::size_t kTraceCapacity = 256;
	std::array<nlohmann::json, kTraceCapacity> g_inputTrace;
	std::size_t g_traceCount{ 0 };
	std::uint64_t g_traceSequence{ 0 }, g_traceEdge{ 0 }, g_traceDropped{ 0 };
	bool g_traceArmed{ false };
	RE::GFxMovie* g_traceMovie{ nullptr };  // identity only, never dereferenced
	TraceClock::time_point g_traceStart, g_traceDeadline;
	std::string g_traceStopReason{ "never_armed" };
	std::mutex g_traceMutex;  // never held across event dispatch or original handlers
	std::uint64_t g_traceEpoch{ 0 };
	std::uint32_t g_traceArmThread{ 0 };
	std::uint64_t g_traceRawEdges{ 0 }, g_traceMenuEdges{ 0 };
	std::atomic<bool> g_traceRawEnabled{ false };

	bool TraceActive(RE::GFxMovie* a_movie)
	{
		if (g_traceArmed && TraceClock::now() >= g_traceDeadline) {
			g_traceArmed = false;
			g_traceRawEnabled.store(false, std::memory_order_relaxed);
			g_traceStopReason = "deadline";
		}
		return g_traceArmed && a_movie == g_traceMovie;
	}

	void AppendTrace(nlohmann::json a_row)
	{
		if (g_traceCount == kTraceCapacity) {
			++g_traceDropped;
			return;  // retain the beginning of the assay, report every dropped row
		}
		a_row["sequence"] = ++g_traceSequence;
		a_row["elapsedUs"] = std::chrono::duration_cast<std::chrono::microseconds>(TraceClock::now() - g_traceStart).count();
		a_row["latestNativeEdge"] = g_traceEdge;
		a_row["threadId"] = GetCurrentThreadId();
		a_row["sameAsArmThread"] = GetCurrentThreadId() == g_traceArmThread;
		g_inputTrace[g_traceCount++] = std::move(a_row);
	}

	bool IsCurrentRaceSexMovie(RE::GFxMovie* a_movie)
	{
		auto* ui = RE::UI::GetSingleton();
		auto menu = ui ? ui->GetMenu<RE::RaceSexMenu>() : RE::GPtr<RE::RaceSexMenu>{};
		return menu && menu->uiMovie.get() == a_movie;
	}

	void TraceButtonBoundary(const RE::ButtonEvent& a_button, const char* a_boundary, bool a_pointer = false) noexcept
	{
		if (!g_traceRawEnabled.load(std::memory_order_relaxed) || (!a_button.IsDown() && !a_button.IsUp())) return;
		try {
			std::scoped_lock guard{ g_traceMutex };
			// Raw input does not access the movie or assert that the menu consumed it.
			if (!TraceActive(g_traceMovie)) return;
			if (std::strcmp(a_boundary, "raw_device_button") == 0) ++g_traceRawEdges;
			else ++g_traceMenuEdges;
			AppendTrace({{"boundary", a_boundary}, {"device", std::to_underlying(a_button.GetDevice())},
				{"idCode", a_button.GetIDCode()}, {"userEvent", std::string{ a_button.GetUserEvent().c_str() }.substr(0, 64)},
				{"down", a_button.IsDown()}, {"up", a_button.IsUp()}, {"value", a_button.Value()},
				{"heldDuration", a_button.HeldDuration()}, {"pointerCandidate", a_pointer},
				{"armedMovieIdentity", reinterpret_cast<std::uintptr_t>(g_traceMovie)}});
		} catch (...) {
			std::scoped_lock guard{ g_traceMutex };
			g_traceArmed = false;
			g_traceRawEnabled.store(false, std::memory_order_relaxed);
			g_traceStopReason = "observer_error";
		}
	}

	class RawInputTraceSink final : public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_events, RE::BSTEventSource<RE::InputEvent*>*) override
		{
			if (g_traceRawEnabled.load(std::memory_order_relaxed)) {
				for (auto* event = a_events ? *a_events : nullptr; event; event = event->next) {
					if (auto* button = event->AsButtonEvent()) TraceButtonBoundary(*button, "raw_device_button");
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};
	// Stable lifetime even when the engine defers sink removal during notification.
	RawInputTraceSink g_rawInputTraceSink;
	RE::BSInputDeviceManager* g_rawInputTraceSource{ nullptr };
	void RemoveRawInputTraceSink()
	{
		g_traceRawEnabled.store(false, std::memory_order_relaxed);
		if (g_rawInputTraceSource) g_rawInputTraceSource->RemoveEventSink(&g_rawInputTraceSink);
		g_rawInputTraceSource = nullptr;
	}

	class InputTraceFunction final : public RE::GFxFunctionHandler
	{
	public:
		enum Action { kBegin, kRecord, kRead, kEnd };
		void Call(Params& p) override
		{
			if (p.retVal) p.retVal->SetBoolean(false);
			if (!p.thisPtr || !IsCurrentRaceSexMovie(p.movie)) return;
			const auto action = static_cast<Action>(reinterpret_cast<std::uintptr_t>(p.userData));
			try {
				std::unique_lock guard{ g_traceMutex };
				if (action == kBegin) {
					if (g_traceArmed) return;
					auto* input = RE::BSInputDeviceManager::GetSingleton();
					if (!input) return;
					for (auto& row : g_inputTrace) row = nullptr;
					g_traceCount = 0;
					g_traceSequence = g_traceEdge = g_traceDropped = 0;
					g_traceRawEdges = g_traceMenuEdges = 0;
					g_traceMovie = p.movie;
					++g_traceEpoch;
					g_traceArmThread = GetCurrentThreadId();
					g_traceStart = TraceClock::now();
					g_traceDeadline = g_traceStart + std::chrono::seconds{ 90 };
					g_traceStopReason = "armed";
					g_traceArmed = true;
					// Never take the engine event-source lock while holding our trace lock.
					guard.unlock();
					input->PrependEventSink(&g_rawInputTraceSink);
					g_rawInputTraceSource = input;
					g_traceRawEnabled.store(true, std::memory_order_relaxed);
				} else if (action == kRecord) {
					if (!TraceActive(p.movie) || p.argCount != 10 || !p.args[0].IsString() || !p.args[1].IsString()) return;
					nlohmann::json row{
						{"boundary", "movie_handler"},
						{"event", std::string{ p.args[0].GetString() }.substr(0, 64)},
						{"receiver", std::string{ p.args[1].GetString() }.substr(0, 256)} };
					constexpr const char* fields[]{ "localX", "localY", "mouseIndex", "button", "stageX", "stageY", "depth" };
					for (std::size_t i = 0; i < 7; ++i) row[fields[i]] = p.args[i + 2].IsNumber() ? nlohmann::json(p.args[i + 2].GetNumber()) : nlohmann::json(nullptr);
					row["receiverTruncated"] = std::strlen(p.args[1].GetString()) > 256;
					row["state"] = p.args[9].IsString() ? std::string{ p.args[9].GetString() }.substr(0, 64) : "";
					AppendTrace(std::move(row));
				} else if (action == kEnd) {
					if (p.movie == g_traceMovie && g_traceArmed) {
						g_traceArmed = false;
						g_traceStopReason = "disarmed";
					}
					guard.unlock();
					RemoveRawInputTraceSink();
				} else if (action == kRead) {
					TraceActive(p.movie);
					guard.unlock();
					auto* movie = static_cast<RE::GFxMovieView*>(p.movie);
					// Read-only AS2 self-inspection supplies typeof results that cannot
					// be recovered through Papyrus UI.GetString's scalar conversion.
					const auto snapshotReadStart = TraceClock::now();
					const auto snapshotReadTickMs = GetTickCount64();
					bool snapshotReady = false;
					std::string snapshotError;
					nlohmann::json targets = nlohmann::json::array();
					try {
					RE::GFxValue snapshotResult, snapshot;
					snapshotReady = movie->Invoke(
						"_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.ReadVRInputTargetSnapshot",
						&snapshotResult, nullptr, 0) && snapshotResult.IsBool() && snapshotResult.GetBool() &&
						movie->GetVariable(&snapshot, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.vrInputTargetSnapshot") && snapshot.IsArray();
					if (!snapshotReady) snapshotError = "movie_inspector_unavailable_or_invalid";
					const auto scalar = [](const RE::GFxValue& value) -> nlohmann::json {
						if (value.IsBool()) return value.GetBool();
						if (value.IsNumber()) return value.GetNumber();
						if (value.IsString()) return std::string{ value.GetString() }.substr(0, 256);
						return nullptr;
					};
					if (snapshotReady) {
						for (std::uint32_t i = 0; i < std::min(snapshot.GetArraySize(), 14u); ++i) {
							RE::GFxValue source;
							if (!snapshot.GetElement(i, &source) || !source.IsObject()) continue;
							nlohmann::json target = nlohmann::json::object();
							for (const auto* field : { "role", "present", "target", "parentTarget", "initialized", "enabled", "disabled",
								"focusEnabled", "visible", "alpha", "depth", "label", "onPressKind", "onReleaseKind",
								"handleMousePressKind", "handleMouseReleaseKind", "hitAreaKind", "hitAreaTarget", "hitTestDisable", "centerShapeHit" }) {
								RE::GFxValue value;
								target[field] = source.GetMember(field, &value) ? scalar(value) : nlohmann::json(nullptr);
							}
							for (const auto* field : { "bounds", "backgroundBounds" }) {
								RE::GFxValue bounds;
								target[field] = nullptr;
								if (source.GetMember(field, &bounds) && bounds.IsObject()) {
									target[field] = nlohmann::json::object();
									for (const auto* axis : { "xMin", "xMax", "yMin", "yMax" }) {
										RE::GFxValue value;
										target[field][axis] = bounds.GetMember(axis, &value) ? scalar(value) : nlohmann::json(nullptr);
									}
								}
							}
							targets.push_back(std::move(target));
						}
					}
					} catch (...) {
						// Failure of this optional projection must not end recording or
						// discard the independent, already captured input trace.
						snapshotReady = false;
						snapshotError = "target_inspection_error";
					}
					const auto snapshotReadDurationUs = std::chrono::duration_cast<std::chrono::microseconds>(TraceClock::now() - snapshotReadStart).count();
					float x{}, y{};
					std::uint32_t buttons{};
					movie->GetMouseState(0, &x, &y, &buttons);
					RE::GFxValue ignored;
					movie->GetVariable(&ignored, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.vrInputTraceIgnoredStateChanges");
					const auto ignoredStateChanges = ignored.IsNumber() ? nlohmann::json(ignored.GetNumber()) : nlohmann::json(nullptr);
					const auto cursorCount = movie->GetMouseCursorCount();
					// No movie code or member access occurs while copying trace state.
					guard.lock();
					nlohmann::json rows = nlohmann::json::array();
					for (std::size_t i = 0; i < g_traceCount; ++i) rows.push_back(g_inputTrace[i]);
					const auto report = nlohmann::json{
						{"contractVersion", 2}, {"armed", g_traceArmed}, {"stopReason", g_traceStopReason},
						{"epoch", g_traceEpoch}, {"pid", GetCurrentProcessId()}, {"armThreadId", g_traceArmThread},
						{"producerBuild", SKEE_NATIVE_PLUGIN_VERSION_STRING}, {"packageVersion", SKEE_VR2_PACKAGE_VERSION_STRING},
						{"coverage", {"raw_device_button", "menu_can_process_button", "native_submitted_packet", "wrapped_existing_movie_handlers", "movie_mouse_listener"}},
						{"rawEdges", g_traceRawEdges}, {"menuInputEdges", g_traceMenuEdges},
						{"rawSinkRegistered", g_rawInputTraceSource != nullptr}, {"rawRecordingEnabled", g_traceRawEnabled.load()},
						{"rawSinkOrder", "prepend requested; engine may defer registration during notification"},
						{"armedMovieIdentity", reinterpret_cast<std::uintptr_t>(g_traceMovie)}, {"readMovieMatchesArm", p.movie == g_traceMovie},
						{"bufferPolicy", "retain_first_then_drop"}, {"filteredEvent", "dispatchEvent:stateChange; aggregate is movie.vrInputTraceIgnoredStateChanges"},
						{"ignoredStateChanges", ignoredStateChanges},
						{"firstSequence", g_traceCount ? 1 : 0}, {"lastSequence", g_traceSequence}, {"maxDurationSeconds", 90},
						{"capacity", kTraceCapacity}, {"dropped", g_traceDropped}, {"nativeEdges", g_traceEdge},
						{"clock", "steady_clock_elapsed_microseconds"}, {"correlation", "latestNativeEdge is temporal context, not causal attribution"},
						{"movieMouseSnapshot", {{"index", 0}, {"x", x}, {"y", y}, {"buttons", buttons}, {"cursorCount", cursorCount}}},
						{"targetSnapshotReady", snapshotReady}, {"targetSnapshot", std::move(targets)},
						{"targetSnapshotError", snapshotError}, {"targetSnapshotReadTickMs", snapshotReadTickMs},
						{"targetSnapshotReadDurationUs", snapshotReadDurationUs},
						{"targetSnapshotQualification", "read-time clip metadata and shape membership, not native selection eligibility or event-time targeting"},
						{"rows", std::move(rows)} }.dump();
					guard.unlock();
					RE::GFxValue value;
					p.movie->CreateString(&value, report.c_str());
					p.thisPtr->SetMember("vrInputTraceJson", value);
				}
				if (p.retVal) p.retVal->SetBoolean(true);
			} catch (...) {
				RemoveRawInputTraceSink();
				std::scoped_lock guard{ g_traceMutex };
				g_traceArmed = false;
				g_traceStopReason = "observer_error";
				// Diagnostics must not throw across the engine's GFx callback ABI.
			}
		}
	};

	void TraceNativePacket(RE::GFxMovieView* a_movie, const RE::ButtonEvent& a_source,
		RE::GFxEvent::EventType a_requestedType, const RE::GFxMouseEvent* a_packet, const MouseCoords& a_coords)
	{
		try {
			std::scoped_lock guard{ g_traceMutex };
			if (!TraceActive(a_movie)) return;
			++g_traceEdge;
			nlohmann::json row{
				{"boundary", "native_submitted_packet"}, {"device", std::to_underlying(a_source.GetDevice())},
				{"idCode", a_source.GetIDCode()}, {"userEvent", std::string{ a_source.GetUserEvent().c_str() }.substr(0, 64)},
				{"requestedType", std::to_underlying(a_requestedType)}, {"nativeX", a_coords.x}, {"nativeY", a_coords.y},
				{"movieIdentity", reinterpret_cast<std::uintptr_t>(a_movie)}, {"cursorCount", a_movie->GetMouseCursorCount()},
				{"queued", a_packet != nullptr}, {"submissionAboutToOccur", a_packet != nullptr} };
			if (a_packet) {
				row["packet"] = {{"type", a_packet->type.underlying()}, {"x", a_packet->x}, {"y", a_packet->y},
					{"button", a_packet->button}, {"mouseIndex", a_packet->mouseIndex}, {"scrollDelta", a_packet->scrollDelta}};
			}
			AppendTrace(std::move(row));
		} catch (...) {
			std::scoped_lock guard{ g_traceMutex };
			g_traceArmed = false;
			g_traceStopReason = "observer_error";
		}
	}

	template <std::size_t N>
	bool HasExactBytes(std::uintptr_t a_address, const std::array<std::uint8_t, N>& a_expected)
	{
		return a_address && std::memcmp(reinterpret_cast<const void*>(a_address), a_expected.data(), N) == 0;
	}

	bool IsInsideSegment(std::uintptr_t a_address, std::size_t a_size, const REL::Segment& a_segment)
	{
		return a_address >= a_segment.address() &&
		       a_size <= a_segment.size() &&
		       a_address - a_segment.address() <= a_segment.size() - a_size;
	}

	bool ValidateRaceSexMenuVRRuntimeContract()
	{
		const auto& module = REL::Module::get();
		if (!REL::Module::IsVR() || module.version() != REL::Version{ 1, 4, 15, 0 }) {
			SKSE::log::error(
				"RaceSexMenu VR pointer input requires Skyrim VR 1.4.15.0; detected runtime={} version={}",
				static_cast<std::uint32_t>(REL::Module::GetRuntime()),
				module.version().string("."));
			return false;
		}

		const auto text = module.segment(REL::Segment::Name::textx);
		const auto data = module.segment(REL::Segment::Name::data);
		const auto queueAddress = RE::ScaleformEvent::QueueGFxMouseEvent.address();
		const auto dispatchAddress = RE::ScaleformEvent::DispatchGFxEvent.address();
		if (!IsInsideSegment(queueAddress, kQueueGFxMouseEventProlog.size(), text) ||
			!HasExactBytes(queueAddress, kQueueGFxMouseEventProlog)) {
			SKSE::log::error("RaceSexMenu VR QueueGFxMouseEvent signature mismatch at 0x{:X}", queueAddress);
			return false;
		}
		if (!IsInsideSegment(dispatchAddress, kDispatchGFxEventProlog.size(), text) ||
			!HasExactBytes(dispatchAddress, kDispatchGFxEventProlog)) {
			SKSE::log::error("RaceSexMenu VR DispatchGFxEvent signature mismatch at 0x{:X}", dispatchAddress);
			return false;
		}
		if (!IsInsideSegment(RE::ScaleformEvent::g_scaleformGFxEventData.address(), sizeof(void*), data) ||
			!IsInsideSegment(g_mouseCoords.address(), sizeof(void*), data)) {
			SKSE::log::error(
				"RaceSexMenu VR Scaleform globals are outside Skyrim's data segment: eventData=0x{:X}, mouseCoords=0x{:X}",
				RE::ScaleformEvent::g_scaleformGFxEventData.address(),
				g_mouseCoords.address());
			return false;
		}

		return true;
	}

	bool ApplyRaceSexMenuWorldTransform()
	{
		if (g_uiQuadTransformOverridden || !g_raceSexMenuWorldTransformEnabled) {
			return true;
		}

		auto* settings = RE::INISettingCollection::GetSingleton();
		auto* scaleSetting = settings ? settings->GetSetting("fVRMenuScene_UIQuadScale:VRUI") : nullptr;
		auto* rotationSetting = settings ? settings->GetSetting("fVRMenuScene_UIQuadRotation:VRUI") : nullptr;
		if (!scaleSetting || scaleSetting->GetType() != RE::Setting::Type::kFloat ||
			!rotationSetting || rotationSetting->GetType() != RE::Setting::Type::kFloat) {
			SKSE::log::error("RaceSexMenu VR transform could not access the shared UI quad scale/rotation settings");
			return false;
		}

		g_uiQuadScaleSetting = scaleSetting;
		g_uiQuadRotationSetting = rotationSetting;
		g_previousUIQuadScale = scaleSetting->GetFloat();
		g_previousUIQuadRotation = rotationSetting->GetFloat();
		scaleSetting->SetFloat(g_previousUIQuadScale * g_raceSexMenuWorldScale);
		rotationSetting->SetFloat(g_raceSexMenuWorldYaw);
		g_appliedUIQuadScale = scaleSetting->GetFloat();
		g_appliedUIQuadRotation = rotationSetting->GetFloat();
		g_uiQuadTransformOverridden = true;
		SKSE::log::info(
			"RaceSexMenu-local UI quad transform applied: scale {:.4f}->{:.4f} (x{:.3f}), rotation {:.4f}->{:.4f}",
			g_previousUIQuadScale,
			scaleSetting->GetFloat(),
			g_raceSexMenuWorldScale,
			g_previousUIQuadRotation,
			rotationSetting->GetFloat());
		return true;
	}

	bool IsVRController(RE::INPUT_DEVICE a_device)
	{
		const auto value = std::to_underlying(a_device);
		return value >= kFirstVRInputDevice && value <= kLastVRInputDevice;
	}

	bool IsMappedPointerClick(const RE::ButtonEvent& a_event)
	{
		// Accept the resolved menu action first so custom controller maps remain
		// usable, then use the physical controller fallback below.
		if (const auto* userEvents = RE::UserEvents::GetSingleton()) {
			const auto& userEvent = a_event.GetUserEvent();
			if (userEvent == userEvents->accept || userEvent == userEvents->activate || userEvent == userEvents->click) {
				return true;
			}
		}

		// Physical fallback for profiles that leave A/X unmapped. Restrict it to the
		// primary controller because the same OpenVR code represents left X.
		return a_event.GetIDCode() == static_cast<std::uint32_t>(RE::BSOpenVRControllerDevice::Keys::kXA) &&
		       RE::BSOpenVRControllerDevice::IsPrimaryController(a_event.GetDevice());
	}

	bool IsPointerClick(const RE::ButtonEvent& a_event)
	{
		if (!IsVRController(a_event.GetDevice())) {
			return false;
		}

		if (a_event.GetIDCode() == static_cast<std::uint32_t>(RE::BSOpenVRControllerDevice::Keys::kTrigger)) {
			return true;
		}

		return IsMappedPointerClick(a_event);
	}

	bool DispatchMouseButton(
		RE::GFxEvent::EventType a_type,
		const RE::ButtonEvent& a_source)
	{
		auto* ui = RE::UI::GetSingleton();
		auto menu = ui ? ui->GetMenu<RE::RaceSexMenu>() : RE::GPtr<RE::RaceSexMenu>{};
		auto* movie = menu ? menu->uiMovie.get() : nullptr;
		if (!movie) {
			SKSE::log::warn(
				"RaceSex VR pointer {} ignored: Scaleform movie is unavailable",
				a_type == RE::GFxEvent::EventType::kMouseDown ? "down" : "up");
			return false;
		}

		// RefreshPlatform can remove mouse endpoints again after the open observer
		// has run. Repair only the movie-local endpoint at the input edge; never
		// alter menu flags that Skyrim balances across open and close.
		if (movie->GetMouseCursorCount() == 0) {
			movie->SetMouseCursorCount(1);
		}

		auto* coordinates = *g_mouseCoords;
		auto* eventData = *RE::ScaleformEvent::g_scaleformGFxEventData;
		if (!coordinates || !eventData) {
			SKSE::log::warn(
				"RaceSex VR pointer {} ignored: native mouse coordinates or Scaleform event data are unavailable",
				a_type == RE::GFxEvent::EventType::kMouseDown ? "down" : "up");
			return false;
		}

		auto* mouseEvent = RE::ScaleformEvent::QueueGFxMouseEvent(
			eventData,
			a_type,
			kScaleformPrimaryButton,
			coordinates->x,
			coordinates->y,
			0.0F,
			0);
		// Copy engine-owned packet fields before dispatch. The pool owns its
		// lifetime; the observer never touches the packet after dispatch returns.
		TraceNativePacket(movie, a_source, a_type, mouseEvent, *coordinates);
		if (!mouseEvent) {
			SKSE::log::warn(
				"RaceSex VR pointer {} ignored: Skyrim's Scaleform event pool is full",
				a_type == RE::GFxEvent::EventType::kMouseDown ? "down" : "up");
			return false;
		}

		static const RE::BSFixedString raceSexMenuName{ RE::RaceSexMenu::MENU_NAME };
		RE::ScaleformEvent::DispatchGFxEvent(raceSexMenuName, mouseEvent);

		// Successful click edges are deliberately quiet.  The former per-edge
		// trace was useful while qualifying the hook, but it is high-volume test
		// instrumentation rather than operational logging.  Failure paths above
		// remain logged with the information needed to diagnose input routing.
		return true;
	}

	// The stock RaceSexMenu constructor asks for the flat movie even in Skyrim VR.
	// Its stage does not share the 2048x2048 coordinate system used by the native
	// wand raycaster, so correctly queued mouse events miss every control.  The
	// independently qualified constructor call is replaced with the purpose-built
	// VR movie. Establish offscreen-target ownership before
	// the menu enters UI::menuStack. The optional native quill claims cursor
	// ownership here, never after menu-open accounting. Both pointer modes use
	// engine coordinates supplied to DispatchMouseButton, not OCU laser state.
	bool RaceSexMenuLoadMovieHook(
		RE::BSScaleformManager* a_manager,
		RE::IMenu* a_menu,
		RE::GPtr<RE::GFxMovieView>& a_viewOut,
		const char* a_requestedMovie,
		RE::GFxMovieView::ScaleModeType a_mode,
		float a_backgroundAlpha)
	{
		if (!SKEE::RaceSexMenuSwfPatch::Prepare(a_manager)) return false;
		// This general VR setting is sampled while Skyrim constructs the projected
		// menu quad.  Override it before the RaceSex movie is loaded, then restore
		// it on RaceSexMenu's close event so subsequent menus keep their own size.
		ApplyRaceSexMenuWorldTransform();

		if (a_menu) {
			a_menu->menuFlags.set(RE::UI_MENU_FLAGS::kRendersOffscreenTargets);
            SKEE::VR::MenuOptionsPolicy::ConfigureQuill(a_menu->menuFlags,
                SKEE::MenuConfiguration::UseQuill(), RE::UI_MENU_FLAGS::kUsesCursor,
                RE::UI_MENU_FLAGS::kUpdateUsesCursor);
		}

		auto loaded = g_originalLoadMovie && g_originalLoadMovie(
			a_manager,
			a_menu,
			a_viewOut,
			kVRRaceSexMovie,
			a_mode,
			a_backgroundAlpha);
		if (loaded && !SKEE::RaceSexMenuSwfPatch::HasServedMovie()) {
			SKSE::log::error("RaceSex movie did not pass the runtime SWF adapter; refusing unverified cache/fallback");
			a_viewOut.reset();
			loaded = false;
		}

		if (loaded && a_viewOut) {
			// Keep one movie-local mouse endpoint for the engine's native VR events.
			// This endpoint alone does not show the quill; bUseQuill separately
			// selects constructor-time cursor ownership above.
			a_viewOut->SetMouseCursorCount(1);
		}

		if (!g_loggedMovieLoad) {
			SKSE::log::info(
				"RaceSex VR movie load: requested='{}', forced='{}', loaded={}, cursor={}, offscreen={}, mouseCursors={}",
				a_requestedMovie ? a_requestedMovie : "<null>",
				kVRRaceSexMovie,
				loaded,
				a_menu ? a_menu->UsesCursor() : false,
				a_menu ? a_menu->RendersOffscreenTargets() : false,
				a_viewOut ? a_viewOut->GetMouseCursorCount() : 0);
			g_loggedMovieLoad = true;
		}

		if (!loaded) {
			SKEE::VR::RestoreRaceSexMenuWorldTransform();
		}

		return loaded;
	}

	// RaceSexMenu owns a second vtable for its MenuEventHandler subobject in
	// Skyrim VR. Hooking CanProcess here keeps ownership local to RaceSexMenu:
	// the menu claims Trigger/Accept and the mouse edge is injected while that
	// menu is the active input recipient, rather than from a global pre-sink.
	bool RaceSexMenuCanProcessHook(RE::MenuEventHandler* a_handler, RE::InputEvent* a_event)
	{
		auto* button = a_event ? a_event->AsButtonEvent() : nullptr;
		if (button) TraceButtonBoundary(*button, "menu_can_process_button", IsPointerClick(*button));
		if (!button || !IsPointerClick(*button)) {
			return g_originalCanProcess ? g_originalCanProcess(a_handler, a_event) : false;
		}

		if (button->IsDown()) {
			SKEE::SculptTrace::Count(SKEE::SculptTrace::Event::PointerDown);
			DispatchMouseButton(RE::GFxEvent::EventType::kMouseDown, *button);
		} else if (button->IsUp()) {
			SKEE::SculptTrace::Count(SKEE::SculptTrace::Event::PointerUp);
			DispatchMouseButton(RE::GFxEvent::EventType::kMouseUp, *button);
		}
		return true;
	}
}

namespace SKEE::VR
{
	void RegisterRaceSexMenuInputTrace(RE::GFxMovieView* a_view, RE::GFxValue* a_root)
	{
		static RE::GPtr<PolarPlacementFunction> polar{new PolarPlacementFunction{}};
		RE::GFxValue placement; a_view->CreateFunction(&placement,polar.get()); a_root->SetMember("SetMenuPolarPlacement",placement);
		a_view->CreateFunction(&placement,polar.get(),reinterpret_cast<void*>(1)); a_root->SetMember("GetMenuPolarPlacementState",placement);
		a_root->SetMember("vrInputTraceContractVersion", RE::GFxValue{ 2 });
		static RE::GPtr<InputTraceFunction> handler{ new InputTraceFunction{} };
		constexpr const char* names[]{ "BeginVRInputTrace", "RecordVRMovieInput", "ReadVRInputTrace", "EndVRInputTrace" };
		for (std::uintptr_t i = 0; i < 4; ++i) {
			RE::GFxValue function;
			a_view->CreateFunction(&function, handler.get(), reinterpret_cast<void*>(i));
			a_root->SetMember(names[i], function);
		}
	}

	void SetRaceSexMenuWorldTransform(float a_scale, float a_yawDegrees, bool a_enabled)
	{
		g_raceSexMenuWorldTransformEnabled = a_enabled;
		g_raceSexMenuWorldScale = a_scale > 0.0F ? a_scale : 1.0F;
		g_raceSexMenuWorldYaw = a_yawDegrees;
	}

	bool ApplyRaceSexMenuWorldYaw()
	{
		if (g_uiNodeYawOverridden || g_uiQuadGeoYawOverridden || !g_raceSexMenuWorldTransformEnabled) {
			return true;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* vrNodes = player ? player->GetVRNodeData() : nullptr;
		auto* uiNode = vrNodes ? vrNodes->uiNode.get() : nullptr;
		auto* uiQuadGeo = vrNodes ? vrNodes->InWorldUIQuadGeo.get() : nullptr;
		if (!uiNode || !uiQuadGeo) {
			return false;
		}

		RE::NiMatrix3 yaw;
		constexpr float kDegreesToRadians = 0.01745329251994329577F;
		const auto yawDelta = (g_raceSexMenuWorldYaw - g_previousUIQuadRotation) * kDegreesToRadians;
		yaw.MakeZRotation(yawDelta);

		// Skyrim's wand ray terminates against the projected-UI parent while the
		// visible movie is carried by InWorldUIQuadGeo. Rotate the parent so the
		// laser plane and its 2D coordinates move with the menu. Some VR scene
		// layouts attach the geometry elsewhere; in that case rotate the visible
		// geometry too, but never double-rotate a descendant of uiNode.
		g_raceSexMenuUINode.reset(uiNode);
		g_raceSexMenuUINodeParent.reset(uiNode->parent);
		g_previousUINodeRotation = uiNode->local.rotate;
		uiNode->local.rotate = yaw * g_previousUINodeRotation;
		g_appliedUINodeRotation = uiNode->local.rotate;
		RE::NiUpdateData updateData{ 0.0F, RE::NiUpdateData::Flag::kDirty };
		uiNode->Update(updateData);
		g_uiNodeYawOverridden = true;

		bool geometryDescendsFromUINode = false;
		for (auto* ancestor = uiQuadGeo->parent; ancestor; ancestor = ancestor->parent) {
			if (ancestor == uiNode) {
				geometryDescendsFromUINode = true;
				break;
			}
		}
		if (!geometryDescendsFromUINode) {
			g_raceSexMenuUIQuadGeo.reset(uiQuadGeo);
			g_raceSexMenuUIQuadGeoParent.reset(uiQuadGeo->parent);
			g_previousUIQuadGeoRotation = uiQuadGeo->local.rotate;
			uiQuadGeo->local.rotate = yaw * g_previousUIQuadGeoRotation;
			g_appliedUIQuadGeoRotation = uiQuadGeo->local.rotate;
			uiQuadGeo->Update(updateData);
			g_uiQuadGeoYawOverridden = true;
		}
		SKSE::log::info(
			"RaceSexMenu-local projected UI yaw applied: parent='{}', geometry='{}', geometryDescendant={}, baseline={:.4f}, target={:.4f}, delta={:.4f}",
			uiNode->name.c_str(),
			uiQuadGeo->name.c_str(),
			geometryDescendsFromUINode,
			g_previousUIQuadRotation,
			g_raceSexMenuWorldYaw,
			g_raceSexMenuWorldYaw - g_previousUIQuadRotation);
		return true;
	}

	void RestoreRaceSexMenuWorldTransform()
	{
		g_polarGeneration.fetch_add(1);
		if (g_polarCaptured) {
			RE::NiUpdateData update{0,RE::NiUpdateData::Flag::kDirty};
			const auto restorePolar = [&](auto& a_identity, auto& a_parent, PolarObject& a_values, const char* a_label) {
				if (!a_identity || !a_values.applied) return;
				const bool topologyMatches = a_parent && a_identity->parent == a_parent.get();
				if (SKEE::VR::CooperativeRestore::ShouldRestore(
						a_identity->local,
						a_values.appliedLocal,
						topologyMatches,
						[](const RE::NiTransform& a_left, const RE::NiTransform& a_right) {
							return Near(a_left, a_right);
						})) {
					a_identity->local = a_values.local;
					a_identity->Update(update);
				} else {
					SKSE::log::warn("RaceSexMenu-local {} polar transform changed externally; preserving the newer value", a_label);
				}
			};
			restorePolar(g_polarNodeIdentity, g_polarNodeParentIdentity, g_polarNode, "UI node");
			if (!g_polarDescendant) {
				restorePolar(g_polarQuadIdentity, g_polarQuadParentIdentity, g_polarQuad, "UI quad");
			}
			g_polarCaptured=false;
			g_polarAnchorView=~0U;
			g_polarNodeIdentity.reset(); g_polarQuadIdentity.reset();
			g_polarNodeParentIdentity.reset(); g_polarQuadParentIdentity.reset();
			g_polarNode.applied = false; g_polarQuad.applied = false;
		}
		g_polarState.store(0);
		// Restore movie-local handlers and listeners while its owner is still
		// available. Never dereference the stored identity after menu destruction.
		auto* ui = RE::UI::GetSingleton();
		auto menu = ui ? ui->GetMenu<RE::RaceSexMenu>() : RE::GPtr<RE::RaceSexMenu>{};
		if (menu && menu->uiMovie) {
			menu->uiMovie->Invoke("_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.DisarmVRInputTrace", nullptr, nullptr, 0);
		}
		RemoveRawInputTraceSink();
		{
			std::scoped_lock guard{ g_traceMutex };
			if (g_traceMovie) {
				g_traceArmed = false;
				g_traceStopReason = "menu_close";
			}
			g_traceMovie = nullptr;
		}
		RE::NiUpdateData updateData{ 0.0F, RE::NiUpdateData::Flag::kDirty };
		if (g_uiQuadGeoYawOverridden && g_raceSexMenuUIQuadGeo) {
			const bool topologyMatches = g_raceSexMenuUIQuadGeoParent &&
				g_raceSexMenuUIQuadGeo->parent == g_raceSexMenuUIQuadGeoParent.get();
			if (SKEE::VR::CooperativeRestore::ShouldRestore(
					g_raceSexMenuUIQuadGeo->local.rotate,
					g_appliedUIQuadGeoRotation,
					topologyMatches,
					[](const RE::NiMatrix3& a_left, const RE::NiMatrix3& a_right) { return Near(a_left, a_right); })) {
				g_raceSexMenuUIQuadGeo->local.rotate = g_previousUIQuadGeoRotation;
				g_raceSexMenuUIQuadGeo->Update(updateData);
			} else {
				SKSE::log::warn("RaceSexMenu-local UI quad yaw changed externally; preserving the newer value");
			}
		}
		if (g_uiNodeYawOverridden && g_raceSexMenuUINode) {
			const bool topologyMatches = g_raceSexMenuUINodeParent &&
				g_raceSexMenuUINode->parent == g_raceSexMenuUINodeParent.get();
			if (SKEE::VR::CooperativeRestore::ShouldRestore(
					g_raceSexMenuUINode->local.rotate,
					g_appliedUINodeRotation,
					topologyMatches,
					[](const RE::NiMatrix3& a_left, const RE::NiMatrix3& a_right) { return Near(a_left, a_right); })) {
				g_raceSexMenuUINode->local.rotate = g_previousUINodeRotation;
				g_raceSexMenuUINode->Update(updateData);
				SKSE::log::info("RaceSexMenu-local projected UI yaw restored");
			} else {
				SKSE::log::warn("RaceSexMenu-local UI node yaw changed externally; preserving the newer value");
			}
		}
		g_raceSexMenuUINode.reset();
		g_raceSexMenuUIQuadGeo.reset();
		g_raceSexMenuUINodeParent.reset();
		g_raceSexMenuUIQuadGeoParent.reset();
		g_uiNodeYawOverridden = false;
		g_uiQuadGeoYawOverridden = false;

		if (g_uiQuadTransformOverridden && g_uiQuadScaleSetting && g_uiQuadRotationSetting) {
			auto* settings = RE::INISettingCollection::GetSingleton();
			auto* currentScaleSetting = settings ? settings->GetSetting("fVRMenuScene_UIQuadScale:VRUI") : nullptr;
			auto* currentRotationSetting = settings ? settings->GetSetting("fVRMenuScene_UIQuadRotation:VRUI") : nullptr;
			const auto overriddenScale = g_uiQuadScaleSetting->GetFloat();
			const auto overriddenRotation = g_uiQuadRotationSetting->GetFloat();
			const bool restoreScale = SKEE::VR::CooperativeRestore::ShouldRestore(
				overriddenScale,
				g_appliedUIQuadScale,
				currentScaleSetting == g_uiQuadScaleSetting,
				[](float a_left, float a_right) { return SKEE::VR::CooperativeRestore::Near(a_left, a_right); });
			const bool restoreRotation = SKEE::VR::CooperativeRestore::ShouldRestore(
				overriddenRotation,
				g_appliedUIQuadRotation,
				currentRotationSetting == g_uiQuadRotationSetting,
				[](float a_left, float a_right) { return SKEE::VR::CooperativeRestore::Near(a_left, a_right); });
			if (restoreScale) {
				g_uiQuadScaleSetting->SetFloat(g_previousUIQuadScale);
			} else {
				SKSE::log::warn("RaceSexMenu-local UI scale changed externally; preserving the newer value {:.4f}", overriddenScale);
			}
			if (restoreRotation) {
				g_uiQuadRotationSetting->SetFloat(g_previousUIQuadRotation);
			} else {
				SKSE::log::warn("RaceSexMenu-local UI rotation changed externally; preserving the newer value {:.4f}", overriddenRotation);
			}
			SKSE::log::info(
				"RaceSexMenu-local UI quad cleanup: scaleRestored={}, rotationRestored={}",
				restoreScale,
				restoreRotation);
		}
		g_uiQuadScaleSetting = nullptr;
		g_uiQuadRotationSetting = nullptr;
		g_uiQuadTransformOverridden = false;
	}

	bool RegisterRaceSexMenuPointerInput()
	{
		if (g_installed) {
			return true;
		}
		if (!ValidateRaceSexMenuVRRuntimeContract()) {
			return false;
		}

		REL::Relocation<std::uintptr_t> handlerVtable{ RE::VTABLE_RaceSexMenu[1] };
		if (!handlerVtable.address()) {
			return false;
		}

		const auto handlerEntry = handlerVtable.address() + sizeof(std::uintptr_t);
		REL::Relocation<std::uintptr_t> loadMovieCall{ REL::Offset(kRaceSexLoadMovieCallRva) };
		const auto loadMovieCallAddress = loadMovieCall.address();
		const auto rdata = REL::Module::get().segment(REL::Segment::Name::rdata);
		const auto text = REL::Module::get().segment(REL::Segment::Name::textx);
		if (!IsInsideSegment(handlerEntry, sizeof(std::uintptr_t), rdata)) {
			SKSE::log::error(
				"RaceSexMenu VR MenuEventHandler vtable entry is outside Skyrim's rdata segment: 0x{:X}",
				handlerEntry);
			return false;
		}
		if (!IsInsideSegment(loadMovieCallAddress, 5, text) ||
			*reinterpret_cast<const std::uint8_t*>(loadMovieCallAddress) != 0xE8) {
			SKSE::log::error(
				"RaceSexMenu VR LoadMovie call site is not the expected direct call: 0x{:X}",
				loadMovieCallAddress);
			return false;
		}

		std::uintptr_t originalAddress{};
		std::memcpy(std::addressof(originalAddress), reinterpret_cast<const void*>(handlerEntry), sizeof(originalAddress));
		if (originalAddress < text.address() || originalAddress >= text.address() + text.size()) {
			SKSE::log::error(
				"RaceSexMenu VR MenuEventHandler::CanProcess target is not an unmodified Skyrim function: 0x{:X}",
				originalAddress);
			return false;
		}
		SKEE::VR::HookTransaction::RelativeCall qualifiedLoadMovieCall{};
		std::memcpy(
			qualifiedLoadMovieCall.data(),
			reinterpret_cast<const void*>(loadMovieCallAddress),
			qualifiedLoadMovieCall.size());
		std::uintptr_t originalLoadMovieAddress{};
		if (!SKEE::VR::HookTransaction::DecodeRelativeCall(
				loadMovieCallAddress, qualifiedLoadMovieCall, originalLoadMovieAddress)) {
			SKSE::log::error("RaceSexMenu VR LoadMovie call could not be decoded during qualification");
			return false;
		}
		if (originalLoadMovieAddress < text.address() || originalLoadMovieAddress >= text.address() + text.size()) {
			SKSE::log::error(
				"RaceSexMenu VR LoadMovie target is outside Skyrim's text segment: 0x{:X}",
				originalLoadMovieAddress);
			return false;
		}

		if (const auto* trampolineInterface = SKSE::GetTrampolineInterface()) {
			auto* branch = trampolineInterface->AllocateFromBranchPool(64);
			if (!branch) {
				SKSE::log::error("RaceSexMenu VR could not allocate its LoadMovie branch trampoline");
				return false;
			}
			g_loadMovieTrampoline.set_trampoline(branch, 64);
		} else {
			g_loadMovieTrampoline.create(64);
		}
		#pragma pack(push, 1)
		struct AbsoluteJump
		{
			std::uint8_t opcode{ 0xFF };
			std::uint8_t modrm{ 0x25 };
			std::int32_t displacement{ 0 };
			std::uint64_t address{};
		};
		#pragma pack(pop)
		static_assert(sizeof(AbsoluteJump) == 14);
		auto* loadMovieStub = g_loadMovieTrampoline.allocate<AbsoluteJump>();
		if (!loadMovieStub) {
			SKSE::log::error("RaceSexMenu VR could not reserve its LoadMovie absolute-jump stub");
			return false;
		}
		*loadMovieStub = AbsoluteJump{
			.opcode = 0xFF,
			.modrm = 0x25,
			.displacement = 0,
			.address = reinterpret_cast<std::uint64_t>(RaceSexMenuLoadMovieHook)
		};
		SKEE::VR::HookTransaction::RelativeCall hookedLoadMovieCall{};
		if (!SKEE::VR::HookTransaction::EncodeRelativeCall(
				loadMovieCallAddress,
				reinterpret_cast<std::uintptr_t>(loadMovieStub),
				hookedLoadMovieCall)) {
			SKSE::log::error("RaceSexMenu VR LoadMovie branch stub is outside rel32 range");
			return false;
		}

		// Both original targets and every helper endpoint have been qualified before
		// either game-memory write.  Publish the originals first so neither hook can
		// observe a transient null if input arrives immediately after the first write.
		g_originalLoadMovie = reinterpret_cast<RaceSexMenuLoadMovie_t>(originalLoadMovieAddress);
		g_originalCanProcess = reinterpret_cast<RaceSexMenuCanProcess_t>(originalAddress);
		const auto canProcessHookAddress = reinterpret_cast<std::uintptr_t>(RaceSexMenuCanProcessHook);
		const auto transaction = SKEE::VR::HookTransaction::Commit(
			[&] {
				return REL::safe_write(
					handlerEntry,
					std::addressof(canProcessHookAddress),
					sizeof(canProcessHookAddress),
					std::addressof(originalAddress),
					sizeof(originalAddress));
			},
			[&] {
				return REL::safe_write(
					loadMovieCallAddress,
					hookedLoadMovieCall.data(),
					hookedLoadMovieCall.size(),
					qualifiedLoadMovieCall.data(),
					qualifiedLoadMovieCall.size());
			},
			[&] {
				return REL::safe_write(
					handlerEntry,
					std::addressof(originalAddress),
					sizeof(originalAddress),
					std::addressof(canProcessHookAddress),
					sizeof(canProcessHookAddress));
			});
		g_installed = transaction == SKEE::VR::HookTransaction::Result::kInstalled;
		if (g_installed) {
			SKSE::log::info(
				"RaceSexMenu VR hooks installed transactionally: LoadMovie call=0x{:X} original=0x{:X}; MenuEventHandler vtable=0x{:X} original=0x{:X}",
				loadMovieCallAddress,
				reinterpret_cast<std::uintptr_t>(g_originalLoadMovie),
				handlerVtable.address(),
				reinterpret_cast<std::uintptr_t>(g_originalCanProcess));
		} else {
			if (transaction == SKEE::VR::HookTransaction::Result::kFirstSiteRejected) {
				SKSE::log::error("RaceSexMenu VR MenuEventHandler slot changed during qualification; no pointer hook was installed");
			} else if (transaction == SKEE::VR::HookTransaction::Result::kSecondSiteRejectedRolledBack) {
				SKSE::log::error("RaceSexMenu VR LoadMovie call changed during qualification; the MenuEventHandler hook was rolled back");
			} else {
				SKSE::log::critical("RaceSexMenu VR LoadMovie call changed and the MenuEventHandler rollback was rejected because that slot also changed");
			}
			g_originalLoadMovie = nullptr;
			g_originalCanProcess = nullptr;
		}
		return g_installed;
	}
}

#else

namespace SKEE::VR
{
	void RegisterRaceSexMenuInputTrace(RE::GFxMovieView*, RE::GFxValue*) {}
	void SetRaceSexMenuWorldTransform(float, float, bool) {}
	bool ApplyRaceSexMenuWorldYaw() { return false; }
	void RestoreRaceSexMenuWorldTransform() {}

	bool RegisterRaceSexMenuPointerInput()
	{
		return false;
	}
}

#endif
