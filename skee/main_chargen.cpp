#include "skse/PluginAPI.h"
#include "skse/skse_version.h"
#include "skse/SafeWrite.h"

#include "skse/GameRTTI.h"

#include "skse/ScaleformMovie.h"
#include "skse/ScaleformLoader.h"

#include "MorphHandler.h"
#include "PartHandler.h"
#include "Hooks.h"
#include "ScaleformFunctions.h"
#include "PapyrusCharGen.h"

#include "interfaces/IPluginInterface.h"
#include "interfaces/OverrideInterface.h"
#include "interfaces/NiTransformInterface.h"
#include "interfaces/OverlayInterface.h"
#include "interfaces/BodyMorphInterface.h"

#include "CDXBrush.h"

#include <shlobj.h>
#include <string>

#define MIN_TASK_VERSION 1
#define MIN_PAP_VERSION 1
#define MIN_SCALEFORM_VERSION 1
#define MIN_SERIALIZATION_VERSION 2
#define PLUGIN_VERSION 6

IDebugLog	gLog;
PluginHandle	g_pluginHandle = kPluginHandle_Invalid;
const UInt32 kSerializationDataVersion = 2;

// Interfaces
SKSESerializationInterface		* g_serialization = NULL;
SKSEScaleformInterface			* g_scaleform = NULL;
SKSETaskInterface				* g_task = NULL;
SKSEPapyrusInterface			* g_papyrus = NULL;
SKSEMessagingInterface			* g_messaging = NULL;

OverrideInterface				* g_overrideInterface = NULL;
NiTransformInterface			* g_transformInterface = NULL;
OverlayInterface				* g_overlayInterface = NULL;
BodyMorphInterface				* g_bodyMorphInterface = NULL;

MorphHandler g_morphHandler;
PartSet	g_partSet;
//CustomDataMap	g_customDataMap;

bool	g_externalHeads = false;
bool	g_extendedMorphs = true;
bool	g_allowAllMorphs = true;
bool	g_disableFaceGenCache = true;
float	g_sliderMultiplier = 1.0f;
float	g_sliderInterval = 0.01f;
float	g_panSpeed = 0.01f;
float	g_cameraFOV = 45.0f;
UInt32	g_numPresets = 10;
UInt32	g_customDataMax = 10;
std::string g_raceTemplate = "NordRace";

extern double g_brushProperties[CDXBrush::kBrushTypes][CDXBrush::kBrushProperties][CDXBrush::kBrushPropertyValues];

void Serialization_Revert(SKSESerializationInterface * intfc)
{
	g_morphHandler.Revert();
}

void Serialization_Save(SKSESerializationInterface * intfc)
{
	_DMESSAGE("Saving...");

	PlayerCharacter * player = (*g_thePlayer);
	TESNPC * playerBase = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);

	g_morphHandler.Save(playerBase, intfc, kSerializationDataVersion);

	_DMESSAGE("Save Complete.");
}

void Serialization_Load(SKSESerializationInterface * intfc)
{
	_DMESSAGE("Loading...");
	
#ifdef _DEBUG_DATADUMP
	g_morphHandler.DumpAll();
#endif

	PlayerCharacter * player = (*g_thePlayer);
	TESNPC * playerBase = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);

	g_morphHandler.Load(playerBase, intfc, kSerializationDataVersion);

	g_task->AddTask(new SKSETaskApplyMorphs(player));

	_DMESSAGE("Load Complete.");
}

