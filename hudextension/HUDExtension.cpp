#include "HUDExtension.h"
#include "skse64/NiObjects.h"

#include "skse64/GameRTTI.h"
#include "skse64/GameReferences.h"
#include "skse64/GameData.h"

#include <algorithm>


double ObjectWidget::GetProperty(UInt32 type)
{
	switch(type)
	{
	case kPropertyType_Flags:
		return (double)flags;
		break;
	case kPropertyType_PrimaryColor:
		return params[kProperty_PrimaryColor].GetNumber();
		break;
	case kPropertyType_SecondaryColor:
		return params[kProperty_SecondaryColor].GetNumber();
		break;
	case kPropertyType_FlashColor:
		return params[kProperty_FlashColor].GetNumber();
		break;
	case kPropertyType_PrimaryFriendlyColor:
		return params[kProperty_PrimaryFriendlyColor].GetNumber();
		break;
	case kPropertyType_SecondaryFriendlyColor:
		return params[kProperty_SecondaryFriendlyColor].GetNumber();
		break;
	case kPropertyType_FlashFriendlyColor:
		return params[kProperty_FlashFriendlyColor].GetNumber();
		break;
	case kPropertyType_FillMode:
		return params[kProperty_FillMode].GetNumber();
		break;
	case kPropertyType_CurrentValue:
		return params[kProperty_CurrentValue].GetNumber();
		break;
	case kPropertyType_MaximumValue:
		return params[kProperty_MaximumValue].GetNumber();
		break;
	}

	return 0;
}
void ObjectWidget::SetProperty(UInt32 type, double value)
{
	switch(type)
	{
	case kPropertyType_Flags:
		flags = (UInt32)value;
		break;
	case kPropertyType_CurrentValue:
		params[kProperty_CurrentValue].SetNumber(value);
		break;
	case kPropertyType_MaximumValue:
		params[kProperty_MaximumValue].SetNumber(value);
		break;
	case kPropertyType_PrimaryColor:
		params[kProperty_PrimaryColor].SetNumber(value);
		break;
	case kPropertyType_SecondaryColor:
		params[kProperty_SecondaryColor].SetNumber(value);
		break;
	case kPropertyType_FlashColor:
		params[kProperty_FlashColor].SetNumber(value);
		break;
	case kPropertyType_PrimaryFriendlyColor:
		params[kProperty_PrimaryFriendlyColor].SetNumber(value);
		break;
	case kPropertyType_SecondaryFriendlyColor:
		params[kProperty_SecondaryFriendlyColor].SetNumber(value);
		break;
	case kPropertyType_FlashFriendlyColor:
		params[kProperty_FlashFriendlyColor].SetNumber(value);
		break;
	case kPropertyType_FillMode:
		params[kProperty_FillMode].SetNumber(value);
		break;
	}
}

void ObjectWidget::UpdateProperty(UInt32 type)
{
	switch(type)
	{
	case kPropertyType_Flags:
		{
			// Update visibility if necessary
			if((flags & kFlag_ShowInCombat) == kFlag_ShowInCombat || 
				(flags & kFlag_HideOutOfCombat) == kFlag_HideOutOfCombat || 
				(flags & kFlag_HideOnDeath) == kFlag_HideOnDeath) {
				TESForm * form = LookupFormByID(formId);
				if(form) {
					TESObjectREFR * reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
					if(reference) {
						bool isVisible = false;
						bool isFriendly = false;
						QueryState(reference, &isVisible, &isFriendly);
						if(object.IsDisplayObject()) {
							GFxValue::DisplayInfo dInfo;
							dInfo.SetVisible(isVisible);
							object.SetDisplayInfo(&dInfo);
						}
					}
				}
			}
		}
		break;
	case kPropertyType_CurrentValue:
	case kPropertyType_MaximumValue:
		UpdateValues();
		break;
	case kPropertyType_PrimaryColor:
	case kPropertyType_SecondaryColor:
	case kPropertyType_FlashColor:
	case kPropertyType_PrimaryFriendlyColor:
	case kPropertyType_SecondaryFriendlyColor:
	case kPropertyType_FlashFriendlyColor:
		UpdateColors();
		break;
	case kPropertyType_FillMode:
		UpdateFillMode();
		break;
	case kPropertyType_StartFlash:
		UpdateFlash();
		break;
	case kPropertyType_Name:
		UpdateText();
		break;
	}
}

bool ObjectWidget::IsFriendly() const
{
	return (flags & ObjectWidget::kFlag_Friendly) == ObjectWidget::kFlag_Friendly;
}

