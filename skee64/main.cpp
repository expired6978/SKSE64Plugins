#include "SKSE/API.h"
#include <REX/W32/KERNEL32.h>
#include "SKSE/Interfaces.h"
#include "SKSE/Trampoline.h"
#include "SKSE/Version.h"
#include "REL/Relocation.h"
#include "RE/RTTI.h"
#include "RE/B/BSFixedString.h"
#include "RE/T/TESForm.h"
#include "RE/T/TESObjectREFR.h"
#include "RE/T/TESNPC.h"
#include "RE/T/TESDataHandler.h"
#include "RE/T/TESRace.h"
#include "RE/A/Actor.h"
#include "RE/N/NiNode.h"
#include "RE/N/NiAVObject.h"
#include "RE/B/BSFaceGenNiNode.h"
#include "RE/B/BSGeometry.h"
#include "RE/B/BSShaderProperty.h"
#include "RE/B/BSLightingShaderProperty.h"
#include "RE/B/BSLightingShaderMaterial.h"
#include "RE/B/BSLightingShaderMaterialFacegenTint.h"
#include "RE/B/BSLightingShaderMaterialHairTint.h"
#include "RE/E/ExtraDataList.h"
#include "RE/E/ExtraRank.h"
#include "RE/E/ExtraUniqueID.h"
#include "RE/E/ExtraContainerChanges.h"
#include "RE/S/ScriptEventSourceHolder.h"
#include "RE/T/TESCellFullyLoadedEvent.h"
#include "RE/T/TESObjectLoadedEvent.h"
#include "RE/T/TESInitScriptEvent.h"
#include "RE/T/TESLoadGameEvent.h"
#include "RE/T/TESUniqueIDChangeEvent.h"
#include "RE/I/INISettingCollection.h"
#include "RE/I/IVirtualMachine.h"
#include "RE/V/Variable.h"
#include "RE/N/NativeFunction.h"
#include "RE/G/GFxMovieView.h"
#include "RE/G/GFxValue.h"
#include "RE/G/GFxFunctionHandler.h"
#include "RE/M/MenuOpenCloseEvent.h"
#include "RE/R/RaceSexMenu.h"
#include "RE/U/UI.h"
#include "SKSE/Events.h"
#include "PluginInterface.h"
#include "OverrideInterface.h"
#include "OverlayInterface.h"
#include "BodyMorphInterface.h"
#include "CharacterCreationInterface.h"
#include "VRNewGameIntent.h"
#include "ItemDataInterface.h"
#include "TintMaskInterface.h"
#include "NiTransformInterface.h"
#include "SkinLayerInterface.h"
#include "PresetInterface.h"
#include "RaceSexMenuVRInput.h"
#include "RaceSexMenuVRKeyboard.h"
#include "MenuAppearance.h"
#include "MenuConfiguration.h"
#include "MenuExtensions.h"
#include "RaceSexMenuFaceView.h"
#include "AvatarLighting.h"
#include "SkeletonExtender.h"
#include "AttachmentInterface.h"
#include "ActorUpdateManager.h"
#include "ActorArmorTangentUpdater.h"
#include "CommandInterface.h"
#include "FormTagInterface.h"

#include "FaceMorphInterface.h"
#include "PartHandler.h"

#include "ShaderUtilities.h"
#include "ScaleformFunctions.h"
#include "ScaleformCharGenFunctions.h"
#include "ScaleformUtils.h"
#include "StringTable.h"

#include <string>
#include <chrono>
#include <cstdint>
#include <cassert>
#include <mutex>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <functional>

#include "PapyrusNiOverride.h"
#include "PapyrusCharGen.h"
#include "SKEEHooks.h"
#include "BuildVersion.h"

#include <spdlog/sinks/basic_file_sink.h>

namespace
{
	void InitializeVR2Logging()
	{
#if defined(ENABLE_SKYRIM_VR)
		try {
			auto logDirectory = SKSE::log::log_directory();
			if (!logDirectory) {
				return;
			}

			auto logPath = *logDirectory / "RaceMenuNGVR2.log";
			auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
			auto logger = std::make_shared<spdlog::logger>("RaceMenuNGVR2", std::move(fileSink));
			spdlog::set_default_logger(std::move(logger));
			spdlog::set_pattern("[%Y-%m-%d %T.%e] [%l] %v");
			spdlog::set_level(spdlog::level::info);
			spdlog::flush_on(spdlog::level::info);
		} catch (...) {
			// Diagnostics must never prevent the plugin from loading.
		}
#endif
	}

#if defined(ENABLE_SKYRIM_VR)
	constexpr std::uint32_t kRaceSexCursorConfigureAttempts = 8;

	bool g_enableRaceSexMouseCursor = true;
	bool g_applyRaceSexMenuPlacement = true;
	float g_raceSexMenuOffsetX = 40.0F;
	float g_raceSexMenuOffsetY = -40.0F;
	float g_raceSexMenuOffsetZ = 100.0F;
	// RaceSex offsets translate one shared projected UI quad. Rotate that quad
	// itself so the left-offset panel faces the viewer.
	float g_raceSexMenuWorldYaw = 158.2F;
	float g_raceSexMenuWorldScale = 1.5F;
	std::uint32_t g_raceSexMenuCategoryMask = 0xFFFFFFFF;

	bool ApplyRaceSexMenuPlacement()
	{
		auto* settings = RE::INISettingCollection::GetSingleton();
		if (!settings) {
			SKSE::log::error("RaceSexMenu VR placement could not access Skyrim's INI setting collection");
			return false;
		}

		struct PlacementSetting
		{
			const char* name;
			float value;
		};

		const PlacementSetting placementSettings[] = {
			{ "fRaceSexMenuOffsetX:VRUI", g_raceSexMenuOffsetX },
			{ "fRaceSexMenuOffsetY:VRUI", g_raceSexMenuOffsetY },
			{ "fRaceSexMenuOffsetZ:VRUI", g_raceSexMenuOffsetZ }
		};

		bool appliedAll = true;
		for (const auto& requested : placementSettings) {
			auto* setting = settings->GetSetting(requested.name);
			if (!setting || setting->GetType() != RE::Setting::Type::kFloat) {
				SKSE::log::error("RaceSexMenu VR placement setting '{}' is unavailable or not a float", requested.name);
				appliedAll = false;
				continue;
			}

			const auto previous = setting->GetFloat();
			setting->SetFloat(requested.value);
			SKSE::log::info(
				"RaceSexMenu VR placement setting '{}' applied: {:.4f}->{:.4f}",
				requested.name,
				previous,
				setting->GetFloat());
		}

		return appliedAll;
	}

	void QueueRaceSexMouseCursorConfiguration(std::uint32_t a_attempt = 1);