const std::string & GetRuntimeDirectory(void)
{
	static std::string s_runtimeDirectory;

	if(s_runtimeDirectory.empty())
	{
		// can't determine how many bytes we'll need, hope it's not more than MAX_PATH
		char	runtimePathBuf[MAX_PATH];
		UInt32	runtimePathLength = GetModuleFileName(GetModuleHandle(NULL), runtimePathBuf, sizeof(runtimePathBuf));

		if(runtimePathLength && (runtimePathLength < sizeof(runtimePathBuf)))
		{
			std::string	runtimePath(runtimePathBuf, runtimePathLength);

			// truncate at last slash
			std::string::size_type	lastSlash = runtimePath.rfind('\\');
			if(lastSlash != std::string::npos)	// if we don't find a slash something is VERY WRONG
			{
				s_runtimeDirectory = runtimePath.substr(0, lastSlash + 1);

				_DMESSAGE("runtime root = %s", s_runtimeDirectory.c_str());
			}
			else
			{
				_WARNING("no slash in runtime path? (%s)", runtimePath.c_str());
			}
		}
		else
		{
			_WARNING("couldn't find runtime path (len = %d, err = %08X)", runtimePathLength, GetLastError());
		}
	}

	return s_runtimeDirectory;
}

const std::string & GetConfigPath(void)
{
	static std::string s_configPath;

	if(s_configPath.empty())
	{
		std::string	runtimePath = GetRuntimeDirectory();
		if(!runtimePath.empty())
		{
			s_configPath = runtimePath + "Data\\SKSE\\Plugins\\chargen.ini";

			_DMESSAGE("config path = %s", s_configPath.c_str());
		}
	}

	return s_configPath;
}

std::string GetConfigOption(const char * section, const char * key)
{
	std::string	result;

	const std::string & configPath = GetConfigPath();
	if(!configPath.empty())
	{
		char	resultBuf[256];
		resultBuf[0] = 0;

		UInt32	resultLen = GetPrivateProfileString(section, key, NULL, resultBuf, sizeof(resultBuf), configPath.c_str());

		result = resultBuf;
	}

	return result;
}

template<typename T>
bool GetConfigNumber(const char * section, const char * key, T * dataOut)
{
	std::string	data = GetConfigOption(section, key);
	if (data.empty())
		return false;

	return (sscanf_s(data.c_str(), "%d", dataOut) == 1);
}

template<>
bool GetConfigNumber<float>(const char * section, const char * key, float * dataOut)
{
	std::string	data = GetConfigOption(section, key);
	if(data.empty())
		return false;

	return (sscanf_s(data.c_str(), "%f", dataOut) == 1);
}

template<>
bool GetConfigNumber<double>(const char * section, const char * key, double * dataOut)
{
	std::string	data = GetConfigOption(section, key);
	if (data.empty())
		return false;

	return (sscanf_s(data.c_str(), "%lf", dataOut) == 1);
}

