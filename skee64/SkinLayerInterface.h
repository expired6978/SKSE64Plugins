#pragma once

#include "IPluginInterface.h"
#include "StringTable.h"
#include "CDXTextureRenderer.h"
#include <unordered_map>

struct SKSESerializationInterface;
class TESObjectREFR;

namespace SkinLayerData
{

struct Data
{
	StringTableItem texturePath;
	StringTableItem blendMode;
	CDXTextureRenderer::TextureType textureType;
	UInt32 color;
};

namespace ChangeFlags
{
	static const int TEXTURE_PATH		= (1 << 0);
	static const int BLEND_MODE			= (1 << 1);
	static const int TEXTURE_TYPE		= (1 << 2);
	static const int COLOR				= (1 << 3);
};

enum class Target : UInt8
{
	Face = 0,
	Body,
	Hands,
	Feet,
	Hair
};

class LayerMap : public std::map<SInt32, Data>
{
public:
	void Save(SKSESerializationInterface* intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap& stringTable);
};

class TargetToLayer : public std::unordered_map<Target, LayerMap>
{
public:
	void Save(SKSESerializationInterface* intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap& stringTable);
};

class PerspectiveToTarget : public std::unordered_map<bool, TargetToLayer>
{
public:
	void Save(SKSESerializationInterface* intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap& stringTable);
};

class GenderToPersepective : public std::unordered_map<bool, PerspectiveToTarget>
{
public:
	void Save(SKSESerializationInterface* intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap& stringTable);
};

class ActorToGender : public std::unordered_map<UInt32, GenderToPersepective>
{
public:
	void Save(SKSESerializationInterface* intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap& stringTable);
};
};

class SkinLayerInterface
	: public IPluginInterface
{
public:
	enum
	{
		kCurrentPluginVersion = 1,
		kSerializationVersion1 = 1,
		kSerializationVersion = kSerializationVersion1
	};

	virtual skee_u32 GetVersion();

	// Serialization
	void Save(SKSESerializationInterface* intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface* intfc, UInt32 kVersion, const StringIdMap& stringTable);
	virtual void Revert() override;

	virtual void SetSkinLayerTexture(TESObjectREFR* refr, bool isFemale, bool isFirstPerson, SkinLayerData::Target target, SInt32 layer, const char* texturePath);
	virtual void SetSkinLayerBlendMode(TESObjectREFR* refr, bool isFemale, bool isFirstPerson, SkinLayerData::Target target, SInt32 layer, const char* blendMode);
	virtual void SetSkinLayerTextureType(TESObjectREFR* refr, bool isFemale, bool isFirstPerson, SkinLayerData::Target target, SInt32 layer, const UInt8 textureType);
	virtual void SetSkinLayerColor(TESObjectREFR* refr, bool isFemale, bool isFirstPerson, SkinLayerData::Target target, SInt32 layer, const UInt32 color);
private:
	SafeDataHolder<SkinLayerData::ActorToGender> data;
};