	void ConfigureRaceSexMouseCursor(std::uint32_t a_attempt)
	{
		auto* ui = RE::UI::GetSingleton();
		auto menu = ui ? ui->GetMenu<RE::RaceSexMenu>() : RE::GPtr<RE::RaceSexMenu>{};
		if (!menu || !menu->uiMovie) {
			if (a_attempt < kRaceSexCursorConfigureAttempts) {
				QueueRaceSexMouseCursorConfiguration(a_attempt + 1);
			} else {
				SKSE::log::error("RaceSex mouse compatibility could not find the live movie after {} attempts", a_attempt);
			}
			return;
		}

		auto* movie = menu->uiMovie.get();
		const auto yawApplied = SKEE::VR::ApplyRaceSexMenuWorldYaw();
		const auto cursorCountBefore = movie->GetMouseCursorCount();
		const auto controllerCount = movie->GetControllerCount();
		const auto usedCursorBefore = menu->UsesCursor();
		const auto updatedCursorBefore = menu->UpdateUsesCursor();

		// RaceSexMenu normally asks RefreshPlatform() to remove mouse input while a
		// gamepad is active.  Motion controllers keep that path permanently active
		// in Skyrim VR, leaving the Scaleform movie with zero mouse endpoints even
		// though OCU can map its laser onto the movie correctly.  Add the endpoint
		// only to this movie.  UI_MENU_FLAGS participate in balanced open/close
		// bookkeeping, so mutating them after the menu has opened can leave Skyrim's
		// shared VR UI root in RaceSexMenu's world pose for later menus.
		movie->SetMouseCursorCount(1);

		const auto cursorCountAfter = movie->GetMouseCursorCount();
		SKSE::log::info(
			"RaceSex mouse compatibility attempt {}: cursor {}->{}, controllers {}, UsesCursor {} (unchanged), UpdateUsesCursor {} (unchanged), scene yaw {}",
			a_attempt,
			cursorCountBefore,
			cursorCountAfter,
			controllerCount,
			usedCursorBefore,
			updatedCursorBefore,
			yawApplied ? "applied" : "pending");

		if ((cursorCountAfter == 0 || !yawApplied) && a_attempt < kRaceSexCursorConfigureAttempts) {
			QueueRaceSexMouseCursorConfiguration(a_attempt + 1);
		}
	}

