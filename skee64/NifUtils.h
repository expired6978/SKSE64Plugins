#pragma once

#include <RE/RTTI.h>
#include <RE/B/BSFixedString.h>
#include <RE/N/NiPoint3.h>
#include <RE/N/NiColor.h>
#include <RE/N/NiSmartPointer.h>
#include <RE/N/NiNode.h>
#include <RE/N/NiAVObject.h>
#include <RE/B/BSGeometry.h>
#include <RE/N/NiExtraData.h>
#include <RE/N/NiStream.h>
#include <RE/N/NiBinaryStream.h>
#include <SKSE/API.h>
#include <SKSE/Interfaces.h>

#include <functional>
#include <cstdint>
namespace RE
{
	class RE::BGSHeadPart;
	class RE::BSFaceGenNiNode;
	class RE::BSGeometry;
	class RE::TESNPC;
}

class NiTriStripsData;
class NiBinaryStream;
class InventoryEntryData;
class TESObjectARMO;
class TESObjectARMA;

class SKSETaskExportHead : public SKSE::detail::TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose() { delete this; }

	SKSETaskExportHead(RE::Actor* actor, RE::BSFixedString nifPath, RE::BSFixedString ddsPath);

	std::uint32_t			m_formId;
	RE::BSFixedString		m_nifPath;
	RE::BSFixedString		m_ddsPath;
};

class SKSETaskExportTintMask : public SKSE::detail::TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose() { delete this; };

	SKSETaskExportTintMask(RE::BSFixedString filePath, RE::BSFixedString fileName) : m_filePath(filePath), m_fileName(fileName) {};

	RE::BSFixedString		m_filePath;
	RE::BSFixedString		m_fileName;
};

class SKSETaskRefreshTintMask : public SKSE::detail::TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose() { delete this; };

	SKSETaskRefreshTintMask(RE::Actor* actor, RE::BSFixedString ddsPath);

	std::uint32_t			m_formId;
	RE::BSFixedString		m_ddsPath;
};

class SKSEUpdateFaceModel : public SKSE::detail::TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose() { delete this; }

	SKSEUpdateFaceModel(RE::Actor* actor);

	std::uint32_t			m_formId;
};

RE::TESForm* GetWornForm(RE::Actor* thisActor, std::uint32_t mask);
RE::TESForm* GetSkinForm(RE::Actor* thisActor, std::uint32_t mask);
RE::BSGeometry* GetFirstShaderType(RE::NiAVObject* object, std::uint32_t shaderType);

class NiAVObjectVisitor
{
public:
	virtual bool Accept(RE::NiAVObject* object) = 0;
};

class NiExtraDataFinder : public NiAVObjectVisitor
{
public:
	NiExtraDataFinder(RE::BSFixedString name) : m_name(name), m_data(nullptr) { }

	virtual bool Accept(RE::NiAVObject* object);

	RE::NiExtraData* m_data;
	RE::BSFixedString m_name;
};

void VisitBipedNodes(RE::TESObjectREFR* refr, std::function<void(bool, std::uint32_t, RE::NiNode*, RE::TESForm*, RE::TESForm*, RE::NiAVObject*)> functor);
void VisitEquippedNodes(RE::Actor* actor, std::uint32_t slotMask, std::function<void(RE::TESObjectARMO*, RE::TESObjectARMA*, RE::NiAVObject*, bool)> functor);
void VisitAllWornItems(RE::Actor* thisActor, std::uint32_t slotMask, std::function<void(RE::InventoryEntryData*)> functor);

void VisitSkeletalRoots(RE::TESObjectREFR* ref, std::function<void(RE::NiNode*, bool)> functor);
void VisitArmorAddon(RE::Actor* actor, RE::TESObjectARMO* armor, RE::TESObjectARMA* arma, std::function<void(bool, RE::NiNode*, RE::NiAVObject*)> functor);
RE::NiExtraData* FindExtraData(RE::NiAVObject* object, RE::BSFixedString name);

bool ResolveAnyForm(SKSE::SerializationInterface* intfc, std::uint32_t handle, std::uint32_t* newHandle);
bool ResolveAnyHandle(SKSE::SerializationInterface* intfc, std::uint64_t handle, std::uint64_t* newHandle);

bool IsSlotMatch(RE::TESForm* pForm, std::uint32_t mask);

RE::TESObjectARMO* GetActorSkin(RE::Actor* actor);
RE::BGSTextureSet* GetTextureSetForPart(RE::TESNPC* npc, RE::BGSHeadPart* headPart);
std::pair<RE::BGSTextureSet*, RE::BGSHeadPart*> GetTextureSetForPartByName(RE::TESNPC* npc, RE::BSFixedString partName);
RE::BSGeometry* GetHeadGeometry(RE::Actor* actor, std::uint32_t partType);
void ExportTintMaskDDS(RE::Actor* actor, RE::BSFixedString filePath);
RE::NiAVObject* GetObjectByHeadPart(RE::BSFaceGenNiNode* faceNode, RE::BGSHeadPart* headPart);

bool SaveRenderedDDS(RE::NiTexture* pkTexture, const char* pcFileName);

bool VisitObjects(RE::NiAVObject* parent, std::function<bool(RE::NiAVObject*)> functor);
bool VisitGeometry(RE::NiAVObject* parent, std::function<bool(RE::BSGeometry*)> functor);

// Legacy TESObjectREFR::GetFaceGenNiNode (vfunc 0x61). CommonLib exposes the same
// slot as Character::GetFaceNodeSkinned() (0x61); Actor derives from Character.
inline RE::BSFaceGenNiNode* GetFaceGenNiNode(RE::Actor* a_actor)
{
	if (!a_actor)
		return nullptr;
	return a_actor->GetFaceNodeSkinned();
}

class GeometryVisitor
{
public:
	virtual bool Accept(RE::BSGeometry* geometry) = 0;
};

bool VisitGeometry(RE::NiAVObject* object, GeometryVisitor* visitor);

RE::NiTransform GetGeometryTransform(RE::BSGeometry* geometry);
RE::NiTransform GetLegacyGeometryTransform(RE::NiGeometry* geometry);

std::uint16_t GetStripLengthSum(RE::NiTriStripsData* strips);
void GetTriangleIndices(RE::NiTriStripsData* strips, std::uint16_t i, std::uint16_t& v0, std::uint16_t& v1, std::uint16_t& v2);

RE::NiAVObject* GetRootNode(RE::NiAVObject* object, bool refRoot = false);

class NiStringsExtraDataHelper
{
public:
	static void copy_string(char*& a_value, const char* a_copyValue);
	static void Replace(RE::NiStringsExtraData* _this, std::vector<RE::BSFixedString>& strings);
};

class NifStreamWrapper
{
public:
	NifStreamWrapper();
	~NifStreamWrapper();

	bool LoadStream(RE::NiBinaryStream* stream);
	bool VisitObjects(std::function<bool(RE::NiObject*)> functor);

	// mem is a byte buffer standing in for a NiStream; the game API writes
	// through it, so expose a non-const pointer from this const accessor.
	RE::NiStream* get() const { return reinterpret_cast<RE::NiStream*>(const_cast<std::uint8_t*>(&mem[0])); }
	RE::NiStream* operator->() const { return get(); }

protected:
	std::uint8_t mem[sizeof(RE::NiStream)];
};