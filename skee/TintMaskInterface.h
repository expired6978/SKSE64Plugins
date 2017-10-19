#pragma once

#include "IPluginInterface.h"
#include "IHashType.h"
#include "ItemDataInterface.h"

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

/*
MAKE_NI_POINTER(NiRenderedTexture);
MAKE_NI_POINTER(BSRenderTargetGroup);

class TintMaskShaderMaterial
{
public:
	TintMaskShaderMaterial::TintMaskShaderMaterial()
	{
		unk04 = 0;
		unk08 = 0;
		unk10 = 0;
		unk14 = 0;
		unk28 = -1;
		unk18 = 1.0f;
		unk1C = 1.0f;
		unk20 = 1.0f;
		unk24 = 1.0f;
		unk30 = 1.0f;
		unk34 = 1.0f;
		unk38 = 1.0f;
		diffuse = NULL;
		normalMap = NULL;
		heightMap = NULL;
		specular = NULL;
		unk4C = 3;
		alpha = 1.0f;
		unk58 = 0.0f;
		glossiness = 1.0f;
		specularStrength = 1.0f;
		lightingEffect1 = 0.0f;
		lightingEffect2 = 0.0f;
		unk6C = 0;
		renderedTexture = NULL;
		unk74 = NULL;
		unk78 = NULL;

		// Custom Param
		renderTarget = NULL;
	};
	virtual ~TintMaskShaderMaterial()
	{
		CALL_MEMBER_FN(this, dtor)();
	};
	virtual TintMaskShaderMaterial * Create(void) { return new TintMaskShaderMaterial; };
	virtual void Copy(TintMaskShaderMaterial * source)
	{
		CALL_MEMBER_FN(this, FnCopy)(source);
	};

	virtual bool Unk_03(void * unk1) { return CALL_MEMBER_FN(this, Fn03)(unk1); };
	virtual SInt32 Unk_04(void * unk1) { return CALL_MEMBER_FN(this, Fn04)(unk1); };
	virtual void * Unk_05(void) { return CALL_MEMBER_FN(this, Fn05)(); };
	virtual UInt32 GetShaderType(void) { return BSMaskedShaderMaterial::kShaderType_FaceTint; };
	virtual UInt32 Unk_07(void) { return 2; };	// Always seems to be 2
	virtual void SetTexture(UInt32 index, BSTextureSet * texture, SInt32 unk1)
	{
		CALL_MEMBER_FN(this, FnSetTexture)(index, texture, unk1);
	};
	virtual void ReleaseTextures(void)
	{
		CALL_MEMBER_FN(this, FnReleaseTextures)();
	};
	virtual void Unk_0A(UInt8 unk1, UInt8 unk2, UInt8 unk3, UInt8 unk4, UInt8 unk5, UInt32 unk6, UInt32 unk7) { CALL_MEMBER_FN(this, Fn0A)(unk1, unk2, unk3, unk4, unk5, unk6, unk7); }; // AddRefs
	virtual void Unk_0B(void * unk1, UInt32 unk2) { CALL_MEMBER_FN(this, Fn0B)(unk1, unk2); };
	virtual void * Unk_0C(void * unk1) { return CALL_MEMBER_FN(this, Fn0C)(unk1); };
	virtual void * Unk_0D(void * unk1) { return CALL_MEMBER_FN(this, Fn0D)(unk1); };

	MEMBER_FN_PREFIX(TintMaskShaderMaterial);
	DEFINE_MEMBER_FN(dtor, void, 0x00C987E0);
	DEFINE_MEMBER_FN(FnCopy, void, 0x00C98900, TintMaskShaderMaterial * other);
	DEFINE_MEMBER_FN(Fn03, bool, 0x00C96A30, void * unk1);
	DEFINE_MEMBER_FN(Fn04, SInt32, 0x00C973C0, void * unk1);
	DEFINE_MEMBER_FN(Fn05, void *, 0x00C96950);
	DEFINE_MEMBER_FN(FnSetTexture, void *, 0x00C9A050, UInt32 index, BSTextureSet * texture, SInt32 unk1);
	DEFINE_MEMBER_FN(FnReleaseTextures, void, 0x00C97460);
	DEFINE_MEMBER_FN(Fn0A, void, 0x00C989C0, UInt8 unk1, UInt8 unk2, UInt8 unk3, UInt8 unk4, UInt8 unk5, UInt32 unk6, UInt32 unk7);
	DEFINE_MEMBER_FN(Fn0B, void, 0x00C974E0, void * unk1, UInt32 unk2);
	DEFINE_MEMBER_FN(Fn0C, void *, 0x00C99990, void * unk1);
	DEFINE_MEMBER_FN(Fn0D, void *, 0x00C99A90, void * unk1);

	// redirect to formheap
	static void * operator new(std::size_t size)
	{
		return FormHeap_Allocate(size);
	}

	static void * operator new(std::size_t size, const std::nothrow_t &)
	{
		return FormHeap_Allocate(size);
	}

	// placement new
	static void * operator new(std::size_t size, void * ptr)
	{
		return ptr;
	}

	static void operator delete(void * ptr)
	{
		FormHeap_Free(ptr);
	}

	static void operator delete(void * ptr, const std::nothrow_t &)
	{
		FormHeap_Free(ptr);
	}

	static void operator delete(void *, void *)
	{
		// placement delete
	}

	UInt32	unk04;	// 04 BSIntrusiveRefCounted?
	UInt32	unk08;	// 08 inited to 0
	UInt32	unk0C;	// 0C inited to 0
	UInt32	unk10;	// 10 inited to 0
	UInt32	unk14;	// 14 inited to 0
	float	unk18;	// 18 inited to 1.0
	float	unk1C;	// 1C inited to 1.0
	float	unk20;	// 20 inited to 1.0
	float	unk24;	// 24 inited to 1.0
	SInt32	unk28;	// 28 inited to -1 flags?
	UInt32	unk2C;	// 2C flags?
	float	unk30;
	float	unk34;
	float	unk38;
	NiSourceTexture	* diffuse;	// 3C inited to 0
	NiSourceTexture	* normalMap;	// 40 inited to 0
	NiSourceTexture	* heightMap;	// 44 inited to 0
	NiSourceTexture	* specular;	// 48 inited to 0
	UInt32	unk4C;				// 4C inited to 3
	BSTextureSetPtr	textureSet;		// 50 inited to 0
	float	alpha;				// 54 inited to 1.0
	float	unk58;				// 58 inited to 0
	float	glossiness;			// 5C inited to 1.0
	float	specularStrength;	// 60 inited to 1.0
	float	lightingEffect1;	// 64 inited to 0
	float	lightingEffect2;	// 68 inited to 0
	UInt32	unk6C;				// 6C inited to 0
	NiRenderedTexturePtr renderedTexture;	// 70 inited to 0
	NiSourceTexture * unk74;				// 74 inited to 0
	NiSourceTexture * unk78;				// 78 inited to 0

	// Custom Param
	BSRenderTargetGroupPtr renderTarget;		// 7C
};*/