void ObjectWidget::UpdateText()
{
	GFxValue textField;
	object.GetMember("nameField", &textField);
	textField.SetText(params[kProperty_Name].GetString(), false);
}

void ObjectWidget::UpdateValues()
{
	GFxValue update[2];
	update[0] = params[kProperty_CurrentValue];
	update[1] = params[kProperty_MaximumValue];

	object.Invoke("setValues", NULL, update, 2);
}

void ObjectWidget::UpdateColors()
{
	GFxValue update[3];
	if (!IsFriendly()) {
		update[0] = params[kProperty_PrimaryColor];
		update[1] = params[kProperty_SecondaryColor];
		update[2] = params[kProperty_FlashColor];
	}
	else {
		update[0] = params[kProperty_PrimaryFriendlyColor];
		update[1] = params[kProperty_SecondaryFriendlyColor];
		update[2] = params[kProperty_FlashFriendlyColor];
	}
	object.Invoke("setColors", NULL, update, 3);
}

void ObjectWidget::UpdateFlags()
{
	if (object.HasMember("flags")) {
		GFxValue mFlags;
		object.GetMember("flags", &mFlags);
		mFlags.SetNumber(flags);
		object.SetMember("flags", &mFlags);
	}
}

void ObjectWidget::UpdateFillMode()
{
	object.Invoke("setFillDirection", NULL, &params[kProperty_FillMode], 1);
}

void ObjectWidget::UpdateFlash()
{
	GFxValue update;
	update.SetBool(false);
	object.Invoke("startFlash", NULL, &update, 1);
}

void ObjectWidget::QueryState(TESObjectREFR * reference, bool * isVisible, bool * isFriendly)
{
	bool isHidden = false;
	bool isHostileToActor = false;
	bool isHostile = false;
	UInt8 unk1 = 0;
	if((flags & kFlag_UseLineOfSight) == kFlag_UseLineOfSight)
		isHidden = !HasLOS((*g_thePlayer), reference, &unk1);
	Actor * actor = DYNAMIC_CAST(reference, TESObjectREFR, Actor);
	if(actor) {
		if ((flags & kFlag_UpdatePercent) == kFlag_UpdatePercent) {
			params[kProperty_CurrentValue].SetNumber(actor->actorValueOwner.GetCurrent(24));
			params[kProperty_MaximumValue].SetNumber(actor->actorValueOwner.GetMaximum(24));

			double percent = params[kProperty_CurrentValue].GetNumber() / params[kProperty_MaximumValue].GetNumber();
			if (percent >= 1.0 && (g_hudExtension->hudFlags & HUDExtension::kFlags_HideAtFull) == HUDExtension::kFlags_HideAtFull)
				isHidden = true;
		}
		if((flags & kFlag_ShowInCombat) == kFlag_ShowInCombat)
			if(actor->IsInCombat())
				isHidden = !isHidden;
		if((flags & kFlag_HideOutOfCombat) == kFlag_HideOutOfCombat) // When these are not flagged, hide instead
			if(!actor->IsInCombat())
				isHidden = true;
		if((flags & kFlag_HideOnDeath) == kFlag_HideOnDeath)
			if(actor->IsDead(1))
				isHidden = true;
		if ((flags & kFlag_HideOnInvisibility) == kFlag_HideOnInvisibility)
			if (actor->actorValueOwner.GetCurrent(54) > 0)
				isHidden = true;
		if ((flags & kFlag_UseHostility) == kFlag_UseHostility || g_hudExtension->hudFlags != HUDExtension::kFlags_None) {
			isHostileToActor = CALL_MEMBER_FN(*g_thePlayer, IsHostileToActor)(actor);
			if ((flags & kFlag_UseHostility) == kFlag_UseHostility)
				isHostile = isHostileToActor;
		}

		if (g_hudExtension->hudFlags != HUDExtension::kFlags_None) {
			if (isHostileToActor && (g_hudExtension->hudFlags & HUDExtension::kFlags_HideEnemies) == HUDExtension::kFlags_HideEnemies)
				isHidden = true;
			else if (!isHostileToActor) {
				SInt32 relationshipRank = RelationshipRanks::GetRelationshipRank(actor->baseForm, (*g_thePlayer)->baseForm);
				if ((g_hudExtension->hudFlags & HUDExtension::kFlags_HideAllies) == HUDExtension::kFlags_HideAllies && relationshipRank >= 3)
					isHidden = true;
				if ((g_hudExtension->hudFlags & HUDExtension::kFlags_HideFriendly) == HUDExtension::kFlags_HideFriendly && relationshipRank >= 1 && relationshipRank < 3)
					isHidden = true;
				if ((g_hudExtension->hudFlags & HUDExtension::kFlags_HideNonHostile) == HUDExtension::kFlags_HideNonHostile && relationshipRank < 1)
					isHidden = true;
			}
		}
	}

	*isVisible = !isHidden;
	*isFriendly = !isHostile;
}