bool RegisterScaleform(GFxMovieView * view, GFxValue * root)
{
	RegisterFunction <SKSEScaleform_ImportHead>(root, view, "ImportHead");
	RegisterFunction <SKSEScaleform_ExportHead>(root, view, "ExportHead");
	RegisterFunction <SKSEScaleform_SavePreset>(root, view, "SavePreset");
	RegisterFunction <SKSEScaleform_LoadPreset>(root, view, "LoadPreset");
	RegisterFunction <SKSEScaleform_ReadPreset>(root, view, "ReadPreset");
	RegisterFunction <SKSEScaleform_ReloadSliders>(root, view, "ReloadSliders");
	RegisterFunction <SKSEScaleform_GetSliderData>(root, view, "GetSliderData");
	RegisterFunction <SKSEScaleform_GetModName>(root, view, "GetModName");

	RegisterFunction <SKSEScaleform_GetPlayerPosition>(root, view, "GetPlayerPosition");
	RegisterFunction <SKSEScaleform_GetPlayerRotation>(root, view, "GetPlayerRotation");
	RegisterFunction <SKSEScaleform_SetPlayerRotation>(root, view, "SetPlayerRotation");

	RegisterFunction <SKSEScaleform_GetRaceSexCameraRot>(root, view, "GetRaceSexCameraRot");
	RegisterFunction <SKSEScaleform_GetRaceSexCameraPos>(root, view, "GetRaceSexCameraPos");
	RegisterFunction <SKSEScaleform_SetRaceSexCameraPos>(root, view, "SetRaceSexCameraPos");

	RegisterFunction <SKSEScaleform_CreateMorphEditor>(root, view, "CreateMorphEditor");
	RegisterFunction <SKSEScaleform_ReleaseMorphEditor>(root, view, "ReleaseMorphEditor");

	RegisterFunction <SKSEScaleform_LoadImportedHead>(root, view, "LoadImportedHead");
	RegisterFunction <SKSEScaleform_ReleaseImportedHead>(root, view, "ReleaseImportedHead");

	RegisterFunction <SKSEScaleform_BeginRotateMesh>(root, view, "BeginRotateMesh");
	RegisterFunction <SKSEScaleform_DoRotateMesh>(root, view, "DoRotateMesh");
	RegisterFunction <SKSEScaleform_EndRotateMesh>(root, view, "EndRotateMesh");

	RegisterFunction <SKSEScaleform_BeginPanMesh>(root, view, "BeginPanMesh");
	RegisterFunction <SKSEScaleform_DoPanMesh>(root, view, "DoPanMesh");
	RegisterFunction <SKSEScaleform_EndPanMesh>(root, view, "EndPanMesh");

	RegisterFunction <SKSEScaleform_BeginPaintMesh>(root, view, "BeginPaintMesh");
	RegisterFunction <SKSEScaleform_DoPaintMesh>(root, view, "DoPaintMesh");
	RegisterFunction <SKSEScaleform_EndPaintMesh>(root, view, "EndPaintMesh");

	RegisterFunction <SKSEScaleform_GetCurrentBrush>(root, view, "GetCurrentBrush");
	RegisterFunction <SKSEScaleform_SetCurrentBrush>(root, view, "SetCurrentBrush");

	RegisterFunction <SKSEScaleform_GetBrushes>(root, view, "GetBrushes");
	RegisterFunction <SKSEScaleform_SetBrushData>(root, view, "SetBrushData");

	RegisterFunction <SKSEScaleform_GetMeshes>(root, view, "GetMeshes");
	RegisterFunction <SKSEScaleform_SetMeshData>(root, view, "SetMeshData");

	RegisterFunction <SKSEScaleform_UndoAction>(root, view, "UndoAction");
	RegisterFunction <SKSEScaleform_RedoAction>(root, view, "RedoAction");
	RegisterFunction <SKSEScaleform_GoToAction>(root, view, "GoToAction");
	RegisterFunction <SKSEScaleform_GetActionLimit>(root, view, "GetActionLimit");
	RegisterFunction <SKSEScaleform_ClearSculptData>(root, view, "ClearSculptData");

	RegisterFunction <SKSEScaleform_GetMeshCameraRadius>(root, view, "GetMeshCameraRadius");
	RegisterFunction <SKSEScaleform_SetMeshCameraRadius>(root, view, "SetMeshCameraRadius");

	RegisterFunction <SKSEScaleform_GetExternalFiles>(root, view, "GetExternalFiles");
	return true;
}

void SKSEMessageHandler(SKSEMessagingInterface::Message * message)
{
	switch (message->type)
	{
		case SKSEMessagingInterface::kMessage_PostLoad:
		{
			InterfaceExchangeMessage message;
			g_messaging->Dispatch(g_pluginHandle, InterfaceExchangeMessage::kMessage_ExchangeInterface, (void*)&message, sizeof(InterfaceExchangeMessage*), "nioverride");
			if (message.interfaceMap) {
				g_overrideInterface = (OverrideInterface*)message.interfaceMap->QueryInterface("Override");
				g_transformInterface = (NiTransformInterface *)message.interfaceMap->QueryInterface("NiTransform");
				g_overlayInterface = (OverlayInterface *)message.interfaceMap->QueryInterface("Overlay");
				g_bodyMorphInterface = (BodyMorphInterface *)message.interfaceMap->QueryInterface("BodyMorph");
			}
		}
		break;
	}
}

