#pragma once

#include "skse/ScaleformCallbacks.h"
#include "skse/GameThreads.h"
#include "skse/GameTypes.h"

class GFxValue;
class GFxMovieView;

void RegisterNumber(GFxValue * dst, const char * name, double value);
void RegisterString(GFxValue * dst,  GFxMovieView * view, const char * name, const char * str);

class SKSEScaleform_GetHeadParts : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_GetModName : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_GetSliderData : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_ReloadSliders : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_LoadPreset : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_SavePreset : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_ReadPreset : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_ImportHead : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_LoadImportedHead : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_ClearSculptData : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_ReleaseImportedHead : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_ExportHead : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_GetPlayerPosition : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_GetPlayerRotation : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_SetPlayerRotation : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_GetRaceSexCameraRot : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_GetRaceSexCameraPos : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_SetRaceSexCameraPos : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_CreateMorphEditor : public GFxFunctionHandler
{
public:
	virtual void Invoke(Args * args);
};

class SKSEScaleform_ReleaseMorphEditor : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_BeginRotateMesh : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_DoRotateMesh : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_EndRotateMesh : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_BeginPanMesh : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_DoPanMesh : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_EndPanMesh : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_BeginPaintMesh : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_DoPaintMesh : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_EndPaintMesh : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_GetCurrentBrush : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_SetCurrentBrush : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_GetBrushes : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_SetBrushData : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_GetMeshes : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_SetMeshData : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_GetActionLimit : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_UndoAction : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_RedoAction : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_GoToAction : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_GetMeshCameraRadius : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_SetMeshCameraRadius : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};

class SKSEScaleform_GetExternalFiles : public GFxFunctionHandler
{
public:
	virtual void	Invoke(Args * args);
};