void ObjectWidget::UpdateComponent(GFxMovieView * view, float * depth)
{
	TESForm * form = LookupFormByID(formId);
	if(form) {
		TESObjectREFR * reference = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
		if(reference) {
			NiPoint3 markerPos;
			reference->GetMarkerPosition(&markerPos);
			markerPos.z += 25;

			float x_out = 0.0;
			float y_out = 0.0;
			float z_out = 0.0;

			GRectF rect = view->GetVisibleFrameRect();

			WorldPtToScreenPt3_Internal(g_worldToCamMatrix, g_viewPort, &markerPos, &x_out, &y_out, &z_out, 1e-5);

			// Move component, update component stats
			y_out = 1.0 - y_out; // Flip y for Flash coordinate system
			y_out = rect.top + (rect.bottom - rect.top) * y_out;
			x_out = rect.left + (rect.right - rect.left) * x_out;

			*depth = z_out;

			bool isVisible = false;
			bool isFriendly = false;
			QueryState(reference, &isVisible, &isFriendly);

			if ((g_hudExtension->hudFlags & HUDExtension::kFlags_HideName) == HUDExtension::kFlags_HideName || (flags & ObjectWidget::kFlag_HideName) == ObjectWidget::kFlag_HideName) {
				params[kProperty_Name].SetString("");
				UpdateText();
			} else {
				const char * text = CALL_MEMBER_FN(reference, GetReferenceName)();
				if (params[kProperty_Name].GetString() != text) {
					params[kProperty_Name].SetString(text);
					UpdateText();
				}
			}

			if ((flags & ObjectWidget::kFlag_UseHostility) == ObjectWidget::kFlag_UseHostility) {
				bool nowFriendly = IsFriendly();
				if (nowFriendly && !isFriendly) { // Turned hostile
					flags &= ~ObjectWidget::kFlag_Friendly;
					UpdateFlags();
					UpdateColors();
				}
				else if (!nowFriendly && isFriendly) { // Turned friendly
					flags |= ObjectWidget::kFlag_Friendly;
					UpdateFlags();
					UpdateColors();
				}
			}

			double scale = min(((100 - z_out * 100) * 10), 50);//(1.0 - z_out) * 100;//min(((100 - z_out * 100) * 10), 50);
			if(object.IsDisplayObject()) {
				GFxValue::DisplayInfo dInfo;
				dInfo.SetPosition(x_out, y_out);
				dInfo.SetScale(scale, scale);
				dInfo.SetVisible(isVisible);
				object.SetDisplayInfo(&dInfo);

				if((flags & kFlag_UpdatePercent) == kFlag_UpdatePercent)
					UpdateValues();
			}
		}
	}
}

void ObjectWidgets::AddGFXMeter(GFxMovieView * view, ObjectWidget * objectMeter, float current, float max, UInt32 flags, UInt32 fillMode, UInt32 colors[])
{
	GFxValue update[11];
	update[0].SetNumber(objectMeter->formId);
	update[1].SetNumber(objectMeter->flags);

	if(current != -1)
		update[2] = objectMeter->params[ObjectWidget::kProperty_CurrentValue];
	else
		update[2].SetUndefined();

	if (max != -1)
		update[3] = objectMeter->params[ObjectWidget::kProperty_MaximumValue];
	else
		update[3].SetUndefined();

	if (colors[0] != -1)
		update[4] = objectMeter->params[ObjectWidget::kProperty_PrimaryColor];
	else
		update[4].SetUndefined();

	if (colors[1] != -1)
		update[5] = objectMeter->params[ObjectWidget::kProperty_SecondaryColor];
	else
		update[5].SetUndefined();

	if (colors[2] != -1)
		update[6] = objectMeter->params[ObjectWidget::kProperty_FlashColor];
	else
		update[6].SetUndefined();

	if (colors[3] != -1)
		update[7] = objectMeter->params[ObjectWidget::kProperty_PrimaryFriendlyColor];
	else
		update[7].SetUndefined();

	if (colors[4] != -1)
		update[8] = objectMeter->params[ObjectWidget::kProperty_SecondaryFriendlyColor];
	else
		update[8].SetUndefined();

	if (colors[5] != -1)
		update[9] = objectMeter->params[ObjectWidget::kProperty_FlashFriendlyColor];
	else
		update[9].SetUndefined();

	if(fillMode != -1)
		update[10] = objectMeter->params[ObjectWidget::kProperty_FillMode];
	else
		update[10].SetUndefined();

	view->Invoke("_root.hudExtension.floatingWidgets.loadWidget", &objectMeter->object, update, 11);
	objectMeter->object.AddManaged();
}

