#include "Hooks.h"
#include "MorphHandler.h"
#include "PartHandler.h"

#include "skse/SafeWrite.h"
#include "skse/NiObjects.h"
#include "skse/GameData.h"
#include "skse/GameRTTI.h"

#include "common/IFileStream.h"
#include "skse/GameStreams.h"
#include "skse/HashUtil.h"

#include "skse/NiGeometry.h"

#include <algorithm>

bool CacheTempTRI(UInt32 hash, const char * originalPath);

extern MorphHandler g_morphHandler;
extern PartSet	g_partSet;
extern UInt32	g_customDataMax;
extern bool		g_externalHeads;
extern bool		g_extendedMorphs;
extern bool		g_allowAllMorphs;

static const UInt32 kInstallRegenHeadHook_Base = 0x005A4B80 + 0x49B;
static const UInt32 kInstallRegenHeadHook_Entry_retn = kInstallRegenHeadHook_Base + 0x8;

enum
{
	kRegenHeadHook_EntryStackOffset1 = 0x20,
	kRegenHeadHook_EntryStackOffset2 = 0xA8,

	kRegenHeadHook_VarHeadPart = 0x08,
	kRegenHeadHook_VarFaceGenNode = 0x04,
	kRegenHeadHook_VarNPC = 0x0C
};

void __stdcall ApplyPreset(TESNPC * npc, BSFaceGenNiNode * headNode, BGSHeadPart * headPart)
{
	g_morphHandler.ApplyPreset(npc, headNode, headPart);
}

__declspec(naked) void InstallRegenHeadHook_Entry(void)
{
	__asm
	{
		pushad
		mov		eax, [esp + kRegenHeadHook_EntryStackOffset1 + kRegenHeadHook_EntryStackOffset2 + kRegenHeadHook_VarHeadPart]
		push	eax
		mov		eax, [esp + kRegenHeadHook_EntryStackOffset1 + kRegenHeadHook_EntryStackOffset2 + kRegenHeadHook_VarFaceGenNode + 0x04]
		push	eax
		mov		eax, [esp + kRegenHeadHook_EntryStackOffset1 + kRegenHeadHook_EntryStackOffset2 + kRegenHeadHook_VarNPC + 0x08]
		push	eax
		call	ApplyPreset
		popad

		pop edi
		pop ebx
		add esp, 0xA0
		jmp[kInstallRegenHeadHook_Entry_retn]
	}
}

static const UInt32 kInstallForceRegenHeadHook_Base = 0x0056AEB0 + 0x35;
static const UInt32 kInstallForceRegenHeadHook_Entry_retn = kInstallForceRegenHeadHook_Base + 0x7;		// Standard execution

enum
{
	kForceRegenHeadHook_EntryStackOffset = 0x10,
};

bool	* g_useFaceGenPreProcessedHeads = (bool*)0x0125D280;

bool __stdcall IsHeadGenerated(TESNPC * npc)
{
	// For some reason the NPC vanilla preset data is reset when the actor is disable/enabled
	auto presetData = g_morphHandler.GetPreset(npc);
	if (presetData) {
		if (!npc->faceMorph)
			npc->faceMorph = (TESNPC::FaceMorphs*)FormHeap_Allocate(sizeof(TESNPC::FaceMorphs));

		UInt32 i = 0;
		for (auto & preset : presetData->presets) {
			npc->faceMorph->presets[i] = preset;
			i++;
		}

		i = 0;
		for (auto & morph : presetData->morphs) {
			npc->faceMorph->option[i] = morph;
			i++;
		}
	}
	return (presetData != NULL) || !(*g_useFaceGenPreProcessedHeads);
}

__declspec(naked) void InstallForceRegenHeadHook_Entry(void)
{
	__asm
	{
		push	esi
		call	IsHeadGenerated
		cmp		al, 1
		jmp[kInstallForceRegenHeadHook_Entry_retn]
	}
}

typedef void(*_LoadActorValues)();
const _LoadActorValues LoadActorValues = (_LoadActorValues)0x00692FA0;

#ifdef USE_TRICACHE
void RemoveCachedTRIFiles()
{
	std::vector<std::string> files;
	WIN32_FIND_DATA findFileData;

	std::string cacheDirectory("Data\\meshes\\");
	cacheDirectory.append(MORPH_CACHE_PATH);
	std::string findDirectory = cacheDirectory;
	findDirectory.append("*");

	HANDLE hFindFile = FindFirstFile(findDirectory.c_str(), &findFileData);
	if (hFindFile)
	{
		do
		{
			std::string filePath = findFileData.cFileName;
			if (_strnicmp(filePath.substr(filePath.find_last_of(".") + 1).c_str(), "tri", 3) == 0) {
				files.push_back(filePath);
			}
		} while (FindNextFile(hFindFile, &findFileData));
		FindClose(hFindFile);
	}

	for (std::vector<std::string>::iterator it = files.begin(); it != files.end(); ++it)
	{
		std::string filePath = cacheDirectory;
		filePath.append(*it);
		_DMESSAGE("Removing cached tri - %s", filePath.c_str());
		DeleteFile(filePath.c_str());
	}
}
#endif

class ExtendedMorphCache : public MorphMap::Visitor
{
public:
	virtual bool Accept(BSFixedString morphName)
	{
		g_morphHandler.GetExtendedModelTri(morphName);
		return false;
	}
};

