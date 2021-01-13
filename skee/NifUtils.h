#pragma once

#include "skse64/GameThreads.h"
#include "skse64/GameTypes.h"

#include "skse64/NiTypes.h"
#include "skse64/NiObjects.h"
#include "skse64/NiSerialization.h"

#include <functional>

class NiTexture;
class NiTriBasedGeom;
class TESNPC;
class BGSHeadPart;
class BSFaceGenNiNode;
class BSGeometry;
class NiGeometry;
class NiTriStripsData;
class NiBinaryStream;
struct SKSESerializationInterface;
class InventoryEntryData;
class TESObjectARMO;
class TESObjectARMA;

class SKSETaskExportHead : public TaskDelegate
{
	virtual void Run();
	virtual void Dispose() { delete this; }

public:
	SKSETaskExportHead(Actor * actor, BSFixedString nifPath, BSFixedString ddsPath);

	UInt32			m_formId;
	BSFixedString	m_nifPath;
	BSFixedString	m_ddsPath;
};

class SKSETaskExportTintMask : public TaskDelegate
{
	virtual void Run();
	virtual void Dispose() { delete this; }

public:
	SKSETaskExportTintMask(BSFixedString filePath, BSFixedString fileName) : m_filePath(filePath), m_fileName(fileName) {};

	BSFixedString	m_filePath;
	BSFixedString	m_fileName;
};

class SKSETaskRefreshTintMask : public TaskDelegate
{
	virtual void Run();
	virtual void Dispose() { delete this; };

public:
	SKSETaskRefreshTintMask(Actor * actor, BSFixedString ddsPath);

	UInt32			m_formId;
	BSFixedString	m_ddsPath;
};

class SKSEUpdateFaceModel : public TaskDelegate
{
	virtual void Run();
	virtual void Dispose() { delete this; }

public:
	SKSEUpdateFaceModel(Actor * actor);

	UInt32			m_formId;
};


TESForm* GetWornForm(Actor* thisActor, UInt32 mask);
TESForm* GetSkinForm(Actor* thisActor, UInt32 mask);
BSGeometry* GetFirstShaderType(NiAVObject* object, UInt32 shaderType);

class NiAVObjectVisitor
{
public:
	virtual bool Accept(NiAVObject* object) = 0;
};

class NiExtraDataFinder : public NiAVObjectVisitor
{
public:
	NiExtraDataFinder::NiExtraDataFinder(BSFixedString name) : m_name(name), m_data(NULL) { }

	virtual bool Accept(NiAVObject* object);

	NiExtraData* m_data;
	BSFixedString m_name;
};

void VisitBipedNodes(TESObjectREFR* refr, std::function<void(bool, UInt32, NiNode*, NiAVObject*)> functor);
void VisitEquippedNodes(Actor* actor, UInt32 slotMask, std::function<void(TESObjectARMO*, TESObjectARMA*, NiAVObject*, bool)> functor);
void VisitAllWornItems(Actor* thisActor, UInt32 slotMask, std::function<void(InventoryEntryData*)> functor);

void VisitSkeletalRoots(TESObjectREFR* ref, std::function<void(NiNode*, bool)> functor);
void VisitArmorAddon(Actor* actor, TESObjectARMO* armor, TESObjectARMA* arma, std::function<void(bool, NiNode*, NiAVObject*)> functor);
NiExtraData* FindExtraData(NiAVObject* object, BSFixedString name);

bool ResolveAnyForm(SKSESerializationInterface* intfc, UInt32 handle, UInt32* newHandle);
bool ResolveAnyHandle(SKSESerializationInterface* intfc, UInt64 handle, UInt64* newHandle);

TESObjectARMO* GetActorSkin(Actor* actor);
BGSTextureSet * GetTextureSetForPart(TESNPC * npc, BGSHeadPart * headPart);
std::pair<BGSTextureSet*, BGSHeadPart*> GetTextureSetForPartByName(TESNPC * npc, BSFixedString partName);
BSGeometry * GetHeadGeometry(Actor * actor, UInt32 partType);
void ExportTintMaskDDS(Actor * actor, BSFixedString filePath);
NiAVObject * GetObjectByHeadPart(BSFaceGenNiNode * faceNode, BGSHeadPart * headPart);

bool SaveRenderedDDS(NiTexture * pkTexture, const char * pcFileName);

bool VisitObjects(NiAVObject * parent, std::function<bool(NiAVObject*)> functor);
bool VisitGeometry(NiAVObject* parent, std::function<bool(BSGeometry*)> functor);

class GeometryVisitor
{
public:
	virtual bool Accept(BSGeometry* geometry) = 0;
};

bool VisitGeometry(NiAVObject* object, GeometryVisitor* visitor);

NiTransform GetGeometryTransform(BSGeometry * geometry);
NiTransform GetLegacyGeometryTransform(NiGeometry * geometry);

UInt16	GetStripLengthSum(NiTriStripsData * strips);
void GetTriangleIndices(NiTriStripsData * strips, UInt16 i, UInt16 v0, UInt16 v1, UInt16 v2);

NiAVObjectPtr GetRootNode(NiAVObjectPtr object, bool refRoot = false);

class NifStreamWrapper
{
public:
	NifStreamWrapper();
	~NifStreamWrapper();

	bool LoadStream(NiBinaryStream* stream);
	bool VisitObjects(std::function<bool(NiObject*)> functor);

protected:
	UInt8 mem[sizeof(NiStream)];
};