#include "ActorArmorTangentUpdater.h"

#include "skse64/GameReferences.h"
#include "skse64/GameFormComponents.h"

#include "skse64/NiGeometry.h"
#include "skse64/NiProperties.h"

#include <functional>

#include "NifUtils.h"

void ActorArmorTangentUpdater::OnAttach(TESObjectREFR* refr, TESObjectARMO* armor, TESObjectARMA* addon, NiAVObject* object, bool isFirstPerson, NiNode* skeleton, NiNode* root)
{
	if (refr && refr->formType == Character::kTypeID)
	{
		TESObjectARMO* skinForm = nullptr;
		Character* actor = static_cast<Character*>(refr);
		TESNPC* actorBase = static_cast<TESNPC*>(actor->baseForm);
		UInt8 gender = 0;

		TESRace* race = actor->race;
		if (actorBase) {
			if (!race) {
				race = actorBase->race.race;
			}
			skinForm = actorBase->skinForm.skin;
			gender = CALL_MEMBER_FN(actorBase, GetSex)();
		}

		if (!skinForm && race) {
			skinForm = race->skin.skin;
		}

		if (skinForm)
		{
			std::function<BGSTextureSet* ()> GetTextureSet = [=]()
			{
				BGSTextureSet* textureSet = nullptr;
				for (UInt32 i = 0; i < skinForm->armorAddons.count; ++i)
				{
					TESObjectARMA* arma = nullptr;
					skinForm->armorAddons.GetNthItem(i, arma);
					if (addon->biped.GetSlotMask() & arma->biped.GetSlotMask() && arma->isValidRace(race))
					{
						textureSet = arma->skinTextures[gender];
						break;
					}
				}

				return textureSet;
			};

			BGSTextureSet* textureSet = nullptr;
			VisitGeometry(object, [&textureSet, &GetTextureSet, refr, armor, addon](BSGeometry* geometry)
			{
				BSShaderProperty* shaderProperty = niptr_cast<BSShaderProperty>(geometry->m_spEffectState);
				if (shaderProperty && ni_is_type(shaderProperty->GetRTTI(), BSLightingShaderProperty) && shaderProperty->shaderFlags1 & BSShaderProperty::kSLSF1_Model_Space_Normals)
				{
					BSLightingShaderMaterial* material = (BSLightingShaderMaterial*)shaderProperty->material;
					if (material && material->GetShaderType() == BSLightingShaderMaterial::kShaderType_FaceGenRGBTint)
					{
						if (!textureSet)
						{
							textureSet = GetTextureSet();
						}
						if (textureSet && !(textureSet->flags & BGSTextureSet::kFlagHasModelSpaceNormalMap))
						{
							shaderProperty->shaderFlags1 &= ~BSShaderProperty::kSLSF1_Model_Space_Normals;
						}
					}
				}

				return false;
			});
		}
	}
}