SInt32 GetGameSettingInt(const char * key)
{
	Setting	* setting = (*g_gameSettingCollection)->Get(key);
	if (setting && setting->GetType() == Setting::kType_Integer)
		return setting->data.s32;

	return 0;
}

void _cdecl LoadActorValues_Hook()
{
	LoadActorValues();

#ifdef USE_TRICACHE
	RemoveCachedTRIFiles();
#endif

	DataHandler * dataHandler = DataHandler::GetSingleton();
	if (dataHandler)
	{
		UInt8 modCount = dataHandler->modList.loadedModCount;
		for (UInt32 i = 0; i < modCount; i++)
		{
			ModInfo * modInfo = dataHandler->modList.loadedMods[i];
			std::string fixedPath = "Meshes\\";
			fixedPath.append(SLIDER_MOD_DIRECTORY);
			std::string modPath = modInfo->name;
			modPath.append("\\");

			g_morphHandler.ReadRaces(fixedPath, modPath, "races.ini");
			if (g_extendedMorphs)
				g_morphHandler.ReadMorphs(fixedPath, modPath, "morphs.ini");

			ReadPartReplacements(fixedPath, modPath, "replacements.ini");
		}


		/*for(MorphMap::iterator it = g_morphHandler.m_morphMap.begin(); it != g_morphHandler.m_morphMap.end(); ++it)
		{
		std::string beginLine = "extension = ";
		beginLine.append(it->first.data);
		for(MorphSet::iterator sit = it->second.begin(); sit != it->second.end(); ++sit)
		{
		beginLine.append(", ");
		beginLine.append(sit->data);
		}
		_MESSAGE("%s", beginLine.c_str());
		}*/

		if (g_extendedMorphs) {
			BGSHeadPart * part = NULL;
			for (UInt32 i = 0; i < dataHandler->headParts.count; i++)
			{
				if (dataHandler->headParts.GetNthItem(i, part)) {
					if (g_morphHandler.CacheHeadPartModel(part)) {

						BSFixedString key = part->chargenMorph.GetModelName();

						// Cache all of the extended morphs
						ExtendedMorphCache extendedCache;
						g_morphHandler.VisitMorphMap(key, extendedCache);
					}
				}
			}
		}

		// Create default slider maps
		TESRace * race = NULL;
		for (UInt32 i = 0; i < dataHandler->races.count; i++)
		{
			if (dataHandler->races.GetNthItem(i, race)) {

				if (g_allowAllMorphs) {
					for (UInt32 i = 0; i <= 1; i++) {
						if (race->chargenData[i]) {
							for (UInt32 t = 0; t < FacePresetList::kNumPresets; t++) {
								const char * gameSetting = FacePresetList::GetSingleton()->presets[t].data->gameSettingName;
								race->chargenData[i]->presetFlags[t][0] = 0xFFFFFFFF;
								race->chargenData[i]->presetFlags[t][1] = 0xFFFFFFFF;
								race->chargenData[i]->totalPresets[t] = GetGameSettingInt(gameSetting);
							}
						}
					}
				}
				if ((race->data.raceFlags & TESRace::kRace_FaceGenHead) == TESRace::kRace_FaceGenHead)
					g_morphHandler.m_raceMap.CreateDefaultMap(race);
			}
		}
		/*
		std::set<BSFixedString> uniqueTris;
		for (UInt32 i = 0; i < dataHandler->headParts.count; i++)
		{
			BGSHeadPart * headPart = NULL;
			if (dataHandler->headParts.GetNthItem(i, headPart)) {
				BSFixedString chargenPath(headPart->chargenMorph.GetModelName());
				if (chargenPath == BSFixedString(""))
					continue;
				uniqueTris.insert(chargenPath);
			}
		}

		for (auto tri : uniqueTris)
		{
			std::string lowerStr(tri.data);
			std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
			UInt32 hash = HashUtil::CRC32(lowerStr.c_str());

			char buffer[0x20];
			memset(buffer, 0, 0x20);
			sprintf_s(buffer, 0x20, MORPH_CACHE_TEMPLATE, hash);

			std::string path(MORPH_CACHE_DIR);
			path.append(buffer);
			BSFixedString cachePath(path.c_str());

			//CacheTempTRI(hash, tri.data);

			g_morphHandler.m_morphMap.AddMorph(tri, cachePath);
		}*/
	}
}

typedef void(*_ClearFaceGenCache)();
const _ClearFaceGenCache ClearFaceGenCache = (_ClearFaceGenCache)0x00886B50;

void _cdecl ClearFaceGenCache_Hooked()
{
	ClearFaceGenCache();

	g_morphHandler.RevertInternals();
	g_partSet.Revert(); // Cleanup HeadPart List before loading new ones
}

