#pragma once

#include "skee64/IPluginInterface.h"

class TESObjectREFR;
class TESObjectARMO;
class TESObjectARMA;
class NiAVObject;
class NiNode;

class ActorArmorTangentUpdater : public IAddonAttachmentInterface
{
public:
	// Inherited via IAddonAttachmentInterface
	virtual void OnAttach(TESObjectREFR* refr, TESObjectARMO* armor, TESObjectARMA* addon, NiAVObject* object, bool isFirstPerson, NiNode* skeleton, NiNode* root) override;
};