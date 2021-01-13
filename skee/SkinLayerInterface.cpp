#include "SkinLayerInterface.h"
#include "skse64/GameReferences.h"
#include "skse64/NiGeometry.h"
#include "skse64/GameRTTI.h"
#include "NifUtils.h"

extern StringTable						g_stringTable;

UInt32 SkinLayerInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void SkinLayerInterface::Save(SKSESerializationInterface* intfc, UInt32 kVersion)
{
}

bool SkinLayerInterface::Load(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap& stringTable)
{
	return false;
}

void SkinLayerInterface::Revert()
{
}

void SkinLayerInterface::SetSkinLayerTexture(TESObjectREFR* refr, bool isFemale, bool isFirstPerson, SkinLayerData::Target target, SInt32 layer, const char* texturePath)
{
	data.Lock();
	data.m_data[refr->formID][isFemale][isFirstPerson][target][layer].texturePath = g_stringTable.GetString(texturePath);
	data.Release();
}

void SkinLayerInterface::SetSkinLayerBlendMode(TESObjectREFR* refr, bool isFemale, bool isFirstPerson, SkinLayerData::Target target, SInt32 layer, const char* blendMode)
{
	data.Lock();
	data.m_data[refr->formID][isFemale][isFirstPerson][target][layer].blendMode = g_stringTable.GetString(blendMode);
	data.Release();
}

void SkinLayerInterface::SetSkinLayerTextureType(TESObjectREFR* refr, bool isFemale, bool isFirstPerson, SkinLayerData::Target target, SInt32 layer, const UInt8 textureType)
{
	data.Lock();
	data.m_data[refr->formID][isFemale][isFirstPerson][target][layer].textureType = static_cast<CDXTextureRenderer::TextureType>(textureType);
	data.Release();
}

void SkinLayerInterface::SetSkinLayerColor(TESObjectREFR* refr, bool isFemale, bool isFirstPerson, SkinLayerData::Target target, SInt32 layer, const UInt32 color)
{
	data.Lock();
	data.m_data[refr->formID][isFemale][isFirstPerson][target][layer].color = color;
	data.Release();
}