bool _cdecl CacheTRIFile_Hook(const char * filePath, BSFaceGenDB::TRI::DBTraits::MorphSet ** morphSet, UInt32 * unk1)
{
#ifdef _DEBUG_HOOK
	//_MESSAGE("Caching TRI: '%s'", filePath);
#endif

	bool ret = CacheTRIFile(filePath, morphSet, unk1);
#ifdef __DEBUG
	_MESSAGE("External Cache - %s - %s - MorphSet: %08X", ret ? "Failed" : "Succeeded", filePath, morphSet);
#endif
	// Add additional morphs here based on morph path instead

#ifdef __DEBUG
	BSFaceGenDB::TRI::DBTraits::MorphSet * foundSet = *morphSet;
	if (foundSet)
	{
		BSFaceGenDB::TRI::DBTraits::MorphData morphEntry;

		_MESSAGE("Set - %s MorphCount: %d Capacity: %d", foundSet->fileName, foundSet->morphData.count, foundSet->morphData.arr.capacity);

		gLog.Indent();
		for (UInt32 n = 0; n < foundSet->morphData.count; n++) {
			BSFaceGenDB::TRI::DBTraits::MorphData morphEntry;
			if (foundSet->morphData.GetNthItem(n, morphEntry)) {
				_MESSAGE("Morph %d - %s baseDiff: %f DiffVertNum: %d BaseVertNum: %d DiffVertPosNum: %d\t\t04 : %d\t0C : %d\t10 : %d", n, morphEntry.morphName, morphEntry.baseDiff, morphEntry.diffVertexNum, morphEntry.baseVertexNum, morphEntry.diffVertexPosNum, morphEntry.unk04, morphEntry.unk0C, morphEntry.unk10);
				// 				for(UInt32 i = 0; i < morphEntry.baseVertexNum; i++)
				// 				{
				// 					_MESSAGE("Vertex %d - X: %d Y: %d Z: %d \t\t Floating Point - X: %f Y: %f Z: %f", i, morphEntry.diffData[i].x, morphEntry.diffData[i].y, morphEntry.diffData[i].z, morphEntry.diffData[i].x * morphEntry.baseDiff, morphEntry.diffData[i].y * morphEntry.baseDiff, morphEntry.diffData[i].z * morphEntry.baseDiff);
				// 				}
			}
		}
		gLog.Outdent();
	}
#endif

#ifdef USE_TRICACHE
	std::string pathStr(filePath);
	std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::tolower);
	if (pathStr.compare(0, sizeof(MORPH_CACHE_PATH)-1, MORPH_CACHE_PATH) == 0) {
		pathStr.erase(0, sizeof(MORPH_CACHE_PATH)-1);
		UInt32 hash = 0;
		sscanf_s(pathStr.c_str(), "%08X.tri", &hash);
		auto morphData = g_morphHandler.GetSculptFromHost(hash);
		if (morphData) {
			BSFaceGenDB::TRI::DBTraits::MorphSet * foundSet = *morphSet;
			if (foundSet) {
				g_morphHandler.WriteMorphsToDatabase(morphData, foundSet);
			}
		}
	}
#endif

	return ret;
}

void TESNPC_Hooked::UpdateMorphs_Hooked(void * unk1, BSFaceGenNiNode * faceNode)
{
	CALL_MEMBER_FN(this, UpdateMorphs)(unk1, faceNode);
#ifdef _DEBUG_HOOK
	_DMESSAGE("UpdateMorphs_Hooked - Applying custom morphs");
#endif
	try
	{
		g_morphHandler.ApplyMorphs(this, faceNode);
	}
	catch (...)
	{
		_DMESSAGE("%s - Fatal error", __FUNCTION__);
	}
}

void TESNPC_Hooked::UpdateMorph_Hooked(BGSHeadPart * headPart, BSFaceGenNiNode * faceNode)
{
	CALL_MEMBER_FN(this, UpdateMorph)(headPart, faceNode);
#ifdef _DEBUG_HOOK
	_DMESSAGE("UpdateMorph_Hooked - Applying single custom morph");
#endif
	try
	{
		g_morphHandler.ApplyMorph(this, headPart, faceNode);
	}
	catch (...)
	{
		_DMESSAGE("%s - Fatal error", __FUNCTION__);
	}
}

void FxResponseArgsList_Hooked::AddArgument_Hooked(GFxValue * value)
{
	CALL_MEMBER_FN(this, AddArgument)(value);
#ifdef _DEBUG_HOOK
	_DMESSAGE("AddArgument_Hooked - Loading Category...");
#endif
	GFxValue arg;
	arg.SetString("$EXTRA");
	CALL_MEMBER_FN(this, AddArgument)(&arg);
	arg.SetNumber(SLIDER_CATEGORY_EXTRA);
	CALL_MEMBER_FN(this, AddArgument)(&arg);

	arg.SetString("$EXPRESSIONS");
	CALL_MEMBER_FN(this, AddArgument)(&arg);
	arg.SetNumber(SLIDER_CATEGORY_EXPRESSIONS);
	CALL_MEMBER_FN(this, AddArgument)(&arg);
#ifdef _DEBUG_HOOK
	_DMESSAGE("AddArgument_Hooked - Loaded Category.");
#endif
}

UInt32 SliderArray::AddSlider_Hooked(RaceMenuSlider * slider)
{
	UInt32 ret = CALL_MEMBER_FN(this, AddSlider)(slider);
#ifdef _DEBUG_HOOK
	_DMESSAGE("AddSlider_Hooked - Loading Sliders...");
#endif
	g_morphHandler.LoadSliders(this, slider);
#ifdef _DEBUG_HOOK
	_DMESSAGE("AddSlider_Hooked - Loaded Sliders.");
#endif
	return ret;
}

/*UInt32 RaceSexMenu_Hooked::OpenMenu_Hooked(UInt32 unk1)
{
	UInt32 ret = CALL_MEMBER_FN(this, OpenMenu)(unk1);

	return ret;
}*/