	void QueueRaceSexMouseCursorConfiguration(std::uint32_t a_attempt)
	{
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([a_attempt] { ConfigureRaceSexMouseCursor(a_attempt); });
		}
	}

	class RaceSexMouseCursorSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::MenuOpenCloseEvent* a_event,
			RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			const auto* menuName = a_event ? a_event->menuName.c_str() : nullptr;
			if (a_event && menuName && std::string_view{ menuName } == RE::RaceSexMenu::MENU_NAME) {
				if (a_event->opening) {
					if (g_enableRaceSexMouseCursor) QueueRaceSexMouseCursorConfiguration();
				} else {
					SKEE::VR::CancelRaceSexMenuKeyboard();
					SKEE::VR::RestoreRaceSexMenuWorldTransform();
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	RaceSexMouseCursorSink g_raceSexMouseCursorSink;
#endif
}


// Plugin handle (legacy compatibility)
std::uint32_t g_pluginHandle = static_cast<std::uint32_t>(-1);

// Versions of the actually-running SKSE/game, captured from the LoadInterface
// in SKSE_PLUGIN_LOAD. Written into preset headers instead of compile-time constants.
std::uint32_t g_skseVersion = 0;     // LoadInterface::skseVersion (packed)
std::uint32_t g_runtimeVersion = 0;  // LoadInterface::runtimeVersion (packed)

// Interfaces - accessed via SKSE getters, no globals needed
// SKSE::GetSerializationInterface()
// SKSE::GetScaleformInterface()
// SKSE::GetTaskInterface()

const SKSE::TaskInterface* g_task = nullptr;
// SKSE::GetMessagingInterface()
// SKSE::GetPapyrusInterface()
// SKSE::GetTrampoline()
// SKSE::GetObjectInterface()

// Handlers
InterfaceMap				g_interfaceMap;
DyeMap						g_dyeMap;
OverrideInterface			g_overrideInterface;
TintMaskInterface			g_tintMaskInterface;
OverlayInterface			g_overlayInterface;
BodyMorphInterface			g_bodyMorphInterface;
CharacterCreationInterface	g_characterCreationInterface;
ItemDataInterface			g_itemDataInterface;
NiTransformInterface		g_transformInterface;
FaceMorphInterface			g_morphInterface;
SkeletonExtenderInterface	g_skeletonExtenderInterface;
ActorUpdateManager			g_actorUpdateManager;
ActorArmorTangentUpdater	g_actorArmorTangentUpdater;
AttachmentInterface			g_attachmentInterface;
CommandInterface			g_commandInterface;
PresetInterface				g_presetInterface;
FormTagInterface			g_formTagInterface;

PartSet	g_partSet;

StringTable g_stringTable;

// Feature Toggles
bool	g_enableOverlays = true;
bool	g_enableSculpting = true;
bool	g_enableBodyGen = true;
bool	g_enableAutoTransforms = true;
bool	g_enableHeadExport = true;
bool	g_enableBodyMorph = true;
bool	g_enableTintSync = true;
bool	g_enableTintInventory = true;
bool	g_enableTintHairSlot = true;
bool	g_enableTangentSpaceCorrection = true;
bool	g_enableFaceNormalRecalculate = true;
bool	g_enableBodyNormalRecalculate = true;

bool	g_hookBipedAttach = true;
bool	g_hookNativeSliders = true;
bool	g_hookSliderCallbacks = true;
bool	g_hookHeadPreprocessing = true;
bool	g_hookMorphUpdates = true;
bool	g_hookMorphExtensions = true;
bool	g_hookTintInventory = true;
bool	g_hookTinting = true;
bool	g_hookFaceOverlays = true;

bool	g_suppressPatternWarnings = false;

bool	g_enableEarlyRegistration = false;

bool	g_playerOnly = true;
std::uint32_t	g_numBodyOverlays = 3;
std::uint32_t	g_numHandOverlays = 3;
std::uint32_t	g_numFeetOverlays = 3;
std::uint32_t	g_numFaceOverlays = 3;
std::uint32_t	g_numSpellBodyOverlays = 1;
std::uint32_t	g_numSpellHandOverlays = 1;
std::uint32_t	g_numSpellFeetOverlays = 1;
std::uint32_t	g_numSpellFaceOverlays = 1;
std::uint32_t	g_tintHairSlot = 1;

bool	g_overlayAlphaOverride = true;
std::uint16_t	g_overlayAlphaFlags = 4845;
std::uint16_t	g_overlayAlphaThreshold = 0;
bool	g_overlayForceDecal = true;

bool	g_enableBodyInit = true;
bool	g_firstLoad = false;
bool	g_immediateArmor = true;
bool	g_enableFaceOverlays = true;
bool	g_immediateFace = false;
bool	g_enableEquippableTransforms = true;
bool	g_parallelMorphing = true;
bool	g_deferredBodyMorph = false;
std::uint16_t	g_scaleMode = 0;
std::uint16_t	g_bodyMorphMode = 0;
bool	g_bodyMorphGPUCopy = true;
bool	g_bodyMorphRebind = true;

bool	g_externalHeads = false;
bool	g_extendedMorphs = true;
bool	g_allowAllMorphs = true;
bool	g_allowAnyRacePart = false;
bool	g_allowAnyGenderPart = false;
bool	g_disableFaceGenCache = true;
bool	g_exportSkinToBone = true;
float	g_sliderMultiplier = 1.0f;
float	g_sliderInterval = 0.01f;
std::uint32_t	g_numPresets = 10;
std::uint32_t	g_customDataMax = 10;
std::string g_raceTemplate = "NordRace";

// Compact DirectX vars
#include "CDXCamera.h"
#include "CDXNifScene.h"
#include "CDXBrush.h"


CDXD3DDevice*		g_Device = nullptr;
CDXModelViewerCamera	g_Camera;
CDXNifScene				g_World;

float		g_panSpeed = 0.01f;
float		g_cameraFOV = 45.0f;
std::int32_t		g_viewWidth = 1024;
std::int32_t		g_viewHeight = 1024;
float		g_sculptBackgroundA = 0.0f;
float		g_sculptBackgroundR = 0.0f;
float		g_sculptBackgroundG = 0.0f;
float		g_sculptBackgroundB = 0.0f;
float		g_sculptOffsetX = 0.0f;
float		g_sculptOffsetY = 0.0f;
float		g_sculptOffsetZ = 0.0f;
bool		g_sculptDrawBrushPoint = true;
bool		g_sculptDrawBrushRadius = true;

extern double g_brushProperties[CDXBrush::kBrushTypes][CDXBrush::kBrushProperties][CDXBrush::kBrushPropertyValues];

#define MIN_SERIALIZATION_VERSION	1
#define MIN_TASK_VERSION			2
#define MIN_SCALEFORM_VERSION		1
#define MIN_PAPYRUS_VERSION			1

const std::string & F4EEGetRuntimeDirectory(void)
{
	static std::string s_runtimeDirectory;

	if (s_runtimeDirectory.empty())
	{
		// can't determine how many bytes we'll need, hope it's not more than REX::W32::MAX_PATH
		char	runtimePathBuf[REX::W32::MAX_PATH];
		std::uint32_t	runtimePathLength = REX::W32::GetModuleFileNameA(REX::W32::GetModuleHandleA(nullptr), runtimePathBuf, sizeof(runtimePathBuf));

		if (runtimePathLength && (runtimePathLength < sizeof(runtimePathBuf)))
		{
			std::string	runtimePath(runtimePathBuf, runtimePathLength);

			// truncate at last slash
			std::string::size_type	lastSlash = runtimePath.rfind('\\');
			if (lastSlash != std::string::npos)	// if we don't find a slash something is VERY WRONG
			{
				s_runtimeDirectory = runtimePath.substr(0, lastSlash + 1);

				SKSE::log::debug("runtime root = {}", s_runtimeDirectory.c_str());
			}
			else
			{
				SKSE::log::warn("no slash in runtime path? ({})", runtimePath.c_str());
			}
		}
		else
		{
			SKSE::log::warn("couldn't find runtime path (len = {}, err = {:08X})", runtimePathLength, GetLastError());
		}
	}

	return s_runtimeDirectory;
}

const std::string & SKEE64GetConfigPath(bool custom = false)
{
	static std::string s_configPath;
	static std::string s_configPathCustom;

	if (s_configPath.empty())
	{
		std::string	runtimePath = F4EEGetRuntimeDirectory();
		if (!runtimePath.empty())
		{
			s_configPath = runtimePath + "Data\\SKSE\\Plugins\\skee64.ini";

			SKSE::log::info("default config path = {}", s_configPath.c_str());
		}
	}
	if (s_configPathCustom.empty())
	{
		std::string	runtimePath = F4EEGetRuntimeDirectory();
		if (!runtimePath.empty())
		{
			s_configPathCustom = runtimePath + "Data\\SKSE\\Plugins\\skee64_custom.ini";

			SKSE::log::info("custom config path = {}", s_configPathCustom.c_str());
		}
	}

	return custom ? s_configPathCustom : s_configPath;
}

std::string SKEE64GetConfigOption(const char * section, const char * key)
{
	std::string	result;

	const std::string & configPath = SKEE64GetConfigPath();
	const std::string & configPathCustom = SKEE64GetConfigPath(true);

	// Reject an overlength player name instead of accepting a truncated name.
	char	resultBuf[4096];
	const auto bufferSize = std::strcmp(section, "VR") == 0 && std::strcmp(key, "sPlayerName") == 0 ?
		static_cast<std::uint32_t>(sizeof(resultBuf)) : 256U;
	resultBuf[0] = 0;

	if (!configPath.empty())
	{
		std::uint32_t	resultLen = REX::W32::GetPrivateProfileStringA(section, key, NULL, resultBuf, bufferSize, configPath.c_str());
		result = resultBuf;
	}
	if (!configPathCustom.empty())
	{
		std::uint32_t	resultLen = REX::W32::GetPrivateProfileStringA(section, key, NULL, resultBuf, bufferSize, configPathCustom.c_str());
		if (resultLen > 0) // Only take custom if we have it
			result = resultBuf;
	}

	return result;
}

template<typename T>
const char * SKEE64GetTypeFormatting(T * dataOut)
{
	return false;
}

template<> const char * SKEE64GetTypeFormatting(double * dataOut) { return "%lf"; }
template<> const char * SKEE64GetTypeFormatting(float * dataOut) { return "%f"; }
template<> const char * SKEE64GetTypeFormatting(bool * dataOut) { return "%c"; }
template<> const char * SKEE64GetTypeFormatting(std::int16_t * dataOut) { return "%hd"; }
template<> const char * SKEE64GetTypeFormatting(std::uint16_t * dataOut) { return "%hu"; }
template<> const char * SKEE64GetTypeFormatting(std::int32_t * dataOut) { return "%d"; }
template<> const char * SKEE64GetTypeFormatting(std::uint32_t * dataOut) { return "%u"; }
template<> const char * SKEE64GetTypeFormatting(std::uint64_t * dataOut) { return "%I64u"; }

template<typename T>
bool SKEE64GetConfigValue(const char * section, const char * key, T * dataOut)
{
	std::string	data = SKEE64GetConfigOption(section, key);
	if (data.empty())
		return false;

	T tmp;
	bool res = (sscanf_s(data.c_str(), SKEE64GetTypeFormatting(dataOut), &tmp) == 1);
	if (res) {
		*dataOut = tmp;
	}
	return res;
}

template<>
bool SKEE64GetConfigValue(const char * section, const char * key, bool * dataOut)
{
	std::string	data = SKEE64GetConfigOption(section, key);
	if (data.empty())
		return false;

	std::uint32_t tmp;
	bool res = (sscanf_s(data.c_str(), SKEE64GetTypeFormatting(&tmp), &tmp) == 1);
	if (res) {
		*dataOut = (tmp > 0);
	}
	return res;
}

void SKEE64Serialization_Revert(SKSE::SerializationInterface* a_intfc)
{
	SKSE::log::info("Reverting...");

	g_actorUpdateManager.Revert();
	g_overlayInterface.Revert();
	g_overrideInterface.Revert();
	g_bodyMorphInterface.Revert();
	g_itemDataInterface.Revert();
	g_dyeMap.Revert();
	g_transformInterface.Revert();
	g_morphInterface.Revert();
	g_attachmentInterface.Revert();
	g_characterCreationInterface.Revert();
	g_stringTable.Revert();
}

class StopWatch
{
public:
	StopWatch()
	{
		
	}

	void Start()
	{
		start = std::chrono::system_clock::now();
	}

	long long Stop()
	{
		end = std::chrono::system_clock::now();
		return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	}

private:
	std::chrono::system_clock::time_point start;
	std::chrono::system_clock::time_point end;
};

void SKEE64Serialization_Save(SKSE::SerializationInterface* a_intfc)
{
	SKSE::log::info("Saving...");

	StopWatch sw;

	sw.Start();
	g_stringTable.Save(a_intfc, StringTable::kSerializationVersion);
	SKSE::log::debug("{} - Serialized string table {}ms", __FUNCTION__, sw.Stop());

	sw.Start();
	g_morphInterface.Save(a_intfc, FaceMorphInterface::kSerializationVersion);
	SKSE::log::debug("{} - Player morph data {}ms", __FUNCTION__, sw.Stop());

	sw.Start();
	g_transformInterface.Save(a_intfc, NiTransformInterface::kSerializationVersion);
	SKSE::log::debug("{} - Serialized transforms {}ms", __FUNCTION__, sw.Stop());

	sw.Start();
	g_overlayInterface.Save(a_intfc, OverlayInterface::kSerializationVersion);
	SKSE::log::debug("{} - Serialized overlays {}ms", __FUNCTION__, sw.Stop());

	sw.Start();
	g_overrideInterface.Save(a_intfc, OverrideInterface::kSerializationVersion);
	SKSE::log::debug("{} - Serialized overrides {}ms", __FUNCTION__, sw.Stop());

	sw.Start();
	g_bodyMorphInterface.Save(a_intfc, BodyMorphInterface::kSerializationVersion);
	SKSE::log::debug("{} - Serialized body morphs {}ms", __FUNCTION__, sw.Stop());

	sw.Start();
	g_itemDataInterface.Save(a_intfc, ItemDataInterface::kSerializationVersion);
	SKSE::log::debug("{} - Serialized item data {}ms", __FUNCTION__, sw.Stop());
}

void SKEE64Serialization_Load(SKSE::SerializationInterface* a_intfc)
{
	SKSE::log::info("Loading...");

	std::uint32_t type, length, version;
	bool error = false;

	std::unordered_map<std::uint32_t, StringTableItem> stringTable;

	StopWatch sw;
	sw.Start();
	while (a_intfc->GetNextRecordInfo(type, version, length))
	{
		switch (type)
		{
			case 'STTB':	g_stringTable.Load(a_intfc, version, stringTable);						break;
			case 'MRST':	g_morphInterface.LoadMorphData(a_intfc, version, stringTable);			break;
			case 'SCDT':	g_morphInterface.LoadSculptData(a_intfc, version, stringTable);			break;
			case 'AOVL':	g_overlayInterface.Load(a_intfc, version);								break;
			case 'ACEN':	g_overrideInterface.LoadOverrides(a_intfc, version, stringTable);			break;
			case 'NDEN':	g_overrideInterface.LoadNodeOverrides(a_intfc, version, stringTable);		break;
			case 'WPEN':	g_overrideInterface.LoadWeaponOverrides(a_intfc, version, stringTable);	break;
			case 'SKNR':	g_overrideInterface.LoadSkinOverrides(a_intfc, version, stringTable);		break;
			case 'MRPH':	g_bodyMorphInterface.Load(a_intfc, version, stringTable);					break;
			case 'ITEE':	g_itemDataInterface.Load(a_intfc, version, stringTable);					break;
			case 'ACTM':	g_transformInterface.Load(a_intfc, version, stringTable);					break;
			default:
				SKSE::log::info("unhandled type {:08X} ({:.4})", type, std::string(reinterpret_cast<char*>(&type), 4));
				error = true;
				break;
		}
	}
	SKSE::log::debug("{} - Loaded {}ms", __FUNCTION__, sw.Stop());
	
	g_firstLoad = true;
}

typedef std::map <const std::type_info*, RE::GFxFunctionHandler*>	FunctionHandlerCache;
FunctionHandlerCache g_functionHandlerCache;

template <class T>
void SKEERegisterScaleformFunction(RE::GFxValue* dst, RE::GFxMovieView* movie, const char* name)
	requires(std::is_base_of_v<RE::GFxFunctionHandler, T>)
{
	// either allocate the object or retrieve an existing instance from the cache
	RE::GFxFunctionHandler* fn = nullptr;

	// check the cache
	FunctionHandlerCache::iterator iter = g_functionHandlerCache.find(&typeid(T));
	if (iter != g_functionHandlerCache.end())
		fn = iter->second;

	if (!fn)
	{
		// not found, allocate a new one
		fn = new T{};

		// add it to the cache
		// cache now owns the object as far as refcounting goes
		g_functionHandlerCache[&typeid(T)] = fn;
	}

	// create the function object
	RE::GFxValue	fnValue{};
	movie->CreateFunction(&fnValue, fn);

	// register it
	dst->SetMember(name, fnValue);
}

bool RegisterNiOverrideScaleform(RE::GFxMovieView * view, RE::GFxValue * root)
{
	using namespace ScaleformUtils;

	RE::GFxValue obj{};
	RegisterBool(root, "bEnableOverlays", g_enableOverlays);

	view->CreateObject(&obj);
	RegisterNumber(&obj, "iNumOverlays", g_numBodyOverlays);
	RegisterNumber(&obj, "iSpellOverlays", g_numSpellBodyOverlays);
	root->SetMember("body", obj);

	obj.SetNull();
	view->CreateObject(&obj);
	RegisterNumber(&obj, "iNumOverlays", g_numHandOverlays);
	RegisterNumber(&obj, "iSpellOverlays", g_numSpellHandOverlays);
	root->SetMember("hand", obj);

	obj.SetNull();
	view->CreateObject(&obj);
	RegisterNumber(&obj, "iNumOverlays", g_numFeetOverlays);
	RegisterNumber(&obj, "iSpellOverlays", g_numSpellFeetOverlays);
	root->SetMember("feet", obj);

	obj.SetNull();
	view->CreateObject(&obj);
	RegisterNumber(&obj, "iNumOverlays", g_numFaceOverlays);
	RegisterNumber(&obj, "iSpellOverlays", g_numSpellFaceOverlays);
	root->SetMember("face", obj);

	RegisterBool(root, "bPlayerOnly", g_playerOnly);

	SKEERegisterScaleformFunction<SKSEScaleform_GetDyeItems>(root, view, "GetDyeItems");
	SKEERegisterScaleformFunction<SKSEScaleform_GetDyeableItems>(root, view, "GetDyeableItems");
	SKEERegisterScaleformFunction<SKSEScaleform_SetItemDyeColor>(root, view, "SetItemDyeColor");
	SKEERegisterScaleformFunction<SKSEScaleform_SetItemDyeColors>(root, view, "SetItemDyeColors");

	return true;
}

bool RegisterCharGenScaleform(RE::GFxMovieView * view, RE::GFxValue * root)
{
	using namespace ScaleformUtils;

	RegisterBool(root, "bEnableSculpting", g_enableSculpting);
	RegisterBool(root, "bEnableHeadExport", g_enableHeadExport);
#if defined(ENABLE_SKYRIM_VR)
	RegisterNumber(root, "vrCategoryMask", g_raceSexMenuCategoryMask);
	SKEE::MenuAppearance::Register(view, root);
	SKEE::MenuConfiguration::Register(view, root);
	SKEE::MenuExtensions::Register(view, root);
	g_characterCreationInterface.RegisterMovie(view, root);
	SKEE::FaceView::Register(view, root);
	SKEE::AvatarLighting::Register(view, root);
	SKEE::VR::RegisterRaceSexMenuInputTrace(view, root);
	SKEE::VR::RegisterRaceSexMenuKeyboard(view, root);
#endif

	SKEERegisterScaleformFunction<SKSEScaleform_ImportHead>(root, view, "ImportHead");
	SKEERegisterScaleformFunction<SKSEScaleform_ExportHead>(root, view, "ExportHead");
	SKEERegisterScaleformFunction<SKSEScaleform_SavePreset>(root, view, "SavePreset");
	SKEERegisterScaleformFunction<SKSEScaleform_LoadPreset>(root, view, "LoadPreset");
	SKEERegisterScaleformFunction<SKSEScaleform_ReadPreset>(root, view, "ReadPreset");
	SKEERegisterScaleformFunction<SKSEScaleform_ReloadSliders>(root, view, "ReloadSliders");
	SKEERegisterScaleformFunction<SKSEScaleform_GetSliderData>(root, view, "GetSliderData");
	SKEERegisterScaleformFunction<SKSEScaleform_GetSliderPartData>(root, view, "GetSliderPartData");
	SKEERegisterScaleformFunction<SKSEScaleform_GetModName>(root, view, "GetModName");
	SKEERegisterScaleformFunction<SKSEScaleform_SetCharacterName>(root, view, "SetCharacterName");

	SKEERegisterScaleformFunction<SKSEScaleform_GetPlayerPosition>(root, view, "GetPlayerPosition");
	SKEERegisterScaleformFunction<SKSEScaleform_GetPlayerRotation>(root, view, "GetPlayerRotation");
	SKEERegisterScaleformFunction<SKSEScaleform_SetPlayerRotation>(root, view, "SetPlayerRotation");

	SKEERegisterScaleformFunction<SKSEScaleform_GetRaceSexCameraRot>(root, view, "GetRaceSexCameraRot");
	SKEERegisterScaleformFunction<SKSEScaleform_GetRaceSexCameraPos>(root, view, "GetRaceSexCameraPos");
	SKEERegisterScaleformFunction<SKSEScaleform_SetRaceSexCameraPos>(root, view, "SetRaceSexCameraPos");

	SKEERegisterScaleformFunction<SKSEScaleform_CreateMorphEditor>(root, view, "CreateMorphEditor");
	SKEERegisterScaleformFunction<SKSEScaleform_ReleaseMorphEditor>(root, view, "ReleaseMorphEditor");

	SKEERegisterScaleformFunction<SKSEScaleform_LoadImportedHead>(root, view, "LoadImportedHead");
	SKEERegisterScaleformFunction<SKSEScaleform_ReleaseImportedHead>(root, view, "ReleaseImportedHead");

	SKEERegisterScaleformFunction<SKSEScaleform_BeginRotateMesh>(root, view, "BeginRotateMesh");
	SKEERegisterScaleformFunction<SKSEScaleform_DoRotateMesh>(root, view, "DoRotateMesh");
	SKEERegisterScaleformFunction<SKSEScaleform_EndRotateMesh>(root, view, "EndRotateMesh");

	SKEERegisterScaleformFunction<SKSEScaleform_BeginPanMesh>(root, view, "BeginPanMesh");
	SKEERegisterScaleformFunction<SKSEScaleform_DoPanMesh>(root, view, "DoPanMesh");
	SKEERegisterScaleformFunction<SKSEScaleform_EndPanMesh>(root, view, "EndPanMesh");

	SKEERegisterScaleformFunction<SKSEScaleform_BeginPaintMesh>(root, view, "BeginPaintMesh");
	SKEERegisterScaleformFunction<SKSEScaleform_DoPaintMesh>(root, view, "DoPaintMesh");
	SKEERegisterScaleformFunction<SKSEScaleform_EndPaintMesh>(root, view, "EndPaintMesh");

	SKEERegisterScaleformFunction<SKSEScaleform_DoHoverMesh>(root, view, "DoHoverMesh");
	SKEERegisterScaleformFunction<SKSEScaleform_BeginSculptTrace>(root, view, "BeginSculptTrace");
	SKEERegisterScaleformFunction<SKSEScaleform_ReadSculptTrace>(root, view, "ReadSculptTrace");
	RegisterNumber(root, "sculptTraceContractVersion", 2);
	RegisterString(root, view, "sculptTraceJson", "");

	SKEERegisterScaleformFunction<SKSEScaleform_GetCurrentBrush>(root, view, "GetCurrentBrush");
	SKEERegisterScaleformFunction<SKSEScaleform_SetCurrentBrush>(root, view, "SetCurrentBrush");

	SKEERegisterScaleformFunction<SKSEScaleform_GetBrushes>(root, view, "GetBrushes");
	SKEERegisterScaleformFunction<SKSEScaleform_SetBrushData>(root, view, "SetBrushData");

	SKEERegisterScaleformFunction<SKSEScaleform_GetMeshes>(root, view, "GetMeshes");
	SKEERegisterScaleformFunction<SKSEScaleform_SetMeshData>(root, view, "SetMeshData");

	SKEERegisterScaleformFunction<SKSEScaleform_UndoAction>(root, view, "UndoAction");
	SKEERegisterScaleformFunction<SKSEScaleform_RedoAction>(root, view, "RedoAction");
	SKEERegisterScaleformFunction<SKSEScaleform_GoToAction>(root, view, "GoToAction");
	SKEERegisterScaleformFunction<SKSEScaleform_GetActionLimit>(root, view, "GetActionLimit");
	SKEERegisterScaleformFunction<SKSEScaleform_ClearSculptData>(root, view, "ClearSculptData");

	SKEERegisterScaleformFunction<SKSEScaleform_GetMeshCameraRadius>(root, view, "GetMeshCameraRadius");
	SKEERegisterScaleformFunction<SKSEScaleform_SetMeshCameraRadius>(root, view, "SetMeshCameraRadius");

	SKEERegisterScaleformFunction<SKSEScaleform_GetExternalFiles>(root, view, "GetExternalFiles");
	return true;
}

bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* registry)
{
	papyrusNiOverride::RegisterFuncs(registry);
	papyrusCharGen::RegisterFuncs(registry);
	return true;
}

void InterfaceExchangeMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type)
	{
	case InterfaceExchangeMessage::kMessage_ExchangeInterface:
	{
		InterfaceExchangeMessage* exchangeMessage = (InterfaceExchangeMessage*)message->data;
		exchangeMessage->interfaceMap = &g_interfaceMap;
	}
	break;
	}
}

template <typename T>
static void SKEERegisterEventSink(RE::BSTEventSink<T>* sink)
{
	auto* holder = RE::ScriptEventSourceHolder::GetSingleton();
	if (holder) {
		holder->PrependEventSink(sink);
	}
}

void SKSEMessageHandler(SKSE::MessagingInterface::Message * message)
{
	switch (message->type)
	{
		case SKSE::MessagingInterface::kPostLoad:
		{
#if defined(ENABLE_SKYRIM_VR)
			SKEE::VR::InstallNewGameIntentObserver();
#endif
			if (!g_enableEarlyRegistration)
			{
				if (auto* msg = SKSE::GetMessagingInterface()) {
					msg->RegisterListener(nullptr, InterfaceExchangeMessageHandler);
				}
			}
		}
		break;

		case SKSE::MessagingInterface::kInputLoaded:
		{
			if (g_enableAutoTransforms || g_enableBodyGen) {
				SKEERegisterEventSink<RE::TESObjectLoadedEvent>(&g_actorUpdateManager);
			}
		}
		break;
		case SKSE::MessagingInterface::kPreLoadGame:
			g_characterCreationInterface.OnSaveLoading();
			g_enableBodyInit = false;
			g_tintMaskInterface.ManageTints();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			if (!message->data) g_characterCreationInterface.CancelConfiguredName();
			g_enableBodyInit = true;
			g_tintMaskInterface.ReleaseTints();
			break;
		case SKSE::MessagingInterface::kNewGame:
		{
			g_characterCreationInterface.BeginNewGame();
			g_actorUpdateManager.setNewGame(true);
			break;
		}
		case SKSE::MessagingInterface::kDataLoaded:
		{
			if (auto* ui = RE::UI::GetSingleton()) {
				ui->AddEventSink(&g_characterCreationInterface);
				g_characterCreationInterface.ObserveCurrentState();
				SKSE::log::info("Registered character-creation automation observer");
			} else {
				SKSE::log::error("Could not register character-creation automation observer: UI singleton unavailable");
			}

#if defined(ENABLE_SKYRIM_VR)
			if (g_applyRaceSexMenuPlacement) {
				if (ApplyRaceSexMenuPlacement()) {
					SKSE::log::info("Applied RaceSexMenu-local VR placement through Skyrim's native VRUI settings");
				} else {
					SKSE::log::error("Could not apply the complete RaceSexMenu-local VR placement setting set");
				}
			}

			if (g_enableRaceSexMouseCursor) {
				if (SKEE::VR::RegisterRaceSexMenuPointerInput()) {
					SKSE::log::info("Installed RaceSexMenu VR movie-selection and Trigger/A pointer-input hooks");
				} else {
					SKSE::log::error("Could not install RaceSexMenu VR movie-selection and Trigger/A pointer-input hooks");
				}

			}
			// Keyboard lifetime cleanup must not depend on pointer support being enabled.
			if (auto* ui = RE::UI::GetSingleton()) {
				ui->AddEventSink(&g_raceSexMouseCursorSink);
				SKSE::log::info("Registered RaceSexMenu VR lifetime observer");
			} else {
				SKSE::log::error("Could not register RaceSexMenu VR lifetime observer: UI singleton unavailable");
			}
#endif

			if (g_enableBodyGen) {
				SKEERegisterEventSink<RE::TESInitScriptEvent>(&g_actorUpdateManager);

				g_bodyMorphInterface.LoadMods();
			}

			SKEERegisterEventSink<RE::TESUniqueIDChangeEvent>(&g_itemDataInterface);
			SKEERegisterEventSink<RE::TESLoadGameEvent>(&g_actorUpdateManager);

			g_tintMaskInterface.LoadMods();
			g_morphInterface.LoadMods();
			g_formTagInterface.LoadMods();
		}
		break;
	}
}

