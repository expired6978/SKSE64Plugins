#pragma once

#include "skse64/GameTypes.h"
#include "skse64/NiTypes.h"

#include "StringTable.h"

extern StringTable g_stringTable;

struct SKSESerializationInterface;
class BGSTextureSet;

class OverrideVariant
{
public:
	OverrideVariant() : type(kType_None), index(-1) { };
	~OverrideVariant() { };

	bool operator<(const OverrideVariant & rhs) const	{ return key < rhs.key || (key == rhs.key && index < rhs.index); }
	bool operator==(const OverrideVariant & rhs) const	{ return key == rhs.key && index == rhs.index; }

	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion, const StringIdMap & stringTable);

	UInt16 key;
	enum
	{
		kKeyMax = 0xFFFF,
		kIndexMax = 0xFF
	};
	enum
	{
		kParam_ShaderEmissiveColor = 0,
		kParam_ShaderEmissiveMultiple,
		kParam_ShaderGlossiness,
		kParam_ShaderSpecularStrength,
		kParam_ShaderLightingEffect1,
		kParam_ShaderLightingEffect2,
		kParam_ShaderTextureSet,
		kParam_ShaderTintColor,
		kParam_ShaderAlpha,
		kParam_ShaderTexture,

		kParam_ControllersStart = 20,
		kParam_ControllerStartStop = kParam_ControllersStart,
		kParam_ControllerStartTime,
		kParam_ControllerStopTime,
		kParam_ControllerFrequency,
		kParam_ControllerPhase,
		kParam_ControllersEnd = kParam_ControllerPhase,

		kParam_NodeTransformStart = 30,
		kParam_NodeTransformScale = kParam_NodeTransformStart,
		kParam_NodeTransformPosition,
		kParam_NodeTransformRotation,
		kParam_NodeTransformEnd = kParam_NodeTransformRotation,

		kParam_NodeDestination = 40
	};
	enum
	{
		kType_None = 0,
		kType_Identifier = 1,
		kType_String = 2,
		kType_Int = 3,
		kType_Float = 4,
		kType_Bool = 5
	};

	UInt8	type;
	SInt8	index;
	union
	{
		SInt32			i;
		UInt32			u;
		float			f;
		bool			b;
		void			* p;
	} data;
	StringTableItem		str;

	void	SetNone(void)
	{
		type = kType_None;
		index = -1;
		data.u = 0;
		str = nullptr;
	}

	void	SetInt(UInt16 paramKey, SInt8 controllerIndex, SInt32 i)
	{
		key = paramKey;
		type = kType_Int;
		index = controllerIndex;
		data.i = i;
		str = nullptr;
	}

	void	SetFloat(UInt16 paramKey, SInt8 controllerIndex, float f)
	{
		key = paramKey;
		type = kType_Float;
		index = controllerIndex;
		data.f = f;
		str = nullptr;
	}

	void	SetBool(UInt16 paramKey, SInt8 controllerIndex, bool b)
	{
		key = paramKey;
		type = kType_Bool;
		index = controllerIndex;
		data.b = b;
		str = nullptr;
	}

	void	SetString(UInt16 paramKey, SInt8 controllerIndex, SKEEFixedString string)
	{
		key = paramKey;
		type = kType_String;
		index = controllerIndex;
		data.p = nullptr;
		str = g_stringTable.GetString(string);
	}

	void	SetColor(UInt16 paramKey, SInt8 controllerIndex, NiColor color)
	{
		key = paramKey;
		type = kType_Int;
		index = controllerIndex;
		data.u = (UInt8)(color.r * 255) << 16 | (UInt8)(color.g * 255) << 8 | (UInt8)(color.b * 255);
		str = nullptr;
	}

	void	SetColorA(UInt16 paramKey, SInt8 controllerIndex, NiColorA color)
	{
		key = paramKey;
		type = kType_Int;
		index = controllerIndex;
		data.u = (UInt8)(color.a * 255) << 24 | (UInt8)(color.r * 255) << 16 | (UInt8)(color.g * 255) << 8 | (UInt8)(color.b * 255);
		str = nullptr;
	}

	void SetIdentifier(UInt16 paramKey, SInt8 controllerIndex, void * ptr)
	{
		key = paramKey;
		type = kType_Identifier;
		index = controllerIndex;
		data.p = ptr;
		str = nullptr;
	}

	static bool IsIndexValid(UInt16 key);
};

template <typename T>
void UnpackValue(T * dst, OverrideVariant * src);

template <> void UnpackValue <float>(float * dst, OverrideVariant * src);
template <> void UnpackValue <UInt32>(UInt32 * dst, OverrideVariant * src);
template <> void UnpackValue <SInt32>(SInt32 * dst, OverrideVariant * src);
template <> void UnpackValue <bool>(bool * dst, OverrideVariant * src);
template <> void UnpackValue <SKEEFixedString>(SKEEFixedString * dst, OverrideVariant * src);
template <> void UnpackValue <BSFixedString>(BSFixedString * dst, OverrideVariant * src);
template <> void UnpackValue <NiColor>(NiColor * dst, OverrideVariant * src);
template <> void UnpackValue <NiColorA>(NiColorA * dst, OverrideVariant * src);
template <> void UnpackValue <BGSTextureSet*>(BGSTextureSet ** dst, OverrideVariant * src);

template <typename T>
void PackValue(OverrideVariant * dst, UInt16 key, UInt8 index, T * src);

template <> void PackValue <float>(OverrideVariant * dst, UInt16 key, UInt8 index, float * src);
template <> void PackValue <UInt32>(OverrideVariant * dst, UInt16 key, UInt8 index, UInt32 * src);
template <> void PackValue <SInt32>(OverrideVariant * dst, UInt16 key, UInt8 index, SInt32 * src);
template <> void PackValue <bool>(OverrideVariant * dst, UInt16 key, UInt8 index, bool * src);
template <> void PackValue <SKEEFixedString>(OverrideVariant * dst, UInt16 key, UInt8 index, SKEEFixedString * src);
template <> void PackValue <BSFixedString>(OverrideVariant * dst, UInt16 key, UInt8 index, BSFixedString * src);
template <> void PackValue <NiColor>(OverrideVariant * dst, UInt16 key, UInt8 index, NiColor * src);
template <> void PackValue <NiColorA>(OverrideVariant * dst, UInt16 key, UInt8 index, NiColorA * src);
template <> void PackValue <BGSTextureSet*>(OverrideVariant * dst, UInt16 key, UInt8 index, BGSTextureSet ** src);