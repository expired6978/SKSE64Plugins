#include "OverrideVariant.h"

template <> void PackValue <float>(OverrideVariant * dst, UInt16 key, UInt8 index, float * src)
{
	switch (key)
	{
		case OverrideVariant::kParam_ShaderEmissiveMultiple:
		case OverrideVariant::kParam_ShaderGlossiness:
		case OverrideVariant::kParam_ShaderSpecularStrength:
		case OverrideVariant::kParam_ShaderLightingEffect1:
		case OverrideVariant::kParam_ShaderLightingEffect2:
		case OverrideVariant::kParam_ShaderAlpha:
		case OverrideVariant::kParam_ControllerStartStop:
		case OverrideVariant::kParam_ControllerStartTime:
		case OverrideVariant::kParam_ControllerStopTime:
		case OverrideVariant::kParam_ControllerFrequency:
		case OverrideVariant::kParam_ControllerPhase:
		case OverrideVariant::kParam_NodeTransformPosition:
		case OverrideVariant::kParam_NodeTransformRotation:
		case OverrideVariant::kParam_NodeTransformScale:
		dst->SetFloat(key, index, *src);
		break;
		default:
		dst->SetNone();
		break;
	}
}
template <> void PackValue <UInt32>(OverrideVariant * dst, UInt16 key, UInt8 index, UInt32 * src)
{
	switch (key)
	{
		case OverrideVariant::kParam_ShaderEmissiveColor:
		case OverrideVariant::kParam_ShaderTintColor:
		dst->SetInt(key, index, *src);
		break;
		default:
		dst->SetNone();
		break;
	}
}
template <> void PackValue <SInt32>(OverrideVariant * dst, UInt16 key, UInt8 index, SInt32 * src)
{
	switch (key)
	{
		case OverrideVariant::kParam_ShaderEmissiveColor:
		case OverrideVariant::kParam_ShaderTintColor:
		dst->SetInt(key, index, *src);
		break;
		default:
		dst->SetNone();
		break;
	}
}
template <> void PackValue <bool>(OverrideVariant * dst, UInt16 key, UInt8 index, bool * src)
{
	dst->SetNone();
	//dst->SetBool(key, index, *src);
}
template <> void PackValue <BSFixedString>(OverrideVariant * dst, UInt16 key, UInt8 index, BSFixedString * src)
{
	switch (key)
	{
		case OverrideVariant::kParam_ShaderTexture:
		dst->SetString(key, index, src->data);
		break;
		case OverrideVariant::kParam_NodeDestination:
		dst->SetString(key, index, src->data);
		break;
		default:
		dst->SetNone();
		break;
	}
}
template <> void PackValue <NiColor>(OverrideVariant * dst, UInt16 key, UInt8 index, NiColor * src)
{
	switch (key)
	{
		case OverrideVariant::kParam_ShaderEmissiveColor:
		case OverrideVariant::kParam_ShaderTintColor:
		dst->SetColor(key, index, *src);
		break;
		default:
		dst->SetNone();
		break;
	}
}
template <> void PackValue <NiColorA>(OverrideVariant * dst, UInt16 key, UInt8 index, NiColorA * src)
{
	switch (key)
	{
		case OverrideVariant::kParam_ShaderEmissiveColor:
		dst->SetColorA(key, index, *src);
		break;
		default:
		dst->SetNone();
		break;
	}
}
template <> void PackValue <BGSTextureSet*>(OverrideVariant * dst, UInt16 key, UInt8 index, BGSTextureSet ** src)
{
	switch (key)
	{
		case OverrideVariant::kParam_ShaderTextureSet:
		dst->SetIdentifier(key, index, (void*)*src);
		break;
		default:
		dst->SetNone();
		break;
	}
}

template <> void UnpackValue <float>(float * dst, OverrideVariant * src)
{
	switch (src->type)
	{
		case OverrideVariant::kType_Int:
		*dst = src->data.i;
		break;

		case OverrideVariant::kType_Float:
		*dst = src->data.f;
		break;

		case OverrideVariant::kType_Bool:
		*dst = src->data.b;
		break;

		default:
		*dst = 0;
		break;
	}
}

template <> void UnpackValue <UInt32>(UInt32 * dst, OverrideVariant * src)
{
	switch (src->type)
	{
		case OverrideVariant::kType_Int:
		*dst = src->data.u;
		break;

		case OverrideVariant::kType_Float:
		*dst = src->data.f;
		break;

		case OverrideVariant::kType_Bool:
		*dst = src->data.b;
		break;

		default:
		*dst = 0;
		break;
	}
}

template <> void UnpackValue <SInt32>(SInt32 * dst, OverrideVariant * src)
{
	switch (src->type)
	{
		case OverrideVariant::kType_Int:
		*dst = src->data.u;
		break;

		case OverrideVariant::kType_Float:
		*dst = src->data.f;
		break;

		case OverrideVariant::kType_Bool:
		*dst = src->data.b;
		break;

		default:
		*dst = 0;
		break;
	}
}

template <> void UnpackValue <bool>(bool * dst, OverrideVariant * src)
{
	switch (src->type)
	{
		case OverrideVariant::kType_Int:
		*dst = src->data.u != 0;
		break;

		case OverrideVariant::kType_Float:
		*dst = src->data.f != 0;
		break;

		case OverrideVariant::kType_Bool:
		*dst = src->data.b;
		break;

		default:
		*dst = 0;
		break;
	}
}

template <> void UnpackValue <BSFixedString>(BSFixedString * dst, OverrideVariant * src)
{
	switch (src->type)
	{
		case OverrideVariant::kType_String:
		CALL_MEMBER_FN(dst, Set)(src->data.str);
		break;
		default:
		break;
	}
}

template <> void UnpackValue <NiColor>(NiColor * dst, OverrideVariant * src)
{
	switch (src->type)
	{
		case OverrideVariant::kType_Int:
		dst->r = ((src->data.u >> 16) & 0xFF) / 255.0;
		dst->g = ((src->data.u >> 8) & 0xFF) / 255.0;
		dst->b = ((src->data.u) & 0xFF) / 255.0;
		break;

		default:
		dst->r = 0;
		dst->g = 0;
		dst->b = 0;
		break;
	}
}

template <> void UnpackValue <NiColorA>(NiColorA * dst, OverrideVariant * src)
{
	switch (src->type)
	{
		case OverrideVariant::kType_Int:
		dst->a = ((src->data.u >> 24) & 0xFF) / 255.0;
		dst->r = ((src->data.u >> 16) & 0xFF) / 255.0;
		dst->g = ((src->data.u >> 8) & 0xFF) / 255.0;
		dst->b = ((src->data.u) & 0xFF) / 255.0;
		break;

		default:
		dst->r = 0;
		dst->g = 0;
		dst->b = 0;
		dst->a = 0;
		break;
	}
}

template <> void UnpackValue <BGSTextureSet*>(BGSTextureSet ** dst, OverrideVariant * src)
{
	switch (src->type)
	{
		case OverrideVariant::kType_Identifier:
		*dst = (BGSTextureSet*)src->data.p;
		break;

		default:
		*dst = NULL;
		break;
	}
}

bool OverrideVariant::IsIndexValid(UInt16 key)
{
	return (key >= OverrideVariant::kParam_ControllersStart && key <= OverrideVariant::kParam_ControllersEnd) || key == OverrideVariant::kParam_ShaderTexture || (key >= OverrideVariant::kParam_NodeTransformStart && key <= OverrideVariant::kParam_NodeTransformEnd);
}