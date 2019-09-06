#pragma once

#include "skse64/GameMenus.h"
#include "skse64/Hooks_UI.h"

#include <unordered_map>
#include <memory>

class ObjectWidget
{
public:
	enum Flags
	{
		kFlag_None					= 0,
		kFlag_UpdatePercent			= (1 << 0),
		kFlag_UseLineOfSight		= (1 << 1),
		kFlag_RemoveOnDeath			= (1 << 2),
		kFlag_RemoveOutOfCombat		= (1 << 3),
		kFlag_HideOnDeath			= (1 << 4),
		kFlag_HideOutOfCombat		= (1 << 5),
		kFlag_ShowInCombat			= (1 << 6),
		kFlag_Friendly				= (1 << 7),
		kFlag_UseHostility			= (1 << 8),
		kFlag_HideOnInvisibility	= (1 << 9),
		kFlag_HideName				= (1 << 10),
		kFlag_ShowLevel				= (1 << 11)
	};

	enum FillModes
	{
		kFillMode_Left = 0,
		kFillMode_Right,
		kFillMode_Both
	};

	enum PropertyTypes
	{
		kPropertyType_Flags,
		kPropertyType_CurrentValue,
		kPropertyType_MaximumValue,
		kPropertyType_PrimaryColor,
		kPropertyType_SecondaryColor,
		kPropertyType_FlashColor,
		kPropertyType_PrimaryFriendlyColor,
		kPropertyType_SecondaryFriendlyColor,
		kPropertyType_FlashFriendlyColor,
		kPropertyType_FillMode,
		kPropertyType_StartFlash,
		kPropertyType_Name,
		kPropertyType_Level,
		kPropertyType_NumProperties
	};


	enum Properties
	{
		kProperty_CurrentValue = 0,
		kProperty_MaximumValue,
		kProperty_PrimaryColor,
		kProperty_SecondaryColor,
		kProperty_FlashColor,
		kProperty_PrimaryFriendlyColor,
		kProperty_SecondaryFriendlyColor,
		kProperty_FlashFriendlyColor,
		kProperty_FillMode,
		kProperty_Name,
		kProperty_Level,
		kProperty_NumProperties
	};

	enum StateContext
	{
		kContext_None = 0,
		kContext_LeaveCombat = 1,
		kContext_Death = 2,
		kContext_Friendly = 4
	};

	ObjectWidget(UInt32 formId = 0) : formId(formId), flags(kFlag_RemoveOnDeath | kFlag_RemoveOutOfCombat | kFlag_UpdatePercent | kFlag_UseLineOfSight) { }

	UInt32		formId;
	UInt32		flags;
	GFxValue	params[kProperty_NumProperties];
	GFxValue	object;
	char		displayName[MAX_PATH];

	double GetProperty(UInt32 type) const;
	void SetProperty(UInt32 type, double value);
	void UpdateProperty(UInt32 type);

	void QueryState(TESObjectREFR * reference, bool * visible, bool * hostile);
	bool IsFriendly() const;

	void UpdateFlags();
	void UpdateValues();
	void UpdateColors();
	void UpdateFillMode();
	void UpdateFlash();
	void UpdateText();
	void UpdateLevel();
	void UpdateComponent(GFxMovieView * view, float * depth);
};

class ObjectWidgets : public SafeDataHolder<std::unordered_map<UInt32, std::shared_ptr<ObjectWidget>>>
{
	typedef std::unordered_map<UInt32, std::shared_ptr<ObjectWidget>> HealthbarMap;
public:
	bool AddGFXMeter(GFxMovieView * view, std::shared_ptr<ObjectWidget> & objectMeter, float current, float max, UInt32 flags, UInt32 fillMode, UInt32 colors[]);
	void RemoveGFXMeter(GFxMovieView * view, std::shared_ptr<ObjectWidget> & objectMeter);

	bool AddMeter(GFxMovieView * view, UInt32 formId, float current, float max, UInt32 flags, UInt32 fillMode, UInt32 colors[]);
	bool RemoveMeter(GFxMovieView * view, UInt32 formId, UInt32 context);

	void UpdateMeterProperty(UInt32 formId, UInt32 type);
	double GetMeterProperty(UInt32 formId, UInt32 type);
	void SetMeterProperty(UInt32 formId, UInt32 type, double value);

	void RemoveAllHealthbars(GFxMovieView * view);
	void UpdateComponents(GFxMovieView * view);
};

class HUDExtension : public HUDObject
{
public:
	HUDExtension(GFxMovieView* view);
	virtual ~HUDExtension();

	bool AddMeter(UInt32 formId, float current, float max, UInt32 flags, UInt32 fillMode, UInt32 colors[]);
	bool RemoveMeter(UInt32 formId, UInt32 context);

	void UpdateMeterProperty(UInt32 formId, UInt32 type);
	double GetMeterProperty(UInt32 formId, UInt32 type);
	void SetMeterProperty(UInt32 formId, UInt32 type, double value);

	void RemoveAllHealthbars();

	virtual void Update();

	enum
	{
		kFlags_None				= 0,
		kFlags_HideEnemies		= (1 << 0),
		kFlags_HideAllies		= (1 << 1),
		kFlags_HideFriendly		= (1 << 2),
		kFlags_HideNonHostile	= (1 << 3),
		kFlags_HideAtFull		= (1 << 4),
		kFlags_HideName			= (1 << 5),
		kFlags_ShowLevel		= (1 << 6)
	};

	UInt32 hudFlags;

private:
	ObjectWidgets	m_components;
};


class AddRemoveWidgetTask : public UIDelegate_v1
{
public:
	AddRemoveWidgetTask(UInt32 formId, float current, float max, UInt32 state, UInt32 context) : m_formId(formId), m_current(current), m_max(max), m_state(state), m_context(context){}
	virtual void Run();
	virtual void Dispose()
	{
		delete this;
	}

private:
	UInt32			m_formId;
	float			m_current;
	float			m_max;
	UInt32			m_state;
	UInt32			m_context;
};

class SetWidgetPropertyTask : public UIDelegate_v1
{
public:
	SetWidgetPropertyTask(UInt32 formId, UInt32 type, double value);

	virtual void Run();
	virtual void Dispose()
	{
		delete this;	
	};

private:
	UInt32			m_formId;
	UInt32			m_type;
	double			m_value;
};

extern HUDExtension * g_hudExtension;