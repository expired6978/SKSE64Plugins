#pragma once

#include "skse64/GameTypes.h"
#include "skse64/GameThreads.h"
#include "skse64/GameObjects.h"

#include "skse64/NiNodes.h"
#include "skse64/NiTypes.h"

#include <functional>

class NiExtraData;
class BSGeometry;
class OverrideVariant;

struct SKSESerializationInterface;

class NIOVTaskUpdateTexture : public TaskDelegate
{
public:
	NIOVTaskUpdateTexture(BSGeometry * geometry, UInt32 index, BSFixedString texture);

	virtual void Run();
	virtual void Dispose();

	BSGeometry		* m_geometry;
	UInt32			m_index;
	BSFixedString	m_texture;
};

class NIOVTaskUpdateWorldData : public TaskDelegate
{
public:
	NIOVTaskUpdateWorldData(NiAVObject * object)
	{
		object->IncRef();
		m_object = object;
	}

	virtual void Run()
	{
		NiAVObject::ControllerUpdateContext ctx;
		ctx.flags = 0;
		ctx.delta = 0;
		//m_object->UpdateWorldData(&ctx);
		CALL_MEMBER_FN(m_object, UpdateNode)(&ctx);
	}
	virtual void Dispose()
	{
		m_object->DecRef();
		delete this;
	}

	NiAVObject * m_object;
};


class NIOVTaskMoveNode : public TaskDelegate
{
public:
	NIOVTaskMoveNode(NiNode * destination, NiAVObject * object)
	{
		object->IncRef();
		m_object = object;
		m_destination = destination;
	}

	virtual void Run()
	{
		NiNode * currentParent = m_object->m_parent;
		if (currentParent)
			currentParent->RemoveChild(m_object);
		if (m_destination)
			m_destination->AttachChild(m_object, true);
	}
	virtual void Dispose()
	{
		m_object->DecRef();
		delete this;
	}

	NiAVObject * m_object;
	NiNode * m_destination;
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

void VisitArmorAddon(Actor * actor, TESObjectARMO * armor, TESObjectARMA * arma, std::function<void(bool, NiNode*,NiAVObject*)> functor);
NiExtraData * FindExtraData(NiAVObject * object, BSFixedString name);

bool ResolveAnyHandle(SKSESerializationInterface * intfc, UInt64 handle, UInt64 * newHandle);

class NiAutoRefCounter
{
public:
	NiAutoRefCounter(NiObject * object)
	{
		m_object = object;
		m_object->IncRef();
	}
	~NiAutoRefCounter()
	{
		m_object->DecRef();
	}

private:
	NiObject * m_object;
};

#ifdef _DEBUG
void DumpNodeChildren(NiAVObject * node);
#endif