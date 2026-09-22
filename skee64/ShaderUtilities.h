#pragma once

#include "RE/B/BSFixedString.h"
#include "RE/N/NiSmartPointer.h"
#include "RE/N/NiPoint3.h"
#include "RE/N/NiColor.h"
#include "RE/N/NiNode.h"
#include "RE/N/NiAVObject.h"
#include "RE/N/NiSourceTexture.h"
#include "RE/B/BSGeometry.h"
#include "RE/B/BSShaderProperty.h"
#include "RE/B/BSLightingShaderProperty.h"
#include "RE/B/BSLightingShaderMaterial.h"
#include "RE/B/BSShaderTextureSet.h"
#include "RE/T/TESForm.h"
#include "SKSE/API.h"

#include "StringTable.h"
#include "OverrideVariant.h"
#include <cstdint>


class NIOVTaskUpdateTexture : public SKEETaskDelegate
{
public:
	NIOVTaskUpdateTexture(RE::NiPointer<RE::BSGeometry> geometry, std::uint32_t index, StringTableItem texture) : m_geometry(geometry), m_index(index), m_texture(texture) { }

	virtual void Run();
	virtual void Dispose() { delete this; }

	RE::NiPointer<RE::BSGeometry> m_geometry;
	std::uint32_t			m_index;
	StringTableItem	m_texture;
};

class NIOVTaskUpdateWorldData : public SKEETaskDelegate
{
public:
	NIOVTaskUpdateWorldData(RE::NiPointer<RE::NiAVObject> object) : m_object(object) { }

	virtual void Run();
	virtual void Dispose() { delete this; }

	RE::NiPointer<RE::NiAVObject> m_object;
};

class NIOVTaskMoveNode : public SKEETaskDelegate
{
public:
	NIOVTaskMoveNode(RE::NiPointer<RE::NiNode> destination, RE::NiPointer<RE::NiAVObject> object) : m_object(object), m_destination(destination) { }

	virtual void Run();
	virtual void Dispose() { delete this; }

	RE::NiPointer<RE::NiAVObject> m_object;
	RE::NiPointer<RE::NiNode> m_destination;
};

void GetShaderProperty(RE::NiAVObject* node, OverrideVariant* value);
void SetShaderProperty(RE::NiAVObject* node, OverrideVariant* value, bool immediate);

class NIOVTaskSetShaderProperty : public SKEETaskDelegate
{
public:
	NIOVTaskSetShaderProperty(RE::NiAVObject* node, const OverrideVariant& variant) : m_object(node), m_variant(variant) { }

	virtual void Run()
	{
		SetShaderProperty(m_object.get(), &m_variant, true);
	}

	virtual void Dispose() { delete this; }

protected:
	RE::NiPointer<RE::NiAVObject> m_object;
	OverrideVariant m_variant;
};

SKEEFixedString GetSanitizedPath(const SKEEFixedString& path);
RE::NiPointer<RE::NiSourceTexture>* GetTextureFromIndex(RE::BSLightingShaderMaterial* material, std::uint32_t index);

void DumpNodeChildren(RE::NiAVObject* node);