extern "C"
{

bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
{
	SInt32	logLevel = IDebugLog::kLevel_DebugMessage;
	if (GetConfigNumber("Debug", "iLogLevel", &logLevel))
		gLog.SetLogLevel((IDebugLog::LogLevel)logLevel);

	if (logLevel >= 0)
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim\\SKSE\\skse_chargen.log");

	_DMESSAGE("skse_chargen");

	// populate info structure
	info->infoVersion =	PluginInfo::kInfoVersion;
	info->name =		"chargen";
	info->version = PLUGIN_VERSION;

	// store plugin handle so we can identify ourselves later
	g_pluginHandle = skse->GetPluginHandle();

	if(skse->isEditor)
	{
		_FATALERROR("loaded in editor, marking as incompatible");
		return false;
	}
	else if(skse->runtimeVersion != RUNTIME_VERSION_1_9_32_0)
	{
		_FATALERROR("unsupported runtime version %08X", skse->runtimeVersion);
		return false;
	}

	// get the serialization interface and query its version
	g_serialization = (SKSESerializationInterface *)skse->QueryInterface(kInterface_Serialization);
	if(!g_serialization)
	{
		_FATALERROR("couldn't get serialization interface");
		return false;
	}

	if(g_serialization->version < MIN_SERIALIZATION_VERSION)
	{
		_FATALERROR("serialization interface too old (%d expected %d)", g_serialization->version, MIN_SERIALIZATION_VERSION);
		return false;
	}

	// get the scaleform interface and query its version
	g_scaleform = (SKSEScaleformInterface *)skse->QueryInterface(kInterface_Scaleform);
	if(!g_scaleform)
	{
		_FATALERROR("couldn't get scaleform interface");
		return false;
	}
	if(g_scaleform->interfaceVersion < MIN_SCALEFORM_VERSION)
	{
		_FATALERROR("scaleform interface too old (%d expected %d)", g_scaleform->interfaceVersion, MIN_SCALEFORM_VERSION);
		return false;
	}

	// get the task interface and query its version
	g_task = (SKSETaskInterface *)skse->QueryInterface(kInterface_Task);
	if(!g_task)
	{
		_FATALERROR("couldn't get task interface");
		return false;
	}
	if(g_task->interfaceVersion < MIN_TASK_VERSION)
	{
		_FATALERROR("task interface too old (%d expected %d)", g_task->interfaceVersion, MIN_TASK_VERSION);
		return false;
	}

	// get the papyrus interface and query its version
	g_papyrus = (SKSEPapyrusInterface *)skse->QueryInterface(kInterface_Papyrus);
	if(!g_papyrus)
	{
		_WARNING("couldn't get papyrus interface");
	}
	if(g_papyrus && g_papyrus->interfaceVersion < MIN_PAP_VERSION)
	{
		_WARNING("papyrus interface too old (%d expected %d)", g_papyrus->interfaceVersion, MIN_PAP_VERSION);
	}

	g_messaging = (SKSEMessagingInterface *)skse->QueryInterface(kInterface_Messaging);
	if (!g_messaging) {
		_ERROR("couldn't get messaging interface");
	}

	// supported runtime version
	return true;
}

bool RegisterFuncs(VMClassRegistry * registry)
{	
	papyrusCharGen::RegisterFuncs(registry);
	return true;
}

bool SKSEPlugin_Load(const SKSEInterface * skse)
{
	_MESSAGE("CharGen Morph Support Enabled.");

	std::string	data = GetConfigOption("FaceGen", "sTemplateRace");
	if (!data.empty())
		g_raceTemplate = data;

	float	sliderMultiplier = 1.0f;
	if (GetConfigNumber("FaceGen", "fSliderMultiplier", &sliderMultiplier))
	{
		g_sliderMultiplier = sliderMultiplier;
		if(g_sliderMultiplier <= 0)
			g_sliderMultiplier = 0.01f;
	}
	float	sliderInterval = 0.01f;
	if (GetConfigNumber("FaceGen", "fSliderInterval", &sliderMultiplier))
	{
		g_sliderInterval = sliderInterval;
		if(g_sliderInterval <= 0)
			g_sliderInterval = 0.01f;
		if(g_sliderInterval > 1.0)
			g_sliderInterval = 1.0;
	}

	UInt32	disableFaceGenCache = 1;
	if (GetConfigNumber("FaceGen", "bDisableFaceGenCache", &disableFaceGenCache))
	{
		g_disableFaceGenCache = (disableFaceGenCache > 0);
	}

	UInt32	externalHeads = 0;
	if (GetConfigNumber("FaceGen", "bExternalHeads", &externalHeads))
	{
		g_externalHeads = (externalHeads > 0);
	}
	UInt32	extendedMorphs = 1;
	if (GetConfigNumber("FaceGen", "bExtendedMorphs", &extendedMorphs))
	{
		g_extendedMorphs = (extendedMorphs > 0);
	}
	UInt32	allowAllMorphs = 1;
	if (GetConfigNumber("FaceGen", "bAllowAllMorphs", &allowAllMorphs))
	{
		g_allowAllMorphs = (allowAllMorphs > 0);
	}

	float	panSpeed = 0.01f;
	if (GetConfigNumber("FaceGen", "fPanSpeed", &panSpeed))
	{
		g_panSpeed = panSpeed;
	}

	float	cameraFOV = 45.0f;
	if (GetConfigNumber("FaceGen", "fFOV", &cameraFOV))
	{
		g_cameraFOV = cameraFOV;
	}

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

	for (UInt32 b = 0; b < CDXBrush::kBrushTypes; b++) {
		for (UInt32 p = 0; p < CDXBrush::kBrushProperties; p++) {
			for (UInt32 v = 0; v < CDXBrush::kBrushPropertyValues; v++) {
				std::string section = types[b] + properties[p];
				double val = 0.0;
				if (GetConfigNumber(section.c_str(), values[v].c_str(), &val))
					g_brushProperties[b][p][v] = val;
			}
		}
	}

	InstallHooks();

	if(g_disableFaceGenCache) {
		SafeWrite8(0x008868C0, 0xC3); // Disable PrecacheCharGen
		SafeWrite8(0x00886B50, 0xC3); // Disable PrecacheCharGenClear
	}

	//SafeWrite8(DATA_ADDR(0x005A0AB0, 0xF5) + 2, 0xFF);

	// Patch Morph Limit
	/*SafeWrite8(DATA_ADDR(0x005A7570, 0x33) + 1, 0xFF);
	SafeWrite8(DATA_ADDR(0x005A7870, 0x9D) + 1, 0xFF);
	SafeWrite8(DATA_ADDR(0x005A7870, 0xA4) + 1, 0xFF);*/

	/*SafeWrite8(0x005A7280 + 0x0F + 1, 0xFF);
	SafeWrite8(0x005A7240 + 0x10 + 1, 0xFF);
	SafeWrite8(0x005A7200 + 0x10 + 1, 0xFF);
	SafeWrite8(0x005A71C0 + 0x10 + 1, 0xFF);*/

	if (g_serialization) {
		g_serialization->SetUniqueID(g_pluginHandle, 'FCGN');
		g_serialization->SetRevertCallback(g_pluginHandle, Serialization_Revert);
		g_serialization->SetSaveCallback(g_pluginHandle, Serialization_Save);
		g_serialization->SetLoadCallback(g_pluginHandle, Serialization_Load);
	}

	// register scaleform callbacks
	if (g_scaleform) {
		g_scaleform->Register("CharGen", RegisterScaleform);
	}

	if (g_papyrus) {
		g_papyrus->Register(RegisterFuncs);
	}

	if (g_messaging) {
		g_messaging->RegisterListener(g_pluginHandle, "SKSE", SKSEMessageHandler);
	}

	return true;
}

};