typedef std::unordered_map<BSLightingShaderProperty*, NiTexture*> TintMaskCacheMap;

class TintMaskMap : public SafeDataHolder<TintMaskCacheMap>
{
public:
	void ManageRenderTargetGroups();
	NiTexture * GetRenderTarget(BSLightingShaderProperty* key);
	void AddRenderTargetGroup(BSLightingShaderProperty* key, NiTexture* value);
	void ReleaseRenderTargetGroups();

	bool IsCaching() const { return m_caching; }

private:
	bool m_caching;
};

typedef std::vector<const char*> MaskTextureList;
typedef std::vector<SInt32> MaskColorList;
typedef std::vector<float> MaskAlphaList;

typedef std::tuple<UInt16, UInt16, MaskTextureList, MaskColorList, MaskAlphaList, UInt8> MaskLayerTuple;

// maps diffuse names to layer data
class MaskDiffuseMap : public std::unordered_map<const char*, MaskLayerTuple>
{
public:
	MaskLayerTuple * GetMaskLayers(BSFixedString texture);
};

// maps trishape names to diffuse names
class MaskTriShapeMap : public std::unordered_map<const char*, MaskDiffuseMap>
{
public:
	MaskDiffuseMap * GetDiffuseMap(BSFixedString triShape);
};


typedef std::unordered_map<const char*, MaskTriShapeMap> MaskModelContainer;

// Maps model names to trishape names
class MaskModelMap : public SafeDataHolder<MaskModelContainer>
{
public:
	MaskLayerTuple * GetMask(BSFixedString nif, BSFixedString trishape, BSFixedString diffuse);

	void ApplyLayers(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMA * arma, NiAVObject * node, std::function<void(BSGeometry*, MaskLayerTuple*)> functor);
	MaskTriShapeMap * GetTriShapeMap(BSFixedString nifPath);
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
	
	struct ObjectMask
	{
		BSGeometry		* object = NULL;
		UInt32			layerCount = 0;
		const char		** textureData = NULL;
		SInt32			* colorData = NULL;
		float			* alphaData = NULL;
		UInt32			resolutionWData = NULL;
		UInt32			resolutionHData = NULL;
	};
	typedef std::vector<ObjectMask> MaskList;

	virtual void ApplyMasks(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, std::function<void(ColorMap*)> overrides);
	virtual void ManageTints() { m_maskMap.ManageRenderTargetGroups(); }
	virtual void ReleaseTints() { m_maskMap.ReleaseRenderTargetGroups(); }

	void CreateTintsFromData(tArray<TintMask*> & masks, UInt32 size, const char ** textureData, SInt32 * colorData, float * alphaData, ColorMap & overrides);
	void ReleaseTintsFromData(tArray<TintMask*> & masks);

	//void ReadTintData();
	void ReadTintData(LPCTSTR lpFolder, LPCTSTR lpFilePattern);
	void ParseTintData(LPCTSTR filePath);

	MaskModelMap	m_modelMap;
	TintMaskMap		m_maskMap;
};

class NIOVTaskDeferredMask : public TaskDelegate
{
public:
	NIOVTaskDeferredMask::NIOVTaskDeferredMask(TESObjectREFR * refr, bool isFirstPerson, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, std::function<void(ColorMap*)> overrides);

	virtual void Run();
	virtual void Dispose();

private:
	bool							m_firstPerson;
	UInt32							m_formId;
	UInt32							m_armorId;
	UInt32							m_addonId;
	NiAVObject						* m_object;
	std::function<void(ColorMap*)>	m_overrides;
};