#ifdef _DEBUG_HOOK
class DumpPartVisitor : public PartSet::Visitor
{
public:
	bool Accept(UInt32 key, BGSHeadPart * headPart)
	{
		_DMESSAGE("DumpPartVisitor - Key: %d Part: %s", key, headPart->partName.data);
		return false;
	}
};
#endif

void DataHandler_Hooked::GetValidPlayableHeadParts_Hooked(UInt32 unk1, void * unk2)
{
#ifdef _DEBUG_HOOK
	_DMESSAGE("Reverting Parts:");
	DumpPartVisitor dumpVisitor;
	g_partSet.Visit(dumpVisitor);
#endif

	g_partSet.Revert(); // Cleanup HeadPart List before loading new ones

	CALL_MEMBER_FN(this, GetValidPlayableHeadParts)(unk1, unk2);
}

// Pre-filtered by ValidRace and Gender
UInt8 BGSHeadPart_Hooked::IsPlayablePart_Hooked()
{
	UInt8 ret = CALL_MEMBER_FN(this, IsPlayablePart)();

#ifdef _DEBUG_HOOK
	_DMESSAGE("IsPlayablePart_Hooked - Reading Part: %08X : %s", this->formID, this->partName.data);
#endif

	if(this->type >= BGSHeadPart::kNumTypes) {
		if((this->partFlags & BGSHeadPart::kFlagExtraPart) == 0) { // Skip Extra Parts
			if(strcmp(this->model.GetModelName(), "") == 0)
				g_partSet.SetDefaultPart(this->type, this);
			else
				g_partSet.AddPart(this->type, this);
		}
		return false; // Prevents hanging if the HeadPart is marked as Playable
	}
	else if ((this->partFlags & BGSHeadPart::kFlagExtraPart) == 0 && ret)
	{
		// Sets the default part of this type
		if (g_partSet.GetDefaultPart(this->type) == NULL) {
			auto playeRace = (*g_thePlayer)->race;
			if (playeRace) {
				TESNPC * npc = DYNAMIC_CAST((*g_thePlayer)->baseForm, TESForm, TESNPC);
				UInt8 gender = CALL_MEMBER_FN(npc, GetSex)();

				auto chargenData = playeRace->chargenData[gender];
				if (chargenData) {
					auto headParts = chargenData->headParts;
					if (headParts) {
						for (UInt32 i = 0; i < headParts->count; i++) {
							BGSHeadPart * part;
							headParts->GetNthItem(i, part);
							if (part->type == this->type)
								g_partSet.SetDefaultPart(part->type, part);
						}
					}
				}
			}
		}

		// maps the pre-existing part to this type
		g_partSet.AddPart(this->type, this);
	}

	return ret;
}

#ifdef _DEBUG
#pragma warning (push)
#pragma warning (disable : 4200)
struct RTTIType
{
	void	* typeInfo;
	UInt32	pad;
	char	name[0];
};

struct RTTILocator
{
	UInt32		sig, offset, cdOffset;
	RTTIType	* type;
};
#pragma warning (pop)

const char * GetObjectClassName(void * objBase)
{
	const char	* result = "<no rtti>";

	__try
	{
		void		** obj = (void **)objBase;
		RTTILocator	** vtbl = (RTTILocator **)obj[0];
		RTTILocator	* rtti = vtbl[-1];
		RTTIType	* type = rtti->type;

		// starts with ,?
		if((type->name[0] == '.') && (type->name[1] == '?'))
		{
			// is at most 100 chars long
			for(UInt32 i = 0; i < 100; i++)
			{
				if(type->name[i] == 0)
				{
					// remove the .?AV
					result = type->name + 4;
					break;
				}
			}
		}
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		// return the default
	}

	return result;
}

void DumpClass(void * theClassPtr, UInt32 nIntsToDump)
{
	UInt32* basePtr = (UInt32*)theClassPtr;
	_DMESSAGE("DumpClass: %X", basePtr);

	gLog.Indent();

	if (!theClassPtr) return;
	for (UInt32 ix = 0; ix < nIntsToDump; ix++ ) {
		UInt32* curPtr = basePtr+ix;
		const char* curPtrName = NULL;
		UInt32 otherPtr = 0;
		float otherFloat = 0.0;
		const char* otherPtrName = NULL;
		if (curPtr) {
			curPtrName = GetObjectClassName((void*)curPtr);

			__try
			{
				otherPtr = *curPtr;
				otherFloat = *(float*)(curPtr);
			}
			__except(EXCEPTION_EXECUTE_HANDLER)
			{
				//
			}

			if (otherPtr) {
				otherPtrName = GetObjectClassName((void*)otherPtr);
			}
		}

		_DMESSAGE("%3d +%03X ptr: 0x%08X: %32s *ptr: 0x%08x | %f: %32s", ix, ix*4, curPtr, curPtrName, otherPtr, otherFloat, otherPtrName);
	}

	gLog.Outdent();
}
#endif

