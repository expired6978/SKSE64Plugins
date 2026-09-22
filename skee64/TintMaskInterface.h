#pragma once

#include "IPluginInterface.h"
#include "SafeDataHolder.h"
#include "IHashType.h"

#include "CDXNifTextureRenderer.h"

#include <RE/B/BSGeometry.h>
#include <RE/B/BSLightingShaderProperty.h>
#include <RE/B/BSShaderRenderTargets.h>
#include <RE/N/NiAVObject.h>
#include <RE/N/NiColor.h>
#include <RE/N/NiNode.h>
#include <RE/N/NiSmartPointer.h>
#include <RE/T/TESObjectARMA.h>
#include <RE/T/TESObjectARMO.h>

#include <SKSE/Events.h>

#include <unordered_map>
#include <functional>
#include <memory>
#include <cstdint>
class TintMask;
class ItemAttributeData;
typedef std::shared_ptr<ItemAttributeData> ItemAttributeDataPtr;

struct ShaderHasher
{
	std::size_t operator()(const RE::NiPointer<RE::BSLightingShaderProperty>& k) const
	{
		return (size_t)k.get();
	}
};

typedef std::unordered_map<RE::NiPointer<RE::BSLightingShaderProperty>, std::unordered_map<std::int32_t, std::shared_ptr<CDXNifTextureRenderer>>, ShaderHasher> TintMaskCacheMap;

class TintMaskMap : public SafeDataHolder<TintMaskCacheMap>
{
public:
	void ManageRenderTargetGroups();
	std::shared_ptr<CDXNifTextureRenderer> GetRenderTarget(RE::BSLightingShaderProperty* key, std::int32_t index);
	void AddRenderTargetGroup(RE::BSLightingShaderProperty* key, std::int32_t index, std::shared_ptr<CDXNifTextureRenderer> value);
	void ReleaseRenderTargetGroups();

	bool IsCaching() const { return m_caching; }

private:
	bool m_caching;
};

typedef std::unordered_map<std::int32_t, SKEEFixedString>	LayerTextureMap;
typedef std::unordered_map<std::int32_t, std::int32_t>			LayerColorMap;
typedef std::unordered_map<std::int32_t, float>			LayerAlphaMap;
typedef std::unordered_map<std::int32_t, SKEEFixedString>	LayerBlendMap;
typedef std::unordered_map<std::int32_t, std::uint8_t>			TextureTypeMap;
typedef std::unordered_multimap<std::int32_t, std::int32_t>		LayerSlotMap;

struct TextureLayer
{
	LayerTextureMap textures;
	LayerColorMap colors;
	LayerAlphaMap alphas;
	LayerBlendMap blendModes;
	TextureTypeMap types;
	LayerSlotMap slots;
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

	bool IsRemappable() const { return m_remappable; }
	void SetRemappable(bool remap) { m_remappable = remap; }

protected:
	bool m_remappable;
};

typedef std::unordered_map<SKEEFixedString, MaskTriShapeMap> MaskModelContainer;

// Maps model names to trishape names
class MaskModelMap : public SafeDataHolder<MaskModelContainer>
{
public:
	TextureLayer * GetMask(SKEEFixedString nif, SKEEFixedString trishape, SKEEFixedString diffuse);

	SKEEFixedString GetModelPath(std::uint8_t gender, bool isFirstPerson, RE::TESObjectARMO * armor, RE::TESObjectARMA * arma);
	void ApplyLayers(RE::TESObjectREFR * refr, bool isFirstPerson, RE::TESObjectARMO * armor, RE::TESObjectARMA * arma, RE::NiAVObject * node, std::function<void(RE::NiPointer<RE::BSGeometry>, std::int32_t, TextureLayer*)> functor);
	MaskTriShapeMap * GetTriShapeMap(SKEEFixedString nifPath);
};

struct LayerTarget
{
	enum TargetFlags
	{
		kTarget_EmissiveColor = 1,
	};

	RE::NiPointer<RE::BSGeometry>			object;
	std::uint32_t							targetIndex;
	std::uint32_t							targetFlags;
	LayerTextureMap					textureData;
	LayerColorMap					colorData;
	LayerAlphaMap					alphaData;
	LayerBlendMap					blendData;
	TextureTypeMap					typeData;
	LayerSlotMap					slots;
};
typedef std::vector<LayerTarget> LayerTargetList;
typedef std::function<void(RE::TESObjectARMO *, RE::TESObjectARMA *, const char*, RE::NiTexturePtr, LayerTarget&)> LayerFunctor;

class TintMaskInterface 
	: public IPluginInterface
	, public IAddonAttachmentInterface
	, public RE::BSTEventSink<SKSE::NiNodeUpdateEvent>
{
public:
	enum
	{
		kCurrentPluginVersion = 0,
		kSerializationVersion1 = 1,
		kSerializationVersion = kSerializationVersion1
	};
	virtual skee_u32 GetVersion();

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

	virtual void ApplyMasks(RE::TESObjectREFR * refr, bool isFirstPerson, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::NiAVObject * object, std::uint32_t flags, ItemAttributeDataPtr overrides, LayerFunctor layer = LayerFunctor());
	virtual void ManageTints() { m_maskMap.ManageRenderTargetGroups(); }
	virtual void ReleaseTints() { m_maskMap.ReleaseRenderTargetGroups(); }
	virtual void Revert() { };

	virtual bool IsDyeable(RE::TESObjectARMO * armor);

	virtual void GetTemplateColorMap(RE::TESObjectREFR* actor, RE::TESObjectARMO * armor, std::map<std::int32_t, std::uint32_t>& colorMap);
	virtual void GetSlotTextureIndexMap(RE::TESObjectREFR* actor, RE::TESObjectARMO* armor, std::map<std::int32_t, std::uint32_t>& slotTextureIndexMap);

	virtual void LoadMods();

	void CreateTintsFromData(RE::TESObjectREFR * refr, std::map<std::int32_t, CDXNifTextureRenderer::MaskData> & masks, const LayerTarget & layerTarget, ItemAttributeDataPtr & overrides, std::uint32_t & flags);
	void ParseTintData(const char* filePath);

protected:
	void VisitTemplateData(RE::TESObjectREFR* refr, RE::TESObjectARMO* armor, std::function<void(MaskTriShapeMap*)> functor);

private:
	bool GetActorHairColor(RE::Actor* actor, RE::NiColorA& color);
	// Inherited via IAddonAttachmentInterface
	virtual void OnAttach(RE::TESObjectREFR * refr, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::NiAVObject * object, bool isFirstPerson, RE::NiNode * skeleton, RE::NiNode * root) override;

	MaskModelMap	m_modelMap;
	TintMaskMap		m_maskMap;

	std::recursive_mutex				m_dyeableLock;
	std::unordered_map<std::uint32_t, bool> m_dyeable;

	// Inherited via RE::BSTEventSink
	virtual RE::BSEventNotifyControl ProcessEvent(const SKSE::NiNodeUpdateEvent* a_event, RE::BSTEventSource<SKSE::NiNodeUpdateEvent>* a_source) override;
};

class NIOVTaskDeferredMask : public SKEETaskDelegate
{
public:
	NIOVTaskDeferredMask(RE::TESObjectREFR * refr, bool isFirstPerson, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::NiAVObject * object, ItemAttributeDataPtr overrides);

	virtual void Run();
	virtual void Dispose();

private:
	bool							m_firstPerson;
	std::uint32_t							m_formId;
	std::uint32_t							m_armorId;
	std::uint32_t							m_addonId;
	RE::NiPointer<RE::NiAVObject>			m_object;
	ItemAttributeDataPtr			m_overrides;
};
