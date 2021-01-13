#pragma once

#include "skse64/GameTypes.h"
#include "skse64/GameThreads.h"

#include "skse64/NiGeometry.h"
#include "skse64/NiNodes.h"
#include "skse64/NiTypes.h"

#include "StringTable.h"

class BSGeometry;
class OverrideVariant;
class BSLightingShaderMaterial;

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

	virtual void Run();
	virtual void Dispose() { delete this; }

	NiPointer<NiAVObject> m_object;
};


class NIOVTaskMoveNode : public TaskDelegate
{
public:
	NIOVTaskMoveNode(NiPointer<NiNode> destination, NiPointer<NiAVObject> object) : m_object(object), m_destination(destination) { }

	virtual void Run();
	virtual void Dispose() { delete this; }

	NiPointer<NiAVObject> m_object;
	NiPointer<NiNode> m_destination;
};

void GetShaderProperty(NiAVObject * node, OverrideVariant * value);
void SetShaderProperty(NiAVObject * node, OverrideVariant * value, bool immediate);


SKEEFixedString GetSanitizedPath(const SKEEFixedString & path);
NiTexturePtr * GetTextureFromIndex(BSLightingShaderMaterial* material, UInt32 index);

#ifdef _DEBUG
void DumpNodeChildren(NiAVObject * node);
#endif