class MorphVisitor : public MorphMap::Visitor
{
public:
	MorphVisitor::MorphVisitor(BSFaceGenModel * model, const char ** morphName, NiAVObject ** headNode, float relative, UInt8 unk1)
	{
		m_model = model;
		m_morphName = morphName;
		m_headNode = headNode;
		m_relative = relative;
		m_unk1 = unk1;
	}
	bool Accept(BSFixedString morphName)
	{
		TRIModelData & morphData = g_morphHandler.GetExtendedModelTri(morphName, true);
		if (morphData.morphModel && morphData.triFile) {
			NiGeometry * geometry = NULL;
			if (m_headNode && (*m_headNode))
				geometry = (*m_headNode)->GetAsNiGeometry();
			
			if (geometry)
				morphData.triFile->Apply(geometry, *m_morphName, m_relative);
		}
			//UInt8 ret = CALL_MEMBER_FN(m_model, ApplyMorph)(m_morphName, morphData.morphModel, m_headNode, m_relative, m_unk1);

		/*BSFaceGenDB::TRI::DBTraits::MorphSet * mset = NULL;
		UInt32 unk1 = 0;
		if (CacheTRIFile_Hook(newMorph->GetModelName(), &mset, &unk1) == 0) {
			if (!mset) // Why?
				return false;

			auto modelObj = m_model->unk08;
			if (modelObj) {
				auto morphData = modelObj->unk10;
				if (morphData != mset->faceMorphData) {
					if (morphData)
						morphData->DecRef();

					modelObj->unk10 = mset->faceMorphData;
					if (modelObj->unk10)
						modelObj->unk10->IncRef();
				}
				morphData = modelObj->unk10;
				if (modelObj) {
					NiGeometry * geometry = (*m_headNode)->GetAsNiGeometry();
					NiGeometryData * geometryData = niptr_cast<NiGeometryData>(geometry->m_spModelData);
					if (geometryData) {
						NiGeometryData::Data0 data;
						data.unk00 = 0;
						data.unk04 = 0;
						data.unk08 = 0;
						if (CALL_MEMBER_FN(geometryData, Unk1)(1) && CALL_MEMBER_FN(geometryData, Unk2)(&data)) {
							CALL_MEMBER_FN(morphData, ApplyMorph)(m_morphName, geometry, m_relative, m_unk1);
							CALL_MEMBER_FN(geometryData, Unk3)(1);
						}
					}
				}
			}
		}*/

		
		return false;
	}
private:
	BSFaceGenModel	* m_model;
	const char		** m_morphName;
	NiAVObject		** m_headNode;
	float			m_relative;
	UInt8			m_unk1;
};

UInt8 BSFaceGenModel_Hooked::ApplyRaceMorph_Hooked(const char ** morphName, TESModelTri * modelMorph, NiAVObject ** headNode, float relative, UInt8 unk1)
{
	//BGSHeadPart * headPart = (BGSHeadPart *)((UInt32)modelMorph - offsetof(BGSHeadPart, raceMorph));
	UInt8 ret = CALL_MEMBER_FN(this, ApplyMorph)(morphName, modelMorph, headNode, relative, unk1);
#ifdef _DEBUG
	//_MESSAGE("%08X - Applying %s from %s : %s", this, morphName[0], modelMorph->name.data, headPart->partName.data);
#endif

	try
	{
		MorphVisitor morphVisitor(this, morphName, headNode, relative, unk1);
		g_morphHandler.VisitMorphMap(modelMorph->GetModelName(), morphVisitor);
	}
	catch (...)
	{
		_ERROR("%s - fatal error while applying morph (%s)", __FUNCTION__, *morphName);
	}

	

	return ret;
}