void ObjectWidgets::RemoveGFXMeter(GFxMovieView * view, ObjectWidget * objectMeter)
{
	if(objectMeter->object.IsManaged()) {
		GFxValue arg;
		arg.SetNumber(objectMeter->formId);
		view->Invoke("_root.hudExtension.floatingWidgets.unloadWidget", NULL, &arg, 1);
		objectMeter->object.CleanManaged();
	}
}

bool ObjectWidgets::AddMeter(GFxMovieView * view, UInt32 formId, float current, float max, UInt32 flags, UInt32 fillMode, UInt32 colors[])
{
	bool added = false;
	ObjectWidget objectMeter;
	objectMeter.formId = formId;
	Lock();
	HealthbarSet::iterator it = m_data.find(objectMeter);
	if(it == m_data.end()) {
		objectMeter.flags = flags;
		if(current != -1)
			objectMeter.params[ObjectWidget::kProperty_CurrentValue].SetNumber(current);
		if (max != -1)
			objectMeter.params[ObjectWidget::kProperty_MaximumValue].SetNumber(max);
		if(colors[0] != -1)
			objectMeter.params[ObjectWidget::kProperty_PrimaryColor].SetNumber(colors[0]);
		if(colors[1] != -1)
			objectMeter.params[ObjectWidget::kProperty_SecondaryColor].SetNumber(colors[1]);
		if(colors[2] != -1)
			objectMeter.params[ObjectWidget::kProperty_FlashColor].SetNumber(colors[2]);
		if (colors[3] != -1)
			objectMeter.params[ObjectWidget::kProperty_PrimaryFriendlyColor].SetNumber(colors[3]);
		if (colors[4] != -1)
			objectMeter.params[ObjectWidget::kProperty_SecondaryFriendlyColor].SetNumber(colors[4]);
		if (colors[5] != -1)
			objectMeter.params[ObjectWidget::kProperty_FlashFriendlyColor].SetNumber(colors[5]);
		if(fillMode != -1)
			objectMeter.params[ObjectWidget::kProperty_FillMode].SetNumber(fillMode);

		AddGFXMeter(view, &objectMeter, current, max, flags, fillMode, colors);
		m_data.insert(objectMeter);
		added = true;
	}
	Release();
	return added;
}

bool ObjectWidgets::RemoveMeter(GFxMovieView * view, UInt32 formId, UInt32 context)
{
	bool removed = false;

	ObjectWidget foundBar;
	foundBar.formId = formId;
		
	Lock();
	HealthbarSet::iterator it = m_data.find(foundBar);
	if(it != m_data.end()) {
		if ((((it->flags & ObjectWidget::kFlag_RemoveOnDeath) == ObjectWidget::kFlag_RemoveOnDeath && (context & ObjectWidget::kContext_Death) == ObjectWidget::kContext_Death)) ||
			(((it->flags & ObjectWidget::kFlag_RemoveOutOfCombat) == ObjectWidget::kFlag_RemoveOutOfCombat && (context & ObjectWidget::kContext_LeaveCombat) == ObjectWidget::kContext_LeaveCombat)) ||
			context == ObjectWidget::kContext_None) {
			RemoveGFXMeter(view, const_cast<ObjectWidget*>(&(*it)));
			m_data.erase(it);
			removed = true;
		}
	}
	Release();

	return removed;
}

void ObjectWidgets::RemoveAllHealthbars(GFxMovieView * view)
{
	Lock();
	HealthbarSet::iterator it = m_data.begin();
	while(it != m_data.end()) {
		RemoveGFXMeter(view, const_cast<ObjectWidget*>(&(*it)));
		m_data.erase(it++);
	}
	Release();
}

double ObjectWidgets::GetMeterProperty(UInt32 formId, UInt32 type)
{
	ObjectWidget objectMeter;
	objectMeter.formId = formId;
	double value = 0;
	Lock();
	HealthbarSet::iterator it = m_data.find(objectMeter);
	if(it != m_data.end()) {
		value = const_cast<ObjectWidget*>(&(*it))->GetProperty(type);
	}
	Release();
	return value;
}

