#pragma once

#include "IPluginInterface.h"
#include "StringTable.h"
#include "OverrideVariant.h"
#include <vector>

class TESNPC;
class BGSHeadPart;
class BSFaceGenNiNode;
class Actor;
class TESRace;

class SculptData;
using SculptDataPtr = std::shared_ptr<SculptData>;

class PresetData
{
public:
	PresetData();

	struct Tint
	{
		UInt32 index;
		UInt32 color;
		SKEEFixedString name;
	};

	struct Morph
	{
		float value;
		SKEEFixedString name;
	};

	struct Texture
	{
		UInt8 index;
		SKEEFixedString name;
	};

	float weight;
	UInt32 hairColor;
	std::vector<std::string> modList;
	std::vector<BGSHeadPart*> headParts;
	std::vector<SInt32> presets;
	std::vector<float> morphs;
	std::vector<Tint> tints;
	std::vector<Morph> customMorphs;
	std::vector<Texture> faceTextures;
	BGSTextureSet* headTexture;
	BSFixedString tintTexture;
	typedef std::map<SKEEFixedString, std::vector<OverrideVariant>> OverrideData;
	OverrideData overrideData;
	typedef std::map<UInt32, std::vector<OverrideVariant>> SkinData;
	SkinData skinData[2];
	typedef std::map<SKEEFixedString, std::map<SKEEFixedString, std::vector<OverrideVariant>>> TransformData;
	TransformData transformData[2];
	SculptDataPtr sculptData;
	typedef std::unordered_map<SKEEFixedString, std::unordered_map<SKEEFixedString, float>> BodyMorphData;
	BodyMorphData bodyMorphData;
};
using PresetDataPtr = std::shared_ptr<PresetData>;
class PresetMap : public SafeDataHolder<std::unordered_map<TESNPC*, PresetDataPtr>>
{
public:
	friend class PresetInterface;
};

class PresetInterface : public IPresetInterface
{
public:
    virtual skee_u32 GetVersion() override { return kCurrentPluginVersion; }
    virtual void Revert() { };

	PresetDataPtr GetMappedPreset(TESNPC* npc);
	void AssignMappedPreset(TESNPC* npc, PresetDataPtr presetData);
	void ApplyMappedPreset(TESNPC* npc, BSFaceGenNiNode* faceNode, BGSHeadPart* headPart);
	bool EraseMappedPreset(TESNPC* npc);
	void ClearMappedPresets();

	// Legacy
	bool SaveBinaryPreset(const char* filePath);
	bool LoadBinaryPreset(const char* filePath, PresetDataPtr presetData);

	enum ApplyTypes
	{
		kPresetApplyFace = (0 << 0),
		kPresetApplyOverrides = (1 << 0),
		kPresetApplyBodyMorphs = (1 << 1),
		kPresetApplyTransforms = (1 << 2),
		kPresetApplySkinOverrides = (1 << 3),
		kPresetApplyAll = kPresetApplyFace | kPresetApplyOverrides | kPresetApplyBodyMorphs | kPresetApplyTransforms | kPresetApplySkinOverrides
	};

	void ApplyPresetData(Actor* actor, PresetDataPtr presetData, bool setSkinColor = false, ApplyTypes applyType = kPresetApplyAll);
	void ApplyPreset(Actor* actor, TESRace* race, TESNPC* npc, PresetDataPtr presetData, ApplyTypes applyType);

	bool SaveJsonPreset(const char* filePath);
	bool LoadJsonPreset(const char* filePath, PresetDataPtr presetData);

protected:
	PresetMap m_mappedPreset;
};