#ifdef USE_TRICACHE
// Writes a CustomMorph file for overwriting
bool CacheTempTRI(UInt32 hash, const char * originalPath)
{
	if(originalPath[0] == 0 || hash == 0) {
		_ERROR("%s - Invalid hash or path", __FUNCTION__);
		return true;
	}

	std::string path("meshes\\");
	path.append(MORPH_CACHE_PATH);

	char pathBuffer[MAX_PATH];
	memset(pathBuffer, 0, 0x20);
	sprintf_s(pathBuffer, 0x20, "%08X.tri", hash);
	path.append(pathBuffer);

	BSFixedString newPath(path.c_str());

	// Cached file already exists, load it
	BSResourceNiBinaryStream cached(newPath.data);
	if (cached.IsValid()) {
#ifdef _DEBUG
		//_DMESSAGE("%s - Loaded cached TRI %s", __FUNCTION__, newPath.data);
#endif
		return false;
	}

	memset(pathBuffer, 0, MAX_PATH);
	sprintf_s(pathBuffer, MAX_PATH, "meshes\\%s", originalPath);
	BSFixedString originalFile(pathBuffer);

	memset(pathBuffer, 0, MAX_PATH);
	sprintf_s(pathBuffer, MAX_PATH, "Data\\%s", newPath.data);
	BSFixedString fullPath(pathBuffer);

	// Load up original file
	BSResourceNiBinaryStream file(originalFile.data);
	if (!file.IsValid()) {
		_ERROR("%s - Couldn't open morph for reading: %s", __FUNCTION__, originalFile.data);
		return true;
	}

	IFileStream	currentFile;
	IFileStream::MakeAllDirs(fullPath.data);

	if(!currentFile.Create(fullPath.data))
	{
		_ERROR("%s - Couldn't open morph for writing: %s", __FUNCTION__, fullPath.data);
		return true;
	}
	try
	{
		char buffer[MAX_PATH];
		memset(buffer, 0, MAX_PATH);

		_DMESSAGE("%s - Writing from %s to %s", __FUNCTION__, originalFile.data, fullPath.data);

		UInt32 vertexNum = 0, faceNum = 0, UVNum = 0, morphNum = 0;
		UInt32 baseNum = 0;

		// Header
		file.Read(buffer, 0x08);
		currentFile.WriteBuf(buffer, 0x08);

		// Vertices
		file.Read(&vertexNum, sizeof(vertexNum));
		currentFile.Write32(vertexNum);

		// Faces
		file.Read(&faceNum, sizeof(faceNum));
		currentFile.Write32(faceNum);

		// Polyquads, unknown 2, 3
		file.Read(buffer, 0x0C);
		currentFile.WriteBuf(buffer, 0x0C);

		// UV
		file.Read(&UVNum, sizeof(UVNum));
		currentFile.Write32(UVNum);

		// Flags
		file.Read(buffer, 0x04);
		currentFile.WriteBuf(buffer, 0x04);

		// Num morphs
		file.Read(&morphNum, sizeof(morphNum));
		SInt64 morphOffset = currentFile.GetOffset();
		currentFile.Write32(morphNum);

		// Num modifiers
		file.Read(buffer, 0x04);
		currentFile.WriteBuf(buffer, 0x04);

		// Mod vertices
		file.Read(buffer, 0x04);
		currentFile.WriteBuf(buffer, 0x04);

		// Unknown 7-10
		file.Read(buffer, 0x04 * 4);
		currentFile.WriteBuf(buffer, 0x04 * 4);

		// Base mesh
		for (UInt32 i = 0; i < vertexNum; i++)
		{
			float	x, y, z;
			file.Read(&x, sizeof(x));
			currentFile.WriteFloat(x);
			file.Read(&y, sizeof(y));
			currentFile.WriteFloat(y);
			file.Read(&z, sizeof(z));
			currentFile.WriteFloat(z);
		}

		// Faces
		for (UInt32 i = 0; i < faceNum; i++)
		{
			UInt32	x, y, z;
			file.Read(&x, sizeof(x));
			currentFile.Write32(x);
			file.Read(&y, sizeof(y));
			currentFile.Write32(y);
			file.Read(&z, sizeof(z));
			currentFile.Write32(z);
		}

		// UV coords
		for (UInt32 i = 0; i < UVNum; i++)
		{
			float	x, y;
			file.Read(&x, sizeof(x));
			currentFile.WriteFloat(x);
			file.Read(&y, sizeof(y));
			currentFile.WriteFloat(y);
		}

		// Texture tris
		for (UInt32 i = 0; i < faceNum; i++)
		{
			UInt32	x, y, z;
			file.Read(&x, sizeof(x));
			currentFile.Write32(x);
			file.Read(&y, sizeof(y));
			currentFile.Write32(y);
			file.Read(&z, sizeof(z));
			currentFile.Write32(z);
		}

		// Write the morph count
		morphNum = g_customDataMax;
		SInt64 currentOffset = currentFile.GetOffset();
		currentFile.SetOffset(morphOffset);
		currentFile.Write32(morphNum);
		currentFile.SetOffset(currentOffset);

		// Write morphs
		for(UInt32 i = 0; i < morphNum; i++)
		{
			memset(buffer, 0, MAX_PATH);
			sprintf_s(buffer, MAX_PATH, "CustomMorph%d", i);
			UInt32 strLength = strlen(buffer) + 1;
			currentFile.Write32(strLength);
			currentFile.WriteBuf(buffer, strLength);
			currentFile.WriteFloat(0); // Base Diff

			// Write vertex data
			for(UInt32 j = 0; j < vertexNum; j++)
			{
				currentFile.Write16(0); // DiffX
				currentFile.Write16(0); // DiffY
				currentFile.Write16(0); // DiffZ
			}
		}
	}
	catch(...)
	{
		_ERROR("%s - exception during tri write.", __FUNCTION__);
	}

	currentFile.Close();
	return false;
}
#endif

UInt8 BSFaceGenModel_Hooked::ApplyChargenMorph_Hooked(const char ** morphName, TESModelTri * modelMorph, NiAVObject ** headNode, float relative, UInt8 unk1)
{
#ifdef _DEBUG
	//_MESSAGE("%08X - Applying %s from %s : %s - %f", this, morphName[0], modelMorph->name.data, headPart->partName.data, relative);
#endif

	UInt8 ret = CALL_MEMBER_FN(this, ApplyMorph)(morphName, modelMorph, headNode, relative, unk1);

#ifdef USE_TRICACHE
	std::string lowerStr(modelMorph->GetModelName());
	std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);

	UInt32 hash = HashUtil::CRC32(lowerStr.c_str());
	CacheTempTRI(hash, lowerStr.c_str());
#endif

	try
	{
		MorphVisitor morphVisitor(this, morphName, headNode, relative, unk1);
		g_morphHandler.VisitMorphMap(BSFixedString(modelMorph->GetModelName()), morphVisitor);
	}
	catch (...)
	{
		_ERROR("%s - fatal error while applying morph (%s)", __FUNCTION__, *morphName);
	}

	

	return ret;
}