void ObjectWidgets::UpdateMeterProperty(UInt32 formId, UInt32 type)
{
	ObjectWidget objectMeter;
	objectMeter.formId = formId;
	Lock();
	HealthbarSet::iterator it = m_data.find(objectMeter);
	if(it != m_data.end()) {
		const_cast<ObjectWidget*>(&(*it))->UpdateProperty(type);
	}
	Release();
}

void ObjectWidgets::SetMeterProperty(UInt32 formId, UInt32 type, double value)
{
	ObjectWidget objectMeter;
	objectMeter.formId = formId;

	Lock();
	HealthbarSet::iterator it = m_data.find(objectMeter);
	if(it != m_data.end()) {
		const_cast<ObjectWidget*>(&(*it))->SetProperty(type, value);
	}
	Release();
}

void ObjectWidgets::UpdateComponents(GFxMovieView * view)
{
	Lock();

	GFxValue handleArray;
	view->CreateArray(&handleArray);

	for(HealthbarSet::iterator it = m_data.begin(); it != m_data.end(); ++it) {
		float zPos = 0.0;
		const_cast<ObjectWidget*>(&(*it))->UpdateComponent(view, &zPos);

		GFxValue meterData;
		view->CreateObject(&meterData);
		GFxValue meterId;
		meterId.SetNumber(it->formId);
		GFxValue meterzPos;
		meterzPos.SetNumber(zPos);

		meterData.SetMember("id", &meterId);
		meterData.SetMember("zIndex", &meterzPos);

		handleArray.PushBack(&meterData);
	}
	view->Invoke("_root.hudExtension.floatingWidgets.sortWidgetDepths", NULL, &handleArray, 1);

	Release();
}

HUDExtension::HUDExtension(GFxMovieView* movie) : HUDObject(movie)
{
	hudFlags = 0;
}

HUDExtension::~HUDExtension()
{
	RemoveAllHealthbars();
};

void HUDExtension::RemoveAllHealthbars()
{
	if(view)
		m_components.RemoveAllHealthbars(view);
}

bool HUDExtension::AddMeter(UInt32 formId, float current, float max, UInt32 flags, UInt32 fillMode, UInt32 colors[])
{
	if(view)
		return m_components.AddMeter(view, formId, current, max, flags, fillMode, colors);

	return false;
}

bool HUDExtension::RemoveMeter(UInt32 formId, UInt32 context)
{
	if(view)
		return m_components.RemoveMeter(view, formId, context);

	return false;
}

void HUDExtension::Update()
{
	m_components.UpdateComponents(view);
}

void HUDExtension::UpdateMeterProperty(UInt32 formId, UInt32 type)
{
	m_components.UpdateMeterProperty(formId, type);
}

double HUDExtension::GetMeterProperty(UInt32 formId, UInt32 type)
{
	return m_components.GetMeterProperty(formId, type);
}

void HUDExtension::SetMeterProperty(UInt32 formId, UInt32 type, double value)
{
	m_components.SetMeterProperty(formId, type, value);
}

void AddRemoveWidgetTask::Run()
{
	if(g_hudExtension) {
		switch(m_state) {
			case 0:
				g_hudExtension->RemoveMeter(m_formId, m_context);
				break;
			case 1:
				{
					UInt32 flags = ObjectWidget::kFlag_RemoveOnDeath | ObjectWidget::kFlag_RemoveOutOfCombat | ObjectWidget::kFlag_UpdatePercent | ObjectWidget::kFlag_UseLineOfSight | ObjectWidget::kFlag_UseHostility | ObjectWidget::kFlag_HideOnInvisibility;
					if((m_context & ObjectWidget::kContext_Friendly) == ObjectWidget::kContext_Friendly)
						flags |= ObjectWidget::kFlag_Friendly;
					UInt32 colors[] = {-1, -1, -1, -1, -1, -1};
					g_hudExtension->AddMeter(m_formId, m_current, m_max, flags, -1, colors);
				}
				break;
			default:
				g_hudExtension->RemoveMeter(m_formId, m_context);
				break;
		}
	}
}

SetWidgetPropertyTask::SetWidgetPropertyTask(UInt32 formId, UInt32 type, double value) : m_formId(formId), m_type(type), m_value(value)
{
	if(g_hudExtension) {
		g_hudExtension->SetMeterProperty(formId, type, value);
	}
}

void SetWidgetPropertyTask::Run()
{
	if(g_hudExtension) {
		g_hudExtension->UpdateMeterProperty(m_formId, m_type);
	}
}
