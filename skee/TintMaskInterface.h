#pragma once

#include "IPluginInterface.h"
#include "IHashType.h"

#include "CDXNifTextureRenderer.h"

#include "skse64/NiMaterial.h"
#include "skse64/GameTypes.h"
#include "skse64/GameThreads.h"
#include "skse64/PapyrusEvents.h"

#include <unordered_map>
#include <functional>
#include <memory>

struct SKSESerializationInterface;
struct SKSENiNodeUpdateEvent;

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
class ItemAttributeData;
typedef std::shared_ptr<ItemAttributeData> ItemAttributeDataPtr;

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
typedef std::unordered_multimap<SInt32, SInt32>		LayerSlotMap;

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

	SKEEFixedString GetModelPath(UInt8 gender, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * arma);
	void ApplyLayers(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * arma, NiAVObject * node, std::function<void(NiPointer<BSGeometry>, SInt32, TextureLayer*)> functor);
	MaskTriShapeMap * GetTriShapeMap(SKEEFixedString nifPath);
};

struct LayerTarget
{
	enum TargetFlags
	{
		kTarget_EmissiveColor = 1,
	};

	NiPointer<BSGeometry>			object;
	UInt32							targetIndex;
	UInt32							targetFlags;
	LayerTextureMap					textureData;
	LayerColorMap					colorData;
	LayerAlphaMap					alphaData;
	LayerBlendMap					blendData;
	TextureTypeMap					typeData;
	LayerSlotMap					slots;
};
typedef std::vector<LayerTarget> LayerTargetList;
typedef std::function<void(TESObjectARMO *, TESObjectARMA *, const char*, NiTexturePtr, LayerTarget&)> LayerFunctor;

class TintMaskInterface 
	: public IPluginInterface
	, public IAddonAttachmentInterface
	, public BSTEventSink<SKSENiNodeUpdateEvent>
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

	virtual void ApplyMasks(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, UInt32 flags, ItemAttributeDataPtr overrides, LayerFunctor layer = LayerFunctor());
	virtual void ManageTints() { m_maskMap.ManageRenderTargetGroups(); }
	virtual void ReleaseTints() { m_maskMap.ReleaseRenderTargetGroups(); }
	virtual void Revert() { };

	virtual bool IsDyeable(TESObjectARMO * armor);

	virtual void GetTemplateColorMap(TESObjectREFR* actor, TESObjectARMO * armor, std::map<SInt32, UInt32>& colorMap);
	virtual void GetSlotTextureIndexMap(TESObjectREFR* actor, TESObjectARMO* armor, std::map<SInt32, UInt32>& slotTextureIndexMap);

	virtual void LoadMods();

	void CreateTintsFromData(TESObjectREFR * refr, std::map<SInt32, CDXNifTextureRenderer::MaskData> & masks, const LayerTarget & layerTarget, ItemAttributeDataPtr & overrides, UInt32 & flags);
	void ParseTintData(LPCTSTR filePath);

protected:
	void VisitTemplateData(TESObjectREFR* refr, TESObjectARMO* armor, std::function<void(MaskTriShapeMap*)> functor);

private:
	// Inherited via IAddonAttachmentInterface
	virtual void OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root) override;

	MaskModelMap	m_modelMap;
	TintMaskMap		m_maskMap;

	SimpleLock						m_dyeableLock;
	std::unordered_map<UInt32, bool> m_dyeable;

	// Inherited via BSTEventSink
	virtual EventResult ReceiveEvent(SKSENiNodeUpdateEvent * evn, EventDispatcher<SKSENiNodeUpdateEvent>* dispatcher) override;
};

class NIOVTaskDeferredMask : public TaskDelegate
{
public:
	NIOVTaskDeferredMask(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, ItemAttributeDataPtr overrides);

	virtual void Run();
	virtual void Dispose();

private:
	bool							m_firstPerson;
	UInt32							m_formId;
	UInt32							m_armorId;
	UInt32							m_addonId;
	NiPointer<NiAVObject>			m_object;
	ItemAttributeDataPtr			m_overrides;
};