void RaceSexMenu_Hooked::DoubleMorphCallback_Hooked(float newValue, UInt32 sliderId)
{
	RaceMenuSlider * slider = NULL;
	RaceSexMenu::RaceComponent * raceData = NULL;

	UInt8 gender = 0;
	PlayerCharacter * player = (*g_thePlayer);
	TESNPC * actorBase = DYNAMIC_CAST(player->baseForm, TESForm, TESNPC);
	if(actorBase)
		gender = CALL_MEMBER_FN(actorBase, GetSex)();
	BSFaceGenNiNode * faceNode = player->GetFaceGenNiNode();

	if(raceIndex < sliderData[gender].count)
		raceData = &sliderData[gender][raceIndex];
	if(raceData && sliderId < raceData->sliders.count)
		slider = &raceData->sliders[sliderId];

	if(raceData && slider) {
#ifdef _DEBUG_HOOK
		_DMESSAGE("Name: %s Value: %f Callback: %s Index: %d", slider->name, slider->value, slider->callback, slider->index);
#endif
		if(slider->index >= SLIDER_OFFSET) {
			UInt32 sliderIndex = slider->index - SLIDER_OFFSET;
			SliderInternalPtr sliderInternal = g_morphHandler.GetSliderByIndex(player->race, sliderIndex);
			if(!sliderInternal)
				return;

			float currentValue = g_morphHandler.GetMorphValueByName(actorBase, sliderInternal->name);
			float relative = newValue - currentValue;

			if(relative == 0.0 && sliderInternal->type != SliderInternal::kTypeHeadPart) {
				// Nothing to morph here
#ifdef _DEBUG_HOOK
				_DMESSAGE("Skipping Morph %s", sliderInternal->name.data);
#endif
				return;
			}

			if(sliderInternal->type == SliderInternal::kTypePreset)
			{
				slider->value = newValue;

				char buffer[MAX_PATH];
				slider->value = newValue;
				sprintf_s(buffer, MAX_PATH, "%s%d", sliderInternal->lowerBound.data, (UInt32)currentValue);
				g_morphHandler.SetMorph(actorBase, faceNode, buffer, -1.0);
				memset(buffer, 0, MAX_PATH);
				sprintf_s(buffer, MAX_PATH, "%s%d", sliderInternal->lowerBound.data, (UInt32)newValue);
				g_morphHandler.SetMorph(actorBase, faceNode, buffer, 1.0);

				g_morphHandler.SetMorphValue(actorBase, sliderInternal->name, newValue);
				return;
			}

			if(sliderInternal->type == SliderInternal::kTypeHeadPart)
			{
				slider->value = newValue;

				UInt8 partType = sliderInternal->presetCount;

				HeadPartList * partList = g_partSet.GetPartList(partType);
				if(partList)
				{
					if(newValue == 0.0) {
						BGSHeadPart * oldPart = actorBase->GetCurrentHeadPartByType(partType);
						if(oldPart) {
							BGSHeadPart * defaultPart = g_partSet.GetDefaultPart(partType);
							if(defaultPart && oldPart != defaultPart) {
								CALL_MEMBER_FN(actorBase, ChangeHeadPart)(defaultPart);
								ChangeActorHeadPart(player, oldPart, defaultPart);
							}
						}
						return;
					}
					BGSHeadPart * targetPart = g_partSet.GetPartByIndex(partList, (UInt32)newValue - 1);
					if(targetPart) {
						BGSHeadPart * oldPart = actorBase->GetCurrentHeadPartByType(partType);
						if (oldPart != targetPart) {
							CALL_MEMBER_FN(actorBase, ChangeHeadPart)(targetPart);
							ChangeActorHeadPart(player, oldPart, targetPart);
						}
					}
				}

				return;
			}


			// Cross from positive to negative
			if(newValue < 0.0 && currentValue > 0.0) {
				// Undo the upper morph
				SetRelativeMorph(actorBase, faceNode, sliderInternal->upperBound, -abs(currentValue));
#ifdef _DEBUG_HOOK
				_DMESSAGE("Undoing Upper Morph: New: %f Old: %f Relative %f Remaining %f", newValue, currentValue, relative, relative - currentValue);
#endif
				relative = newValue;
			}

			// Cross from negative to positive
			if(newValue > 0.0 && currentValue < 0.0) {
				// Undo the lower morph
				SetRelativeMorph(actorBase, faceNode, sliderInternal->lowerBound, -abs(currentValue));
#ifdef _DEBUG_HOOK
				_DMESSAGE("Undoing Lower Morph: New: %f Old: %f Relative %f Remaining %f", newValue, currentValue, relative, relative - currentValue);
#endif
				relative = newValue;
			}

#ifdef _DEBUG_HOOK
			_DMESSAGE("CurrentValue: %f Relative: %f SavedValue: %f", currentValue, relative, slider->value);
#endif
			slider->value = newValue;

			BSFixedString bound = sliderInternal->lowerBound;
			if(newValue < 0.0) {
				bound = sliderInternal->lowerBound;
				relative = -relative;
			} else if(newValue > 0.0) {
				bound = sliderInternal->upperBound;
			} else {
				if(currentValue > 0.0) {
					bound = sliderInternal->upperBound;
				} else {
					bound = sliderInternal->lowerBound;
					relative = -relative;
				}
			}

#ifdef _DEBUG_HOOK
			_DMESSAGE("Morphing %d - %s Relative: %f", sliderIndex, bound.data, relative);
#endif

			SetRelativeMorph(actorBase, faceNode, bound, relative);
			g_morphHandler.SetMorphValue(actorBase, sliderInternal->name, newValue);
			return;
		}
	}

	CALL_MEMBER_FN(this, DoubleMorphCallback)(newValue, sliderId);
}