// SKSEPluginInfo moved to generated __skee64Plugin.cpp

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_intfc)
{
	REL::Module::get().reset(); // Static inited addresses fucko'd in CommonLibSSE-NG
	SKSE::Init(a_intfc);
	InitializeVR2Logging();

	g_skseVersion = a_intfc->SKSEVersion();
	g_runtimeVersion = a_intfc->RuntimeVersion().pack();
    g_task = SKSE::GetTaskInterface();

	SKSE::log::info(
		"RaceMenu NG VR 2 build identity: package={}, native={}, runtime={}, SKSE=0x{:08X}, CommonLibSSE-NG={}, VR Address Library={}",
		SKEE_VR2_PACKAGE_VERSION_STRING,
		SKEE_NATIVE_PLUGIN_VERSION_STRING,
		a_intfc->RuntimeVersion().string("."),
		g_skseVersion,
		SKEE_COMMONLIB_VERSION,
		SKEE_VR_ADDRESS_LIBRARY_VERSION);

	SKSE::log::debug("NetImmerse Override Enabled");

	SKEE64GetConfigValue("Features", "bEnableOverlays", &g_enableOverlays);
	SKEE64GetConfigValue("Features", "bEnableFaceOverlays", &g_enableFaceOverlays);
	SKEE64GetConfigValue("Features", "bEnableSculpting", &g_enableSculpting);
	SKEE64GetConfigValue("Features", "bEnableHeadExport", &g_enableHeadExport);
	SKEE64GetConfigValue("Features", "bEnableBodyGen", &g_enableBodyGen);
	SKEE64GetConfigValue("Features", "bEnableBodyMorph", &g_enableBodyMorph);
	SKEE64GetConfigValue("Features", "bEnableAutoTransforms", &g_enableAutoTransforms);
	SKEE64GetConfigValue("Features", "bEnableEquippableTransforms", &g_enableEquippableTransforms);
	SKEE64GetConfigValue("Features", "bEnableTintSync", &g_enableTintSync);
	SKEE64GetConfigValue("Features", "bEnableTintInventory", &g_enableTintInventory);
	SKEE64GetConfigValue("Features", "bEnableTintHairSlot", &g_enableTintHairSlot);
	SKEE64GetConfigValue("Features", "bEnableTangentSpaceCorrection", &g_enableTangentSpaceCorrection);
	SKEE64GetConfigValue("Features", "bEnableFaceNormalRecalculate", &g_enableFaceNormalRecalculate);
	SKEE64GetConfigValue("Features", "bEnableBodyNormalRecalculate", &g_enableBodyNormalRecalculate);
	SKEE64GetConfigValue("Features", "bEnableEarlyRegistration", &g_enableEarlyRegistration);
#if defined(ENABLE_SKYRIM_VR)
	SKEE64GetConfigValue("VR", "bEnableRaceSexMouseCursor", &g_enableRaceSexMouseCursor);
	SKEE64GetConfigValue("VR", "bApplyRaceSexMenuPlacement", &g_applyRaceSexMenuPlacement);
	SKEE64GetConfigValue("VR", "fRaceSexMenuOffsetX", &g_raceSexMenuOffsetX);
	SKEE64GetConfigValue("VR", "fRaceSexMenuOffsetY", &g_raceSexMenuOffsetY);
	SKEE64GetConfigValue("VR", "fRaceSexMenuOffsetZ", &g_raceSexMenuOffsetZ);
	if (!SKEE64GetConfigValue("VR", "fRaceSexMenuWorldYaw", &g_raceSexMenuWorldYaw)) {
		// Compatibility with the short-lived 0.1.25 setting name. The value now
		// controls the actual projected UI quad rather than the inert HMD offset.
		SKEE64GetConfigValue("VR", "fRaceSexMenuHMDYaw", &g_raceSexMenuWorldYaw);
	}
	SKEE64GetConfigValue("VR", "fRaceSexMenuWorldScale", &g_raceSexMenuWorldScale);
	SKEE64GetConfigValue("VR Categories", "uVisibleMask", &g_raceSexMenuCategoryMask);
	SKEE::MenuConfiguration::Configure(SKEE64GetConfigOption);
	SKEE::AvatarLighting::Configure(SKEE64GetConfigOption);
	bool enableFaceView = true;
	float faceDistance = 45;
	SKEE64GetConfigValue("VR", "bEnableFaceView", &enableFaceView);
	SKEE64GetConfigValue("Menu Profile VR Face", "fFaceDistance", &faceDistance);
	float faceEyeHeight = 5;
	SKEE64GetConfigValue("Menu Profile VR Face", "fFaceEyeHeight", &faceEyeHeight);
	SKEE::FaceView::Configure(enableFaceView, faceDistance, faceEyeHeight);
	SKEE::MenuAppearance::Configure(SKEE64GetConfigOption("Menu Appearance", "sBackgroundColor"),
		SKEE64GetConfigOption("Menu Appearance", "sTextColor"), F4EEGetRuntimeDirectory());
	SKEE::VR::SetRaceSexMenuWorldTransform(g_raceSexMenuWorldScale, g_raceSexMenuWorldYaw, g_applyRaceSexMenuPlacement);
	SKSE::log::info("RaceSexMenu VR mouse compatibility: {}", g_enableRaceSexMouseCursor ? "enabled" : "disabled");
	SKSE::log::info(
		"RaceSexMenu VR native placement: {} (X={:.4f}, Y={:.4f}, Z={:.4f}, world yaw={:.4f}, scale={:.3f})",
		g_applyRaceSexMenuPlacement ? "enabled" : "disabled",
		g_raceSexMenuOffsetX,
		g_raceSexMenuOffsetY,
		g_raceSexMenuOffsetZ,
		g_raceSexMenuWorldYaw,
		g_raceSexMenuWorldScale);
#endif

	// Toggle Specific Hooks which interact with game code
	SKEE64GetConfigValue("Hooks", "bBipedAttach", &g_hookBipedAttach);
	SKEE64GetConfigValue("Hooks", "bNativeSliders", &g_hookNativeSliders);
	SKEE64GetConfigValue("Hooks", "bMorphUpdates", &g_hookMorphUpdates);
	SKEE64GetConfigValue("Hooks", "bMorphExtensions", &g_hookMorphExtensions);
	SKEE64GetConfigValue("Hooks", "bSliderCallbacks", &g_hookSliderCallbacks);
	SKEE64GetConfigValue("Hooks", "bTintInventory", &g_hookTintInventory);
	SKEE64GetConfigValue("Hooks", "bHeadPreprocessing", &g_hookHeadPreprocessing);
	SKEE64GetConfigValue("Hooks", "bFaceOverlays", &g_hookFaceOverlays);
	SKEE64GetConfigValue("Hooks", "bTinting", &g_hookTinting);
	SKEE64GetConfigValue("Hooks", "bSuppressPatternWarnings", &g_suppressPatternWarnings);

	SKEE64GetConfigValue("Overlays", "bPlayerOnly", &g_playerOnly);
	SKEE64GetConfigValue("Overlays", "bImmediateArmor", &g_immediateArmor);
	SKEE64GetConfigValue("Overlays", "bImmediateFace", &g_immediateFace);

	SKEE64GetConfigValue("Overlays/Body", "iNumOverlays", &g_numBodyOverlays);
	SKEE64GetConfigValue("Overlays/Body", "iSpellOverlays", &g_numSpellBodyOverlays);
	SKEE64GetConfigValue("Overlays/Hands", "iNumOverlays", &g_numHandOverlays);
	SKEE64GetConfigValue("Overlays/Hands", "iSpellOverlays", &g_numSpellHandOverlays);
	SKEE64GetConfigValue("Overlays/Feet", "iNumOverlays", &g_numFeetOverlays);
	SKEE64GetConfigValue("Overlays/Feet", "iSpellOverlays", &g_numSpellFeetOverlays);
	SKEE64GetConfigValue("Overlays/Face", "iNumOverlays", &g_numFaceOverlays);
	SKEE64GetConfigValue("Overlays/Face", "iSpellOverlays", &g_numSpellFaceOverlays);

	SKEE64GetConfigValue("Overlays/Data", "bAlphaOverride", &g_overlayAlphaOverride);
	SKEE64GetConfigValue("Overlays/Data", "iAlphaFlags", &g_overlayAlphaFlags);
	SKEE64GetConfigValue("Overlays/Data", "iAlphaThreshold", &g_overlayAlphaThreshold);
	SKEE64GetConfigValue("Overlays/Data", "bForceDecal", &g_overlayForceDecal);

	std::string defaultTexture = SKEE64GetConfigOption("Overlays/Data", "sDefaultTexture");
	if (defaultTexture.empty()) {
		defaultTexture = "textures\\actors\\character\\overlays\\default.dds";
	}
	g_overlayInterface.SetDefaultTexture(defaultTexture);

	SKEE64GetConfigValue("General", "bDeferredBodyMorph", &g_deferredBodyMorph);

	SKEE64GetConfigValue("General", "iScaleMode", &g_scaleMode);
	SKEE64GetConfigValue("General", "iBodyMorphMode", &g_bodyMorphMode);
	SKEE64GetConfigValue("General", "bParallelMorphing", &g_parallelMorphing);
	SKEE64GetConfigValue("General", "uTintHairSlot", &g_tintHairSlot);
	SKEE64GetConfigValue("General", "bBodyMorphGPUCopy", &g_bodyMorphGPUCopy);
	SKEE64GetConfigValue("General", "bBodyMorphRebind", &g_bodyMorphRebind);

	std::uint64_t bodyMorphMemoryLimit = 256000000;
	if (SKEE64GetConfigValue("General", "uBodyMorphMemoryLimit", &bodyMorphMemoryLimit))
	{
		g_bodyMorphInterface.SetCacheLimit(bodyMorphMemoryLimit);
	}

	if (g_numBodyOverlays > 0x7F)
		g_numBodyOverlays = 0x7F;
	if (g_numSpellBodyOverlays > 0x7F)
		g_numSpellBodyOverlays = 0x7F;

	if (g_numHandOverlays > 0x7F)
		g_numHandOverlays = 0x7F;
	if (g_numSpellHandOverlays > 0x7F)
		g_numSpellHandOverlays = 0x7F;

	if (g_numFeetOverlays > 0x7F)
		g_numFeetOverlays = 0x7F;
	if (g_numSpellFeetOverlays > 0x7F)
		g_numSpellFeetOverlays = 0x7F;

	if (g_numFaceOverlays > 0x7F)
		g_numFaceOverlays = 0x7F;
	if (g_numSpellFaceOverlays > 0x7F)
		g_numSpellFaceOverlays = 0x7F;

	if (g_overlayAlphaThreshold > 0xFF)
		g_overlayAlphaThreshold = 0xFF;

	if(!g_enableFaceOverlays) {
		g_numFaceOverlays = 0;
		g_numSpellFaceOverlays = 0;
	}

	std::string	data = SKEE64GetConfigOption("FaceGen", "sTemplateRace");
	if (!data.empty())
		g_raceTemplate = data;

	SKEE64GetConfigValue("FaceGen", "fSliderMultiplier", &g_sliderMultiplier);
	SKEE64GetConfigValue("FaceGen", "fSliderInterval", &g_sliderInterval);

	if (g_sliderMultiplier <= 0)
		g_sliderMultiplier = 0.01f;
	if (g_sliderInterval <= 0)
		g_sliderInterval = 0.01f;
	if (g_sliderInterval > 1.0)
		g_sliderInterval = 1.0;

	SKEE64GetConfigValue("FaceGen", "bDisableFaceGenCache", &g_disableFaceGenCache);
	SKEE64GetConfigValue("FaceGen", "bExternalHeads", &g_externalHeads);
	SKEE64GetConfigValue("FaceGen", "bExtendedMorphs", &g_extendedMorphs);
	SKEE64GetConfigValue("FaceGen", "bAllowAllMorphs", &g_allowAllMorphs);
	SKEE64GetConfigValue("FaceGen", "bAllowAnyRacePart", &g_allowAnyRacePart);
	SKEE64GetConfigValue("FaceGen", "bAllowAnyGenderPart", &g_allowAnyGenderPart);
	SKEE64GetConfigValue("FaceGen", "bExportSkinToBone", &g_exportSkinToBone);

	SKEE64GetConfigValue("Sculpting", "fPanSpeed", &g_panSpeed);
	SKEE64GetConfigValue("Sculpting", "fFOV", &g_cameraFOV);
	SKEE64GetConfigValue("Sculpting", "iWidth", &g_viewWidth);
	SKEE64GetConfigValue("Sculpting", "iHeight", &g_viewHeight);

	SKEE64GetConfigValue("Sculpting", "fBackgroundA", &g_sculptBackgroundA);
	SKEE64GetConfigValue("Sculpting", "fBackgroundR", &g_sculptBackgroundR);
	SKEE64GetConfigValue("Sculpting", "fBackgroundG", &g_sculptBackgroundG);
	SKEE64GetConfigValue("Sculpting", "fBackgroundB", &g_sculptBackgroundB);
	SKEE64GetConfigValue("Sculpting", "fOffsetX", &g_sculptOffsetX);
	SKEE64GetConfigValue("Sculpting", "fOffsetY", &g_sculptOffsetY);
	SKEE64GetConfigValue("Sculpting", "fOffsetZ", &g_sculptOffsetZ);
	SKEE64GetConfigValue("Sculpting", "bDrawBrushPoint", &g_sculptDrawBrushPoint);
	SKEE64GetConfigValue("Sculpting", "bDrawBrushRadius", &g_sculptDrawBrushRadius);

	std::string types[CDXBrush::kBrushTypes];
	types[CDXBrush::kBrushType_Mask_Add] = "Brush/MaskAdd/";
	types[CDXBrush::kBrushType_Mask_Subtract] = "Brush/MaskSubtract/";
	types[CDXBrush::kBrushType_Inflate] = "Brush/Inflate/";
	types[CDXBrush::kBrushType_Deflate] = "Brush/Deflate/";
	types[CDXBrush::kBrushType_Smooth] = "Brush/Smooth/";
	types[CDXBrush::kBrushType_Move] = "Brush/Move/";

	std::string properties[CDXBrush::kBrushProperties];
	properties[CDXBrush::kBrushProperty_Radius] = "Radius";
	properties[CDXBrush::kBrushProperty_Strength] = "Strength";
	properties[CDXBrush::kBrushProperty_Falloff] = "Falloff";

	std::string values[CDXBrush::kBrushPropertyValues];
	values[CDXBrush::kBrushPropertyValue_Value] = "dbDefault";
	values[CDXBrush::kBrushPropertyValue_Min] = "dbMin";
	values[CDXBrush::kBrushPropertyValue_Max] = "dbMax";
	values[CDXBrush::kBrushPropertyValue_Interval] = "dbInterval";

	CDXBrush::InitGlobals();

	for (std::uint32_t b = 0; b < CDXBrush::kBrushTypes; b++) {
		for (std::uint32_t p = 0; p < CDXBrush::kBrushProperties; p++) {
			for (std::uint32_t v = 0; v < CDXBrush::kBrushPropertyValues; v++) {
				std::string section = types[b] + properties[p];
				double val = 0.0;
				if (SKEE64GetConfigValue(section.c_str(), values[v].c_str(), &val))
					g_brushProperties[b][p][v] = val;
			}
		}
	}

	g_commandInterface.RegisterCommands();

	if (auto* ser = SKSE::GetSerializationInterface()) {
		ser->SetUniqueID(0x534B4545);  // 'SKEE'
		ser->SetRevertCallback(SKEE64Serialization_Revert);
		ser->SetSaveCallback(SKEE64Serialization_Save);
		ser->SetLoadCallback(SKEE64Serialization_Load);
	}

	// register scaleform callbacks
	if (auto* sf = SKSE::GetScaleformInterface()) {
		sf->Register(RegisterNiOverrideScaleform, "NiOverride");
		sf->Register(RegisterCharGenScaleform, "CharGen");
	}

	if (auto* pap = SKSE::GetPapyrusInterface()) {
		pap->Register(RegisterPapyrusFunctions);
	}

	if (auto* msg = SKSE::GetMessagingInterface()) {
		msg->RegisterListener("SKSE", SKSEMessageHandler);
		if (g_enableEarlyRegistration)
		{
			msg->RegisterListener(nullptr, InterfaceExchangeMessageHandler);
		}

		if (g_enableTintHairSlot)
		{
			RE::BSTEventSource<SKSE::NiNodeUpdateEvent>* dispatcher = SKSE::GetNiNodeUpdateEventSource();
			if (dispatcher)
			{
				dispatcher->AddEventSink(&g_tintMaskInterface);
			}
		}
	}

	g_interfaceMap.AddInterface("Override", &g_overrideInterface);
	g_interfaceMap.AddInterface("Overlay", &g_overlayInterface);
	g_interfaceMap.AddInterface("NiTransform", &g_transformInterface);
	g_interfaceMap.AddInterface("BodyMorph", &g_bodyMorphInterface);
	g_interfaceMap.AddInterface("ItemData", &g_itemDataInterface);
	g_interfaceMap.AddInterface("TintMask", &g_tintMaskInterface);
	g_interfaceMap.AddInterface("FaceMorph", &g_morphInterface);
	g_interfaceMap.AddInterface("ActorUpdateManager", &g_actorUpdateManager);
	g_interfaceMap.AddInterface("Attachment", &g_attachmentInterface);
	g_interfaceMap.AddInterface("Command", &g_commandInterface);
	g_interfaceMap.AddInterface("FormTag", &g_formTagInterface);
    g_interfaceMap.AddInterface("Preset", &g_presetInterface);
	g_interfaceMap.AddInterface("CharacterCreation", &g_characterCreationInterface);
	g_interfaceMap.AddInterface("MenuExtensions", SKEE::MenuExtensions::GetInterface());

	if (g_enableTangentSpaceCorrection)
	{
		g_actorUpdateManager.AddInterface(&g_actorArmorTangentUpdater);
	}

	g_actorUpdateManager.AddInterface(&g_skeletonExtenderInterface);
	g_actorUpdateManager.AddInterface(&g_bodyMorphInterface);
	g_actorUpdateManager.AddInterface(&g_overlayInterface);
	g_actorUpdateManager.AddInterface(&g_overrideInterface);
	g_actorUpdateManager.AddInterface(&g_itemDataInterface);

	if (g_enableTintHairSlot)
	{
		g_actorUpdateManager.AddInterface(&g_tintMaskInterface);
	}

	return InstallSKEEHooks().success;
}
