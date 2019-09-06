#pragma once

#include "IPluginInterface.h"
#include "IHashType.h"
#include "ItemDataInterface.h"
#include "CDXNifTextureRenderer.h"

struct SKSESerializationInterface;

class TESObjectREFR;
class TESObjectARMO;
class TESObjectARMA;
class NiAVObject;
class TintMask;

class NiStringsExtraData;
class NiIntegersExtraData;
class NiFloatsExtraData;
class NiIntegerExtraData;

class BSLightingShaderProperty;
class BSRenderTargetGroup;

#include "skse64/NiMaterial.h"
#include "skse64/GameTypes.h"

#include <unordered_map>
#include <functional>
#include <memory>

struct ShaderHasher
{
	std::size_t operator()(const NiPointer<BSLightingShaderProperty>& k) const
	{
		return (size_t)k.m_pObject;
	}
};

typedef std::unordered_map<NiPointer<BSLightingShaderProperty>, std::unordered_map<SInt32, std::shared_ptr<CDXNifTextureRenderer>>, ShaderHasher> TintMaskCacheMap;

class TintMaskMap : public SafeDataHolder<TintMaskCacheMap>
{
public:
	void ManageRenderTargetGroups();
	std::shared_ptr<CDXNifTextureRenderer> GetRenderTarget(BSLightingShaderProperty* key, SInt32 index);
	void AddRenderTargetGroup(BSLightingShaderProperty* key, SInt32 index, std::shared_ptr<CDXNifTextureRenderer> value);
	void ReleaseRenderTargetGroups();

	bool IsCaching() const { return m_caching; }

private:
	bool m_caching;
};

typedef std::unordered_map<SInt32, SKEEFixedString>	LayerTextureMap;
typedef std::unordered_map<SInt32, SInt32>			LayerColorMap;
typedef std::unordered_map<SInt32, float>			LayerAlphaMap;
typedef std::unordered_map<SInt32, SKEEFixedString>	LayerBlendMap;
typedef std::unordered_map<SInt32, UInt8>			TextureTypeMap;

struct TextureLayer
{
	LayerTextureMap textures;
	LayerColorMap colors;
	LayerAlphaMap alphas;
	LayerBlendMap blendModes;
	TextureTypeMap types;
};

// maps diffuse names to layer data
class TextureLayerMap : public std::unordered_map<SKEEFixedString, TextureLayer>
{
public:
	TextureLayer * GetTextureLayer(SKEEFixedString texture);
};

// maps trishape names to diffuse names
class MaskTriShapeMap : public std::unordered_map<SKEEFixedString, TextureLayerMap>
{
public:
	TextureLayerMap * GetTextureMap(SKEEFixedString triShape);
};


typedef std::unordered_map<SKEEFixedString, MaskTriShapeMap> MaskModelContainer;

// Maps model names to trishape names
class MaskModelMap : public SafeDataHolder<MaskModelContainer>
{
public:
	TextureLayer * GetMask(SKEEFixedString nif, SKEEFixedString trishape, SKEEFixedString diffuse);

	void ApplyLayers(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMA * arma, NiAVObject * node, std::function<void(NiPointer<BSGeometry>, SInt32, TextureLayer*)> functor);
	MaskTriShapeMap * GetTriShapeMap(SKEEFixedString nifPath);
};

class TintMaskInterface : public IPluginInterface
{
public:
	enum
	{
		kCurrentPluginVersion = 0,
		kSerializationVersion1 = 1,
		kSerializationVersion = kSerializationVersion1
	};
	virtual UInt32 GetVersion();

	enum ColorPreset
	{
		kPreset_Skin = -2,
		kPreset_Hair = -1
	};
	enum UpdateFlags
	{
		kUpdate_Skin = 1,
		kUpdate_Hair = 1 << 1,
		kUpdate_All = kUpdate_Skin | kUpdate_Hair
	};
	
	struct LayerTarget
	{
		NiPointer<BSGeometry>			object;
		UInt32							targetIndex;
		LayerTextureMap					textureData;
		LayerColorMap					colorData;
		LayerAlphaMap					alphaData;
		LayerBlendMap					blendData;
		TextureTypeMap					typeData;
	};
	typedef std::vector<LayerTarget> LayerTargetList;

	virtual void ApplyMasks(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, std::function<std::shared_ptr<ItemAttributeData>()> overrides, UInt32 flags);
	virtual void ManageTints() { m_maskMap.ManageRenderTargetGroups(); }
	virtual void ReleaseTints() { m_maskMap.ReleaseRenderTargetGroups(); }
	virtual void Revert() { };

	void CreateTintsFromData(TESObjectREFR * refr, std::map<SInt32, CDXNifTextureRenderer::MaskData> & masks, const LayerTarget & layerTarget, std::shared_ptr<ItemAttributeData> & overrides, UInt32 & flags);
	void ParseTintData(LPCTSTR filePath);

	MaskModelMap	m_modelMap;
	TintMaskMap		m_maskMap;
};

class NIOVTaskDeferredMask : public TaskDelegate
{
public:
	NIOVTaskDeferredMask::NIOVTaskDeferredMask(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, std::function<std::shared_ptr<ItemAttributeData>()> overrides);

	virtual void Run();
	virtual void Dispose();

private:
	bool							m_firstPerson;
	UInt32							m_formId;
	UInt32							m_armorId;
	UInt32							m_addonId;
	NiPointer<NiAVObject>			m_object;
	std::function<std::shared_ptr<ItemAttributeData>()>	m_overrides;
};