void RaceSexMenu_Hooked::SetRelativeMorph(TESNPC * npc, BSFaceGenNiNode * faceNode, BSFixedString name, float relative)
{
	float absRel = abs(relative);
	if(absRel > 1.0) {
		float max = 0.0;
		if(relative < 0.0)
			max = -1.0;
		if(relative > 0.0)
			max = 1.0;
		UInt32 count = (UInt32)absRel;
		for(UInt32 i = 0; i < count; i++) {
			g_morphHandler.SetMorph(npc, faceNode, name.data, max);
			relative -= max;
		}
	}
	g_morphHandler.SetMorph(npc, faceNode, name.data, relative);
}

#include "CDXNifScene.h"
#include "CDXNifMesh.h"

#include "CDXCamera.h"

#include "skse/NiRenderer.h"
#include "skse/NiTextures.h"
#include <d3dx9.h>
#pragma comment(lib, "d3dx9.lib")


CDXNifScene						g_World;
extern CDXModelViewerCamera			g_Camera;                // A model viewing camera

void RaceSexMenu_Hooked::RenderMenu_Hooked(void)
{
	CALL_MEMBER_FN(this, RenderMenu)();

	LPDIRECT3DDEVICE9 pDevice = NiDX9Renderer::GetSingleton()->m_pkD3DDevice9;
	if (!pDevice) // This shouldnt happen
		return;

	if(g_World.IsVisible() && g_World.GetTextureGroup()) {
		NiRenderedTexture * renderedTexture = g_World.GetTextureGroup()->renderedTexture[0];
		if(renderedTexture) {
			g_World.Begin(pDevice);
			LPDIRECT3DSURFACE9 oldTarget;
			pDevice->GetRenderTarget(0,&oldTarget);
			LPDIRECT3DSURFACE9 pRenderSurface = NULL;
			LPDIRECT3DTEXTURE9 pRenderTexture = (LPDIRECT3DTEXTURE9)((NiTexture::NiDX9TextureData*)renderedTexture->rendererData)->texture;
			pRenderTexture->GetSurfaceLevel(0, &pRenderSurface);
			pDevice->SetRenderTarget(0, pRenderSurface);
			g_World.Render(pDevice);
			pDevice->SetRenderTarget(0, oldTarget);
			g_World.End(pDevice);
		}
	}
}

void InstallHooks()
{
	WriteRelCall(DATA_ADDR(0x00699100, 0x275), (UInt32)&LoadActorValues_Hook); // Hook for loading initial data on startup

	/*WriteRelCall(DATA_ADDR(0x005A5B70, 0x48), (UInt32)&CacheTRIFile_Hook); // Used to cache internal morph set
	WriteRelCall(DATA_ADDR(0x005A5E70, 0x20C), (UInt32)&CacheTRIFile_Hook);
	WriteRelCall(DATA_ADDR(0x005A93E0, 0x3A), (UInt32)&CacheTRIFile_Hook);*/

	WriteRelCall(DATA_ADDR(0x00882290, 0x185), GetFnAddr(&DataHandler_Hooked::GetValidPlayableHeadParts_Hooked)); // Cleans up HeadPart List
	WriteRelCall(DATA_ADDR(0x00886C70, 0x97), (UInt32)&ClearFaceGenCache_Hooked); // RaceMenu dtor Cleans up HeadPart List
	WriteRelCall(DATA_ADDR(0x00881320, 0x67), GetFnAddr(&BGSHeadPart_Hooked::IsPlayablePart_Hooked)); // Custom head part insertion

	if (g_extendedMorphs) {
		WriteRelCall(DATA_ADDR(0x005A4070, 0xBE), GetFnAddr(&BSFaceGenModel_Hooked::ApplyChargenMorph_Hooked)); // Load and apply extended morphs
		WriteRelCall(DATA_ADDR(0x005A5CE0, 0x39), GetFnAddr(&BSFaceGenModel_Hooked::ApplyRaceMorph_Hooked)); // Load and apply extended morphs
	}
	WriteRelCall(DATA_ADDR(0x005A4AC0, 0x67), GetFnAddr(&TESNPC_Hooked::UpdateMorphs_Hooked)); // Updating all morphs when head is regenerated
	WriteRelCall(DATA_ADDR(0x005AA230, 0x5C), GetFnAddr(&TESNPC_Hooked::UpdateMorph_Hooked)); // ChangeHeadPart to update morph on single part
	WriteRelCall(DATA_ADDR(0x00882290, 0x2E26), GetFnAddr(&SliderArray::AddSlider_Hooked)); // Create Slider 
	WriteRelCall(DATA_ADDR(0x00881DD0, 0x436), GetFnAddr(&FxResponseArgsList_Hooked::AddArgument_Hooked)); // Add Slider

	WriteRelCall(DATA_ADDR(0x00882290, 0x337C), GetFnAddr(&RaceSexMenu_Hooked::DoubleMorphCallback_Hooked)); // Change Slider OnLoad
	WriteRelCall(DATA_ADDR(0x0087FA50, 0x93), GetFnAddr(&RaceSexMenu_Hooked::DoubleMorphCallback_Hooked)); // Change Slider OnCallback

	SafeWrite32(DATA_ADDR(0x010E7404, 0x18), GetFnAddr(&RaceSexMenu_Hooked::RenderMenu_Hooked)); // Hooks RaceMenu renderer

	if (!g_externalHeads) {
		WriteRelJump(kInstallRegenHeadHook_Base, (UInt32)&InstallRegenHeadHook_Entry); // FaceGen Regenerate HeadPart Hook
		WriteRelJump(kInstallForceRegenHeadHook_Base, (UInt32)&InstallForceRegenHeadHook_Entry); // Preprocessed Head Hook
	}
}