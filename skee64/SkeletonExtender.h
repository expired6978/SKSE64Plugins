#pragma once

class TESObjectREFR;
class NiNode;
class NiAVObject;
class NiStringsExtraData;
class NiStringExtraData;

#include <set>
#include "skse64\GameTypes.h"
#include "StringTable.h"
#include "IPluginInterface.h"

class SkeletonExtenderInterface : public IAddonAttachmentInterface
{
public:
	void Attach(TESObjectREFR * refr, NiNode * skeleton, NiAVObject * objectRoot);
	NiNode * LoadTemplate(NiNode * parent, const char * path);
	void AddTransforms(TESObjectREFR * refr, bool isFirstPerson, NiNode * skeleton, NiAVObject * objectRoot);
	void ReadTransforms(TESObjectREFR * refr, const char * jsonData, bool isFirstPerson, bool isFemale, std::set<SKEEFixedString> & nodes, std::set<SKEEFixedString> & changes);

	// Inherited via IAddonAttachmentInterface
	virtual void OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root) override;
};