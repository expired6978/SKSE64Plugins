#pragma once

#include "RE/G/GFxFunctionHandler.h"


class GFxValue;
class GFxMovieView;

class SKSEScaleform_GetHeadParts : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_GetModName : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_SetCharacterName : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_GetSliderData : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_GetSliderPartData : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_ReloadSliders : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_LoadPreset : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_SavePreset : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_ReadPreset : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_ImportHead : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_LoadImportedHead : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_ClearSculptData : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_ReleaseImportedHead : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_ExportHead : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_GetPlayerPosition : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_GetPlayerRotation : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_SetPlayerRotation : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_GetRaceSexCameraRot : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_GetRaceSexCameraPos : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_SetRaceSexCameraPos : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_CreateMorphEditor : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_ReleaseMorphEditor : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_BeginRotateMesh : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_DoRotateMesh : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_EndRotateMesh : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_BeginPanMesh : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_DoPanMesh : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_EndPanMesh : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_BeginPaintMesh : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_DoPaintMesh : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_EndPaintMesh : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_DoHoverMesh : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_GetCurrentBrush : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_SetCurrentBrush : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_GetBrushes : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_SetBrushData : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_GetMeshes : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_SetMeshData : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_GetActionLimit : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_UndoAction : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_RedoAction : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_GoToAction : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_GetMeshCameraRadius : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_SetMeshCameraRadius : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};

class SKSEScaleform_GetExternalFiles : public RE::GFxFunctionHandler
{
public:
	void Call(RE::GFxFunctionHandler::Params& a_params) override;
};
