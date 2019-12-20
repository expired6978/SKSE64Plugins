#pragma once

#include "skse64/GameTypes.h"
#include "skse64/GameThreads.h"
#include "skse64/GameObjects.h"

#include "skse64/NiGeometry.h"
#include "skse64/NiNodes.h"
#include "skse64/NiTypes.h"
#include "skse64/NiMaterial.h"

#include "StringTable.h"

#include <functional>
#include <unordered_set>

class NiExtraData;
class BSGeometry;
class OverrideVariant;
class BGSLightingShaderProperty;

struct SKSESerializationInterface;

class NIOVTaskUpdateTexture : public TaskDelegate
{
public:
	NIOVTaskUpdateTexture(NiPointer<BSGeometry> geometry, UInt32 index, StringTableItem texture) : m_geometry(geometry), m_index(index), m_texture(texture) { }

	virtual void Run();
	virtual void Dispose() { delete this; }

	NiPointer<BSGeometry> m_geometry;
	UInt32			m_index;
	StringTableItem	m_texture;
};

class NIOVTaskUpdateWorldData : public TaskDelegate
{
public:
	NIOVTaskUpdateWorldData(NiPointer<NiAVObject> object) : m_object(object) { }

	virtual void Run()
	{
#if 0
		NiAVObject::ControllerUpdateContext ctx;
		ctx.flags = 0;
		ctx.delta = 0;
		m_object->UpdateWorldData(&ctx);
#endif
	}
	virtual void Dispose() { delete this; }

	NiPointer<NiAVObject> m_object;
};


class NIOVTaskMoveNode : public TaskDelegate
{
public:
	NIOVTaskMoveNode(NiPointer<NiNode> destination, NiPointer<NiAVObject> object) : m_object(object), m_destination(destination) { }

	virtual void Run()
	{
		NiPointer<NiNode> currentParent = m_object->m_parent;
		if (currentParent)
			currentParent->RemoveChild(m_object);
		if (m_destination)
			m_destination->AttachChild(m_object, true);
	}
	virtual void Dispose() { delete this; }

	NiPointer<NiAVObject> m_object;
	NiPointer<NiNode> m_destination;
};

void GetShaderProperty(NiAVObject * node, OverrideVariant * value);
void SetShaderProperty(NiAVObject * node, OverrideVariant * value, bool immediate);

TESForm* GetWornForm(Actor* thisActor, UInt32 mask);
TESForm* GetSkinForm(Actor* thisActor, UInt32 mask);
BSGeometry * GetFirstShaderType(NiAVObject * object, UInt32 shaderType);

class GeometryVisitor
{
public:
	virtual bool Accept(BSGeometry * geometry) = 0;
};

void VisitGeometry(NiAVObject * object, GeometryVisitor * visitor);

class NiAVObjectVisitor
{
public:
	virtual bool Accept(NiAVObject * object) = 0;
};

class NiExtraDataFinder : public NiAVObjectVisitor
{
public:
	NiExtraDataFinder::NiExtraDataFinder(BSFixedString name) : m_name(name), m_data(NULL) { }

	virtual bool Accept(NiAVObject * object);

	NiExtraData * m_data;
	BSFixedString m_name;
};

void VisitEquippedNodes(Actor* actor, UInt32 slotMask, std::function<void(TESObjectARMO*, TESObjectARMA*, NiAVObject*, bool)> functor);
void VisitAllWornItems(Actor* thisActor, UInt32 slotMask, std::function<void(InventoryEntryData*)> functor);

void VisitArmorAddon(Actor * actor, TESObjectARMO * armor, TESObjectARMA * arma, std::function<void(bool, NiNode*,NiAVObject*)> functor);
NiExtraData * FindExtraData(NiAVObject * object, BSFixedString name);

bool ResolveAnyForm(SKSESerializationInterface * intfc, UInt32 handle, UInt32 * newHandle);
bool ResolveAnyHandle(SKSESerializationInterface * intfc, UInt64 handle, UInt64 * newHandle);

SKEEFixedString GetSanitizedPath(const SKEEFixedString & path);
NiTexturePtr * GetTextureFromIndex(BSLightingShaderMaterial* material, UInt32 index);

#ifdef _DEBUG
void DumpNodeChildren(NiAVObject * node);
#endif