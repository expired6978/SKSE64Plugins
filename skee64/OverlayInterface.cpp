#include "SKEETasks.h"


#include "RE/N/NiGeometry.h"
#include "RE/N/NiRTTI.h"
#include "RE/N/NiExtraData.h"

#include "ActorUpdateManager.h"
#include "OverlayInterface.h"
#include "OverrideInterface.h"
#include "BodyMorphInterface.h"
#include "OverrideVariant.h"
#include "ShaderUtilities.h"
#include "NifUtils.h"
#include "SKEEHooks.h"
#include "Utilities.h"

// windows.h maps the Win32 API name to an intrinsic, which would hide the
// REX::W32 declaration below.
#ifdef InterlockedIncrement
#undef InterlockedIncrement
#endif

#include <unordered_set>
#include <format>
#include <cstdint>
#include "NiRTTIUtils.h"

extern ActorUpdateManager				g_actorUpdateManager;
extern OverlayInterface					g_overlayInterface;
extern OverrideInterface				g_overrideInterface;
extern BodyMorphInterface				g_bodyMorphInterface;

extern const SKSE::TaskInterface* g_task;

extern bool		g_enableFaceOverlays;
extern bool		g_enableOverlays;
extern bool		g_playerOnly;

extern std::uint32_t	g_numBodyOverlays;
extern std::uint32_t	g_numHandOverlays;
extern std::uint32_t	g_numFeetOverlays;
extern std::uint32_t	g_numFaceOverlays;
extern std::uint32_t	g_numSpellBodyOverlays;
extern std::uint32_t	g_numSpellHandOverlays;
extern std::uint32_t	g_numSpellFeetOverlays;
extern std::uint32_t	g_numSpellFaceOverlays;

extern bool		g_overlayAlphaOverride;
extern std::uint16_t	g_overlayAlphaFlags;
extern std::uint16_t	g_overlayAlphaThreshold;
extern bool		g_overlayForceDecal;

extern bool		g_immediateArmor;

extern std::unordered_set<void*> g_adjustedBlocks;

skee_u32 OverlayInterface::GetVersion()
{
	return kCurrentPluginVersion;
}

void OverlayInterface::UninstallOverlay(const char * nodeName, RE::TESObjectREFR * refr, RE::NiNode * parent)
{
	if(!parent)
		return;

	// Remove all overlay instances
	RE::BSFixedString overlayName(nodeName);
	RE::NiAVObject* foundObject = parent->GetObjectByName(overlayName);
	while(foundObject)
	{
		RE::BSGeometry* foundGeometry = foundObject ? foundObject->AsGeometry() : nullptr;
		if(foundGeometry)
		{
			foundGeometry->GetGeometryRuntimeData().skinInstance = nullptr;
		}

		if (foundObject->parent)
			foundObject->parent->DetachChild(foundObject);
		
		foundObject = parent->GetObjectByName(overlayName);
	}
}

void OverlayInterface::InstallOverlay(const char * nodeName, const char * path, RE::TESObjectREFR * refr, RE::BSGeometry * source, RE::NiNode * destination, RE::BGSTextureSet * textureSet)
{
	RE::NiPointer<RE::NiAVObject> newShape;
	RE::NiPointer<RE::NiProperty> alphaProperty;
	RE::NiPointer<RE::NiProperty> shaderProperty;
	RE::BSFixedString overlayName(nodeName);
	RE::NiAVObject * foundGeometry = destination->GetObjectByName(overlayName);
	if (foundGeometry)
	{
		auto* castObj = foundGeometry ? foundGeometry->AsGeometry() : nullptr;
		newShape.reset(castObj);
	}

	bool attachNew = false;
	if(!newShape)
	{
		RE::BSResourceNiBinaryStream binaryStream(path);
		if(!binaryStream.good()) {
			return;
		}

		// Load the overlay NIF and locate its first geometry. The loaded geometry's
		// shader/alpha properties are carried onto the new shape; attachNew marks that a
		// fresh overlay should be attached to the destination node.
		NifStreamWrapper niStream;
		if (!niStream->Load1(&binaryStream)) {
			return;
		}

		for (std::uint32_t t = 0; t < niStream->topObjects.size() && !attachNew; ++t)
		{
			RE::NiObject* root = niStream->topObjects[t].get();
			if (!root)
				continue;

			auto captureGeometry = [&](RE::NiGeometry* geometry) {
				shaderProperty = geometry->spEffectState;
				alphaProperty = geometry->spPropertyState;
				attachNew = true;
			};

			if (RE::NiNode* node = root ? root->AsNode() : nullptr) {
				VisitObjects(node, [&](RE::NiAVObject* object) -> bool {
					if (auto * geometry = object ? object->AsNiGeometry() : nullptr) {
						captureGeometry(geometry);
						return true;
					}
					return false;
				});
			} else if (auto * geometry = root ? root->AsNiGeometry() : nullptr) {
				captureGeometry(geometry);
			}
		}

		// Create a fresh empty shape matching source's type, then copy fields from source
		// (legacy InstallOverlay). Not a deep copy — the overlay carries source's structure
		// (vertices/skin/dynamic data) but the NIF's shader/alpha properties.
		RE::NiAVObject* newShapeRaw = nullptr;
		if (source->AsDynamicTriShape()) {
			newShapeRaw = SKEE::CreateBSDynamicTriShape();
		} else {
			newShapeRaw = SKEE::CreateBSTriShape();
		}
		if (newShapeRaw) {
			newShape.reset(newShapeRaw);
			newShape->name = overlayName;
			newShape->flags.set(RE::NiAVObject::Flag::kAlwaysDraw);
		}
	}
	

	auto * targetShape = newShape.get() ? newShape.get()->AsGeometry() : nullptr;
	if(targetShape)
	{
		targetShape->vertexDesc = source->vertexDesc;

		if (shaderProperty && shaderProperty.get())
			targetShape->GetGeometryRuntimeData().shaderProperty.reset(static_cast<RE::BSShaderProperty*>(shaderProperty.get()));
		if (alphaProperty && alphaProperty.get())
			targetShape->GetGeometryRuntimeData().alphaProperty.reset(static_cast<RE::NiAlphaProperty*>(alphaProperty.get()));

		// Dynamic shape data copy: share the buffer when in g_adjustedBlocks, else memcpy.
		if (auto * newDynShape = targetShape ? targetShape->AsDynamicTriShape() : nullptr) {
			if (auto * sourceShape = source ? source->AsDynamicTriShape() : nullptr) {
				auto & srcRT = sourceShape->GetDynamicTrishapeRuntimeData();
				auto & dstRT = newDynShape->GetDynamicTrishapeRuntimeData();
				dstRT.dataSize = srcRT.dataSize;
				dstRT.frameCount = srcRT.frameCount;
				if (g_enableFaceOverlays && g_adjustedBlocks.find(srcRT.dynamicData) != g_adjustedBlocks.end()) {
					void * ptr = reinterpret_cast<void*>((uintptr_t)srcRT.dynamicData - 0x10);
					REX::W32::InterlockedIncrement(reinterpret_cast<volatile std::uint32_t*>(ptr));
					dstRT.dynamicData = srcRT.dynamicData;  // shared buffer
				} else {
					dstRT.dynamicData = RE::NiMalloc(srcRT.dataSize);
					if (dstRT.dynamicData) {
						std::memcpy(dstRT.dynamicData, srcRT.dynamicData, srcRT.dataSize);
					}
				}
				dstRT.unk178 = srcRT.unk178;
				dstRT.unk17C = 0;
			}
		}

		targetShape->local = source->local;  // m_localTransform
		targetShape->GetGeometryRuntimeData().skinInstance = source->GetGeometryRuntimeData().skinInstance;

		RE::NiProperty * newProperty = netimmerse_cast<RE::NiProperty*>(targetShape->GetGeometryRuntimeData().shaderProperty.get());
		RE::NiProperty * sourceProperty = netimmerse_cast<RE::NiProperty*>(source->GetGeometryRuntimeData().shaderProperty.get());

		auto * shaderProperty = netimmerse_cast<RE::BSLightingShaderProperty*>(newProperty);
		auto * sourceShader = netimmerse_cast<RE::BSLightingShaderProperty*>(sourceProperty);
		if(sourceShader && shaderProperty) {
			if (sourceShader->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kVertexAlpha))
				shaderProperty->flags.set(RE::BSShaderProperty::EShaderPropertyFlag::kVertexAlpha);
			else
				shaderProperty->flags.reset(RE::BSShaderProperty::EShaderPropertyFlag::kVertexAlpha);

			if (sourceShader->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals))
				shaderProperty->flags.set(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals);
			else
				shaderProperty->flags.reset(RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals);

			if (g_overlayForceDecal && !shaderProperty->flags.any(RE::BSShaderProperty::EShaderPropertyFlag::kDecal)) {
				shaderProperty->flags.set(RE::BSShaderProperty::EShaderPropertyFlag::kDecal);
			}

			if (netimmerse_isKind<RE::BSLightingShaderProperty>(sourceShader) && netimmerse_isKind<RE::BSLightingShaderProperty>(shaderProperty))
			{
				RE::BSLightingShaderMaterial * sourceMaterial = (RE::BSLightingShaderMaterial *)sourceShader->material;
				RE::BSLightingShaderMaterial * targetMaterial = (RE::BSLightingShaderMaterial *)shaderProperty->material;

				if(sourceMaterial && targetMaterial)
				{
					// Copy the remaining textures
					if(!textureSet)
					{
						for(std::uint32_t i = 1; i < RE::BSTextureSet::Texture::kTotal; i++)
						{
							const char * texturePath = (sourceMaterial->textureSet ? sourceMaterial->textureSet->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(i)) : "");
							if (targetMaterial->textureSet) { targetMaterial->textureSet->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(i), texturePath); };
						}
					}
					else
					{
						for (std::uint32_t i = 1; i < RE::BSTextureSet::Texture::kTotal; i++)
						{
							const char* texturePath = textureSet->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(i));
							if (targetMaterial->textureSet) { targetMaterial->textureSet->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(i), texturePath); };
						}
					}
					
					targetMaterial->ClearTextures();
					SKEE::InvalidateTextures(shaderProperty, 0);
					SKEE::InitializeShader(shaderProperty, targetShape);
				}
			}
		}

		if(g_overlayAlphaOverride) {
			auto* alphaProp = targetShape->GetGeometryRuntimeData().alphaProperty.get();
			if(alphaProp) {
				alphaProp->alphaFlags = g_overlayAlphaFlags;
				alphaProp->alphaThreshold = g_overlayAlphaThreshold;
			}
		}

		g_overrideInterface.Impl_ApplyNodeOverrides(refr, newShape.get(), true);

		m_callbacks.Lock();
		for (auto cb : m_callbacks.m_data) {
			cb.second(refr, newShape.get());
		}
		m_callbacks.Release();

		if(attachNew) {
			destination->AttachChild(newShape.get(), false);
#ifdef _DEBUG
			SKSE::log::debug("{} - Successfully installed overlay {} to actor: {:08X}", __FUNCTION__, newShape->name, refr->formID);
#endif
		}
	}
}

void OverlayInterface::ResetOverlay(const char * nodeName, RE::TESObjectREFR * refr, RE::BSGeometry * source, RE::NiNode * destination, RE::BGSTextureSet * textureSet, bool resetDiffuse)
{
	RE::NiNode* rootNode = nullptr;
	RE::BSGeometry* foundGeometry = nullptr;

	RE::BSFixedString overlayName(nodeName);
	RE::NiAVObject* foundNode = destination->GetObjectByName(overlayName);
	if(foundNode)
		foundGeometry = foundNode ? foundNode->AsGeometry() : nullptr;

	if(foundGeometry)
	{
		RE::BSLightingShaderProperty* shaderProperty = netimmerse_cast<RE::BSLightingShaderProperty*>(foundGeometry->GetGeometryRuntimeData().shaderProperty.get());
		RE::BSLightingShaderProperty* sourceShader = netimmerse_cast<RE::BSLightingShaderProperty*>(source->GetGeometryRuntimeData().shaderProperty.get());
		if(sourceShader && shaderProperty)
		{
			if(netimmerse_isKind<RE::BSLightingShaderProperty>(sourceShader) && netimmerse_isKind<RE::BSLightingShaderProperty>(shaderProperty))
			{
				RE::BSLightingShaderMaterial* sourceMaterial = (RE::BSLightingShaderMaterial *)sourceShader->material;
				RE::BSLightingShaderMaterial* targetMaterial = (RE::BSLightingShaderMaterial *)shaderProperty->material;

				if(sourceMaterial && targetMaterial)
				{
					/*NiColor color;
					color.r = 0;
					color.g = 0;
					color.b = 0;
					OverrideVariant defaultValue;
					defaultValue.SetColor(OverrideVariant::kParam_ShaderEmissiveColor, -1, color);
					SetShaderProperty(foundGeometry, &defaultValue, true);
					defaultValue.SetFloat(OverrideVariant::kParam_ShaderEmissiveMultiple, -1, 1.0);
					SetShaderProperty(foundGeometry, &defaultValue, true);
					defaultValue.SetFloat(OverrideVariant::kParam_ShaderAlpha, -1, 1.0);
					SetShaderProperty(foundGeometry, &defaultValue, true);
					defaultValue.SetFloat(OverrideVariant::kParam_ShaderGlossiness, -1, 30.0);
					SetShaderProperty(foundGeometry, &defaultValue, true);
					defaultValue.SetFloat(OverrideVariant::kParam_ShaderSpecularStrength, -1, 3.0);
					SetShaderProperty(foundGeometry, &defaultValue, true);
					defaultValue.SetFloat(OverrideVariant::kParam_ShaderLightingEffect1, -1, 0.4);
					SetShaderProperty(foundGeometry, &defaultValue, true);
					defaultValue.SetFloat(OverrideVariant::kParam_ShaderLightingEffect2, -1, 2.0);
					SetShaderProperty(foundGeometry, &defaultValue, true);*/

					// Copy the remaining textures
					if(!textureSet)
					{
						if (resetDiffuse)
							if (targetMaterial->textureSet) { targetMaterial->textureSet->SetTexturePath(RE::BSTextureSet::Texture(0), GetDefaultTexture().c_str()); }

						for(std::uint32_t i = 1; i < RE::BSTextureSet::Texture::kTotal; i++)
						{
							const char * texturePath = (sourceMaterial->textureSet ? sourceMaterial->textureSet->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(i)) : "");
							if (targetMaterial->textureSet) { targetMaterial->textureSet->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(i), texturePath); };
						}
					}
					else
					{
						if (resetDiffuse)
							if (targetMaterial->textureSet) { targetMaterial->textureSet->SetTexturePath(RE::BSTextureSet::Texture(0), GetDefaultTexture().c_str()); }

						for(std::uint32_t i = 1; i < RE::BSTextureSet::Texture::kTotal; i++)
							if (targetMaterial->textureSet) { targetMaterial->textureSet->SetTexturePath(static_cast<RE::BSTextureSet::Texture>(i), textureSet->GetTexturePath(static_cast<RE::BSTextureSet::Texture>(i))); }
					}
					targetMaterial->ClearTextures();
					SKEE::InvalidateTextures(shaderProperty, 0);
					SKEE::InitializeShader(shaderProperty, static_cast<RE::BSGeometry*>(foundGeometry));
				}
			}
		}
	}
}

void OverlayInterface::RelinkOverlay(const char * nodeName, RE::TESObjectREFR * refr, RE::BSGeometry * source, RE::NiNode * skeleton)
{
	RE::NiNode* rootNode = nullptr;
	RE::BSGeometry* foundGeometry = nullptr;
	RE::BSFixedString overlayName(nodeName);
	RE::NiAVObject* foundNode = skeleton->GetObjectByName(overlayName);
	if (foundNode)
		foundGeometry = foundNode ? foundNode->AsGeometry() : nullptr;

	if (source && foundGeometry)
	{
		foundGeometry->vertexDesc = source->vertexDesc;
		auto sourceSkin = source->GetGeometryRuntimeData().skinInstance;
		if (sourceSkin) {
			auto* clonedSkin = static_cast<RE::NiSkinInstance*>(sourceSkin->Clone());
			foundGeometry->GetGeometryRuntimeData().skinInstance.reset(clonedSkin);
		}
	}
}

RE::TESObjectARMA* GetArmorAddonByMask(RE::TESRace * race, RE::TESObjectARMO * armor, std::uint32_t mask)
{
	RE::TESObjectARMA* currentAddon = nullptr;
	for(std::uint32_t i = 0; i < armor->armorAddons.size(); i++)
	{
		currentAddon = armor->armorAddons[i];
		if(currentAddon->IsValidRace(race) && (currentAddon->GetSlotMask().underlying() & mask) == mask) {
			return currentAddon;
		}
	}

	return nullptr;
}

SKSETaskRevertOverlay::SKSETaskRevertOverlay(RE::TESObjectREFR * refr, RE::BSFixedString nodeName, std::uint32_t armorMask, std::uint32_t addonMask, bool resetDiffuse)
{
	m_formId = refr->formID;
	m_nodeName = nodeName;
	m_armorMask = armorMask;
	m_addonMask = addonMask;
	m_resetDiffuse = resetDiffuse;
}

void SKSETaskRevertOverlay::Run()
{
	RE::TESForm * form = RE::TESForm::LookupByID(m_formId);
	RE::TESObjectREFR * reference = form ? form->As<RE::TESObjectREFR>() : nullptr;
	if (reference && g_overlayInterface.HasOverlays(reference))
	{
		RE::Actor * actor = reference ? reference->As<RE::Actor>() : nullptr;
		if(actor)
		{
			RE::TESForm * form = GetSkinForm(actor, m_armorMask);
			if(RE::TESObjectARMO * armor = form ? form->As<RE::TESObjectARMO>() : nullptr)
			{
				RE::TESObjectARMA* foundAddon = GetArmorAddonByMask(actor->race, armor, m_addonMask);
				if(foundAddon)
				{
					VisitArmorAddon(actor, armor, foundAddon, [&](bool isFP, RE::NiNode * rootNode, RE::NiAVObject * armorNode)
					{
						RE::BSGeometry * firstSkin = GetFirstShaderType(armorNode, static_cast<std::uint32_t>(RE::BSShaderMaterial::Feature::kFaceGenRGBTint));
						if (firstSkin)
						{
							g_overlayInterface.ResetOverlay(m_nodeName.c_str(), actor, firstSkin, rootNode, NULL, m_resetDiffuse);
						}
					});
				}
			}
		}
	}
}

void SKSETaskRevertOverlay::Dispose()
{
	delete this;
}

SKSETaskRevertFaceOverlay::SKSETaskRevertFaceOverlay(RE::TESObjectREFR * refr, RE::BSFixedString nodeName, RE::BGSHeadPart::HeadPartType partType, RE::BSShaderMaterial::Feature shaderType, bool resetDiffuse)
{
	m_formId = refr->formID;
	m_nodeName = nodeName;
	m_partType = partType;
	m_shaderType = shaderType;
	m_resetDiffuse = resetDiffuse;
}

void SKSETaskRevertFaceOverlay::Run()
{
	RE::TESForm * form = RE::TESForm::LookupByID(m_formId);
	RE::TESObjectREFR * reference = form ? form->As<RE::TESObjectREFR>() : nullptr;
	if (reference && g_overlayInterface.HasOverlays(reference))
	{
		RE::Actor * actor = reference ? reference->As<RE::Actor>() : nullptr;
		if(actor)
		{
			RE::BSFaceGenNiNode* faceNode = GetFaceGenNiNode(static_cast<RE::Actor*>(reference));
			RE::TESNPC* actorBase = reference->GetBaseObject() ? reference->GetBaseObject()->As<RE::TESNPC>() : nullptr;
			RE::BGSHeadPart* headPart = actorBase->GetCurrentHeadPartByType(m_partType);
			RE::BSFixedString rootName("NPC Root [Root]");
			RE::NiNode* skeletonRoot = actor->Get3D(false) ? actor->Get3D(false)->AsNode() : nullptr;
			RE::BGSTextureSet* textureSet = nullptr;
			if(actorBase->headRelatedData)
				textureSet = actorBase->headRelatedData->faceDetails;

			if(skeletonRoot && faceNode && headPart)
			{
				RE::NiAVObject* root = skeletonRoot->GetObjectByName(rootName);
				if(root)
				{
					RE::NiNode* rootNode = root ? root->AsNode() : nullptr;
					if(rootNode)
					{
						RE::NiAVObject* headNode = faceNode->GetObjectByName(headPart->formEditorID.c_str());
						if(headNode)
						{
							RE::BSGeometry* firstFace = GetFirstShaderType(headNode, static_cast<std::uint32_t>(m_shaderType));
							if(firstFace)
							{
								g_overlayInterface.ResetOverlay(m_nodeName.c_str(), actor, firstFace, rootNode, textureSet, m_resetDiffuse);
							}
						}
					}
				}
			}
		}
	}
}

void SKSETaskRevertFaceOverlay::Dispose()
{
	delete this;
}

SKSETaskInstallOverlay::SKSETaskInstallOverlay(RE::TESObjectREFR * refr, RE::BSFixedString nodeName, RE::BSFixedString overlayPath, std::uint32_t armorMask, std::uint32_t addonMask)
{
	m_formId = refr->formID;
	m_nodeName = nodeName;
	m_overlayPath = overlayPath;
	m_armorMask = armorMask;
	m_addonMask = addonMask;
}

void SKSETaskInstallOverlay::Run()
{
	RE::TESForm* form = RE::TESForm::LookupByID(m_formId);
	RE::TESObjectREFR* reference = form ? form->As<RE::TESObjectREFR>() : nullptr;
	if (reference && g_overlayInterface.HasOverlays(reference))
	{
		RE::Actor* actor = reference ? reference->As<RE::Actor>() : nullptr;
		if(actor)
		{
#ifdef _DEBUG
			SKSE::log::debug("{} - Installing Overlay {} from {} to actor: {:08X}", __FUNCTION__, m_nodeName.c_str(), m_overlayPath.c_str(), actor->formID);
#endif
			RE::TESForm* form = GetSkinForm(actor, m_armorMask);
			if(RE::TESObjectARMO* armor = form ? form->As<RE::TESObjectARMO>() : nullptr)
			{
#ifdef _DEBUG
				SKSE::log::debug("{} - Installing Overlay to Armor: {:08X} on RE::Actor: {:08X}", __FUNCTION__, armor->formID, actor->formID);
#endif
				RE::TESObjectARMA* foundAddon = GetArmorAddonByMask(actor->race, armor, m_addonMask);
				if(foundAddon)
				{
					VisitArmorAddon(actor, armor, foundAddon, [&](bool isFirstPerson, RE::NiNode* skeleton, RE::NiAVObject* armorNode)
					{
						RE::BSGeometry* firstSkin = GetFirstShaderType(armorNode, static_cast<std::uint32_t>(RE::BSShaderMaterial::Feature::kFaceGenRGBTint));
						if (firstSkin)
						{
							RE::BSFixedString rootName("NPC Root [Root]");
							RE::NiAVObject* root = skeleton->GetObjectByName(rootName);
							if (RE::NiNode* rootNode = root ? root->AsNode() : nullptr) {
#ifdef _DEBUG
								SKSE::log::debug("{} - Installing Overlay {} to {:08X} on skeleton [{:#x}]", __FUNCTION__, m_nodeName.c_str(), actor->formID, (std::uintptr_t)skeleton);
#endif
								g_overlayInterface.InstallOverlay(m_nodeName.c_str(), m_overlayPath.c_str(), actor, firstSkin, rootNode);
							}
						}
					});
				}
#ifdef _DEBUG
				else {
					SKSE::log::debug("{} - Failed to locate addon by mask {} for Armor: {:08X} on RE::Actor: {:08X}", __FUNCTION__, m_addonMask, armor->formID, actor->formID);
				}
#endif
			}
		}
	}
}

void SKSETaskInstallOverlay::Dispose()
{
	delete this;
}

SKSETaskInstallFaceOverlay::SKSETaskInstallFaceOverlay(RE::TESObjectREFR * refr, RE::BSFixedString nodeName, RE::BSFixedString overlayPath, RE::BGSHeadPart::HeadPartType partType, RE::BSShaderMaterial::Feature shaderType)
{
	m_formId = refr->formID;
	m_nodeName = nodeName;
	m_overlayPath = overlayPath;
	m_partType = partType;
	m_shaderType = shaderType;
}

void SKSETaskInstallFaceOverlay::Run()
{
	RE::TESForm* form = RE::TESForm::LookupByID(m_formId);
	RE::TESObjectREFR* reference = form ? form->As<RE::TESObjectREFR>() : nullptr;
	if (reference && g_overlayInterface.HasOverlays(reference))
	{
		RE::Actor* actor = reference ? reference->As<RE::Actor>() : nullptr;
		if(actor)
		{
			RE::BSFaceGenNiNode* faceNode = GetFaceGenNiNode(static_cast<RE::Actor*>(reference));
			RE::TESNPC* actorBase = reference->GetBaseObject() ? reference->GetBaseObject()->As<RE::TESNPC>() : nullptr;
			RE::BGSHeadPart* headPart = actorBase->GetCurrentHeadPartByType(m_partType);
			RE::BSFixedString rootName("NPC Root [Root]");
			RE::NiNode* skeletonRoot = actor->Get3D(false) ? actor->Get3D(false)->AsNode() : nullptr;
			RE::BGSTextureSet* textureSet = nullptr;
			if(actorBase->headRelatedData)
				textureSet = actorBase->headRelatedData->faceDetails;

#ifdef _DEBUG
			SKSE::log::debug("{} - Installing Face Overlay {} from {} to actor: {:08X} - Face [{:#x}] Skeleton [{:#x}] HeadPart [{:#x}]", __FUNCTION__, m_nodeName.c_str(), m_overlayPath.c_str(), actor->formID, (std::uintptr_t)faceNode, (std::uintptr_t)skeletonRoot, (std::uintptr_t)headPart);
#endif
			if(skeletonRoot && faceNode && headPart)
			{
				RE::NiAVObject* root = skeletonRoot->GetObjectByName(rootName);
				if(root)
				{
					RE::NiNode* rootNode = root ? root->AsNode() : nullptr;
					if(rootNode)
					{
						// Already installed, update its visibility
						if(RE::NiAVObject* foundOverlay = root->GetObjectByName(m_nodeName))
						{
							bool hiddenFlag = (faceNode->flags & 0x01) == 0x01;
							if(hiddenFlag)
								foundOverlay->flags.set(RE::NiAVObject::Flag::kHidden);
							else
								foundOverlay->flags.reset(RE::NiAVObject::Flag::kHidden);
#ifdef _DEBUG
							SKSE::log::info("{} - Toggling Face Overlay {} to {:08X} on skeleton", __FUNCTION__, m_nodeName.c_str(), actor->formID);
#endif
							return;
						}

						RE::NiAVObject* headNode = faceNode->GetObjectByName(headPart->formEditorID.c_str());
						if(headNode)
						{
							RE::BSGeometry* firstFace = GetFirstShaderType(headNode, static_cast<std::uint32_t>(m_shaderType));
							if(firstFace)
							{
#ifdef _DEBUG
								SKSE::log::info("{} - Installing Face Overlay {} to {:08X} on skeleton", __FUNCTION__, m_nodeName.c_str(), actor->formID);
#endif
								g_overlayInterface.InstallOverlay(m_nodeName.c_str(), m_overlayPath.c_str(), actor, firstFace, rootNode, textureSet);
							}
						}
					}
				}
			}
		}
	}
}

void SKSETaskInstallFaceOverlay::Dispose()
{
	delete this;
}

SKSETaskModifyOverlay::SKSETaskModifyOverlay(RE::TESObjectREFR * refr, RE::BSFixedString nodeName)
{
	m_formId = refr->formID;
	m_nodeName = nodeName;
}

void SKSETaskModifyOverlay::Run()
{
	RE::TESForm* form = RE::TESForm::LookupByID(m_formId);
	RE::TESObjectREFR* reference = form ? form->As<RE::TESObjectREFR>() : nullptr;
	if (reference && g_overlayInterface.HasOverlays(reference))
	{
		RE::Actor* actor = reference ? reference->As<RE::Actor>() : nullptr;
		if (actor)
		{
			VisitSkeletalRoots(actor, [&](RE::NiNode* rootNode, bool isFirstPerson)
			{
				RE::NiAVObject* overlayNode = rootNode->GetObjectByName(m_nodeName);
				if (overlayNode)
				{
#ifdef _DEBUG
					SKSE::log::info("{} - Modifying Overlay {} from {:08X} on skeleton", __FUNCTION__, m_nodeName.c_str(), actor->formID);
#endif
					Modify(actor, overlayNode, rootNode);
				}
			});
		}
	}
}

void SKSETaskModifyOverlay::Dispose()
{
	delete this;
}

void SKSETaskUninstallOverlay::Modify(RE::TESObjectREFR * reference, RE::NiAVObject * targetNode, RE::NiNode * parent)
{
	g_overlayInterface.UninstallOverlay(targetNode->name.c_str(), reference, parent);
}

void OverlayInterface::SetupOverlay(std::uint32_t primaryCount, const char * primaryPath, const char * primaryNode, std::uint32_t secondaryCount, const char * secondaryPath, const char * secondaryNode, RE::TESObjectREFR * refr, RE::NiNode * boneTree, RE::NiAVObject * resultNode)
{
	RE::BSGeometry * skin = GetFirstShaderType(resultNode, static_cast<std::uint32_t>(RE::BSShaderMaterial::Feature::kFaceGenRGBTint));
	if(skin)
	{
#ifdef _DEBUG
		SKSE::log::info("{} - Installing body overlay for {} on {:08X}", __FUNCTION__, skin->name, refr->formID);
#endif
		for(std::uint32_t i = 0; i < primaryCount; i++)
		{
			auto nodeName = std::vformat(primaryNode, std::make_format_args(i));
			InstallOverlay(nodeName.c_str(), primaryPath, refr, skin, boneTree);
		}
		for(std::uint32_t i = 0; i < secondaryCount; i++)
		{
			auto nodeName = std::vformat(secondaryNode, std::make_format_args(i));
			InstallOverlay(nodeName.c_str(), secondaryPath, refr, skin, boneTree);
		}
	}
	else
	{
#ifdef _DEBUG
		SKSE::log::info("{} - Uninstalling body overlay on {:08X}", __FUNCTION__, refr->formID);
#endif
		for(std::uint32_t i = 0; i < primaryCount; i++)
		{
			auto nodeName = std::vformat(primaryNode, std::make_format_args(i));
			UninstallOverlay(nodeName.c_str(), refr, boneTree);
		}

		for(std::uint32_t i = 0; i < secondaryCount; i++)
		{
			auto nodeName = std::vformat(secondaryNode, std::make_format_args(i));
			UninstallOverlay(nodeName.c_str(), refr, boneTree);
		}
	}
}

void OverlayInterface::RelinkOverlays(std::uint32_t primaryCount, const char * primaryNode, std::uint32_t secondaryCount, const char * secondaryNode, RE::TESObjectREFR * refr, RE::NiNode * boneTree, RE::NiAVObject * resultNode)
{
	RE::BSGeometry* skin = GetFirstShaderType(resultNode, static_cast<std::uint32_t>(RE::BSShaderMaterial::Feature::kFaceGenRGBTint));
	if (skin)
	{
		for (std::uint32_t i = 0; i < primaryCount; i++)
		{
			auto nodeName = std::vformat(primaryNode, std::make_format_args(i));
			RelinkOverlay(nodeName.c_str(), refr, skin, boneTree);
		}
		for (std::uint32_t i = 0; i < secondaryCount; i++)
		{
			auto nodeName = std::vformat(secondaryNode, std::make_format_args(i));
			RelinkOverlay(nodeName.c_str(), refr, skin, boneTree);
		}
	}
}

void OverlayInterface::BuildOverlays(std::uint32_t armorMask, std::uint32_t addonMask, RE::TESObjectREFR * refr, RE::NiNode * boneTree, RE::NiAVObject * resultNode)
{
	if ((armorMask & (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody) == (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody && (addonMask & (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody) == (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody)
	{
		SetupOverlay(g_numBodyOverlays, BODY_MESH, BODY_NODE, g_numSpellBodyOverlays, BODY_MAGIC_MESH, BODY_NODE_SPELL, refr, boneTree, resultNode);
	}
	if ((armorMask & (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands) == (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands && (addonMask & (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands) == (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands)
	{
		SetupOverlay(g_numHandOverlays, HAND_MESH, HAND_NODE, g_numSpellHandOverlays, HAND_MAGIC_MESH, HAND_NODE_SPELL, refr, boneTree, resultNode);
	}
	if ((armorMask & (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet) == (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet && (addonMask & (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet) == (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet)
	{
		SetupOverlay(g_numFeetOverlays, FEET_MESH, FEET_NODE, g_numSpellFeetOverlays, FEET_MAGIC_MESH, FEET_NODE_SPELL, refr, boneTree, resultNode);
	}
}

void OverlayInterface::RebuildOverlays(std::uint32_t armorMask, std::uint32_t addonMask, RE::TESObjectREFR * refr, RE::NiNode * boneTree, RE::NiAVObject * resultNode)
{
	if ((armorMask & (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody) == (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody && (addonMask & (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody) == (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody)
	{
		RelinkOverlays(g_numBodyOverlays, BODY_NODE, g_numSpellBodyOverlays, BODY_NODE_SPELL, refr, boneTree, resultNode);
	}
	if ((armorMask & (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands) == (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands && (addonMask & (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands) == (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands)
	{
		RelinkOverlays(g_numHandOverlays, HAND_NODE, g_numSpellHandOverlays, HAND_NODE_SPELL, refr, boneTree, resultNode);
	}
	if ((armorMask & (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet) == (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet && (addonMask & (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet) == (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet)
	{
		RelinkOverlays(g_numFeetOverlays, FEET_NODE, g_numSpellFeetOverlays, FEET_NODE_SPELL, refr, boneTree, resultNode);
	}
}

void OverlayInterface::RevertOverlay(RE::TESObjectREFR * reference, const char* nodeName, skee_u32 armorMask, skee_u32 addonMask, bool resetDiffuse, bool immediate)
{
	if(!reference)
		return;

	if (!immediate) {
		SKEE_AddTask(g_task, new SKSETaskRevertOverlay(reference, nodeName, armorMask, addonMask, resetDiffuse));
	}
	else {
		SKSETaskRevertOverlay(reference, nodeName, armorMask, addonMask, resetDiffuse).Run();
	}
	
}

void OverlayInterface::EraseOverlays(RE::TESObjectREFR * reference, bool immediate)
{
	RevertOverlays(reference, true, immediate);

	std::lock_guard<std::recursive_mutex> locker(overlays.m_lock);
	overlays.m_data.erase(reference->formID);
}

void OverlayInterface::RevertOverlays(RE::TESObjectREFR * reference, bool resetDiffuse, bool immediate)
{
	if(!reference)
		return;

	if (!immediate) {
		SKEE_AddTask(g_task, new SKSETaskRevertOverlays(reference, resetDiffuse));
	}
	else {
		SKSETaskRevertOverlays(reference, resetDiffuse).Run();
	}
}

void OverlayInterface::RevertHeadOverlay(RE::TESObjectREFR * reference, const char* nodeName, skee_u32 partType, skee_u32 shaderType, bool resetDiffuse, bool immediate)
{
	if(!reference)
		return;

	if (!immediate) {
		SKEE_AddTask(g_task, new SKSETaskRevertFaceOverlay(reference, nodeName, static_cast<RE::BGSHeadPart::HeadPartType>(partType), static_cast<RE::BSShaderMaterial::Feature>(shaderType), resetDiffuse));
	}
	else {
		SKSETaskRevertFaceOverlay(reference, nodeName, static_cast<RE::BGSHeadPart::HeadPartType>(partType), static_cast<RE::BSShaderMaterial::Feature>(shaderType), resetDiffuse).Run();
	}
}

void OverlayInterface::RevertHeadOverlays(RE::TESObjectREFR * reference, bool resetDiffuse, bool immediate)
{
	if(!reference)
		return;

	// Face
	if (!immediate)
	{
		for (std::uint32_t i = 0; i < g_numFaceOverlays; i++)
		{
			SKEE_AddTask(g_task, new SKSETaskRevertFaceOverlay(reference, std::format(FACE_NODE, i).c_str(), RE::BGSHeadPart::HeadPartType::kFace, RE::BSShaderMaterial::Feature::kFaceGen, resetDiffuse));
		}
		for (std::uint32_t i = 0; i < g_numSpellFaceOverlays; i++)
		{
			SKEE_AddTask(g_task, new SKSETaskRevertFaceOverlay(reference, std::format(FACE_NODE_SPELL, i).c_str(), RE::BGSHeadPart::HeadPartType::kFace, RE::BSShaderMaterial::Feature::kFaceGen, resetDiffuse));
		}
	}
	else
	{
		for (std::uint32_t i = 0; i < g_numFaceOverlays; i++)
		{
			SKSETaskRevertFaceOverlay(reference, std::format(FACE_NODE, i).c_str(), RE::BGSHeadPart::HeadPartType::kFace, RE::BSShaderMaterial::Feature::kFaceGen, resetDiffuse).Run();
		}
		for (std::uint32_t i = 0; i < g_numSpellFaceOverlays; i++)
		{
			SKSETaskRevertFaceOverlay(reference, std::format(FACE_NODE_SPELL, i).c_str(), RE::BGSHeadPart::HeadPartType::kFace, RE::BSShaderMaterial::Feature::kFaceGen, resetDiffuse).Run();
		}
	}
	
}

bool OverlayInterface::HasOverlays(RE::TESObjectREFR * reference)
{
	if(reference == RE::PlayerCharacter::GetSingleton()) // Always true for the player
		return true;

	std::lock_guard<std::recursive_mutex> locker(overlays.m_lock);
	auto it = overlays.m_data.find(reference->formID);
	if(it != overlays.m_data.end())
		return true;

	return false;
}

void OverlayInterface::RemoveOverlays(RE::TESObjectREFR * reference, bool immediate)
{
	if(!reference || reference == RE::PlayerCharacter::GetSingleton()) // Cannot remove from player
		return;

	if (!immediate) {
		SKEE_AddTask(g_task, new SKSETaskRemoveOverlays(reference));
	}
	else {
		SKSETaskRemoveOverlays(reference).Run();
	}

	std::lock_guard<std::recursive_mutex> locker(overlays.m_lock);
	overlays.m_data.erase(reference->formID);
}

void OverlayInterface::AddOverlays(RE::TESObjectREFR * reference, bool immediate)
{
	if (!reference || reference == RE::PlayerCharacter::GetSingleton()) // Cannot add to player, already exists
		return;

	std::lock_guard<std::recursive_mutex> locker(overlays.m_lock);
	overlays.m_data.insert(reference->formID);

	QueueOverlayBuild(reference, immediate);
}

void OverlayInterface::QueueOverlayBuild(RE::TESObjectREFR* reference, bool immediate)
{
	if (!reference) // Cannot add to player, already exists
		return;

	if (!immediate) {
		SKEE_AddTask(g_task, new SKSETaskUpdateOverlays(reference));
	}
	else {
		SKSETaskUpdateOverlays(reference).Run();
	}
}

void OverlayInterface::Revert()
{
	std::lock_guard<std::recursive_mutex> locker(overlays.m_lock);
	for (auto & formId : overlays.m_data) {
		RE::TESForm* form = RE::TESForm::LookupByID(formId);
		if (form && form->IsActor())
		{
			RevertOverlays(static_cast<RE::TESObjectREFR*>(form), true);
		}
	}
	overlays.m_data.clear();
}

std::string & OverlayInterface::GetDefaultTexture()
{
	return defaultTexture;
}

void OverlayInterface::SetDefaultTexture(const std::string & newTexture)
{
	defaultTexture = newTexture;
}

skee_u32 OverlayInterface::GetOverlayCount(OverlayType type, OverlayLocation location)
{
	if (type == OverlayType::Normal) {
		switch (location) {
		case OverlayLocation::Body:	return g_numBodyOverlays;
		case OverlayLocation::Hand: return g_numHandOverlays;
		case OverlayLocation::Feet: return g_numFeetOverlays;
		case OverlayLocation::Face: return g_numFaceOverlays;
		}
	} else if (type == OverlayType::Spell) {
		switch (location) {
		case OverlayLocation::Body:	return g_numSpellBodyOverlays;
		case OverlayLocation::Hand: return g_numSpellHandOverlays;
		case OverlayLocation::Feet: return g_numSpellFeetOverlays;
		case OverlayLocation::Face: return g_numSpellFaceOverlays;
		}
	}
	return 0;
}

const char* OverlayInterface::GetOverlayFormat(OverlayType type, OverlayLocation location)
{
	if (type == OverlayType::Normal) {
		switch (location) {
		case OverlayLocation::Body:	return BODY_NODE;
		case OverlayLocation::Hand: return HAND_NODE;
		case OverlayLocation::Feet: return FEET_NODE;
		case OverlayLocation::Face: return FACE_NODE;
		}
	}
	else if (type == OverlayType::Spell) {
		switch (location) {
		case OverlayLocation::Body:	return BODY_NODE_SPELL;
		case OverlayLocation::Hand: return HAND_NODE_SPELL;
		case OverlayLocation::Feet: return FEET_NODE_SPELL;
		case OverlayLocation::Face: return FACE_NODE_SPELL;
		}
	}
	return nullptr;
}

bool OverlayInterface::RegisterInstallCallback(const char* key, OverlayInstallCallback cb)
{
	std::lock_guard<std::recursive_mutex> locker(m_callbacks.m_lock);
	auto it = m_callbacks.m_data.emplace(key, cb);
	return it.second;
}

bool OverlayInterface::UnregisterInstallCallback(const char* key)
{
	std::lock_guard<std::recursive_mutex> locker(m_callbacks.m_lock);
	size_t before = m_callbacks.m_data.size();
	m_callbacks.m_data.erase(key);
	return before != m_callbacks.m_data.size();
}

void OverlayHolder::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	std::lock_guard<std::recursive_mutex> locker(m_lock);
	for(auto it = m_data.begin(); it != m_data.end(); ++it)
	{
		if (!intfc->OpenRecord('AOVL', kVersion)) {
			SKSE::log::error("{} - Failed to open record", __FUNCTION__);
		}

		std::uint64_t handle = (*it);
		intfc->WriteRecordData(&handle, sizeof(handle));
	}
}

bool OverlayHolder::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	bool error = false;

	std::uint64_t handle = 0;
	// Key
	if (!intfc->ReadRecordData(&handle, sizeof(handle)))
	{
		SKSE::log::error("{} - Error loading overlay actor", __FUNCTION__);
		error = true;
		return error;
	}

	std::uint64_t newHandle = 0;
	// Skip if handle is no longer valid.
	if (!ResolveAnyHandle(intfc, handle, &newHandle)) {
		return error;
	}

	std::uint32_t formId = newHandle & 0xFFFFFFFF;

#ifdef _DEBUG
	SKSE::log::debug("{} - Loading overlay for %016llX", __FUNCTION__, newHandle);
#endif

	std::lock_guard<std::recursive_mutex> locker(m_lock);
	m_data.insert(formId);

	g_actorUpdateManager.AddOverlayUpdate(formId);

	return error;
}

void OverlayInterface::Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
	overlays.Save(intfc, kVersion);
}

bool OverlayInterface::Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion)
{
#ifdef _DEBUG
	SKSE::log::debug("{} Loading Overlays...", __FUNCTION__);
#endif
	return overlays.Load(intfc, kVersion);
}

void OverlayInterface::OnAttach(RE::TESObjectREFR * refr, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::NiAVObject * object, bool isFirstPerson, RE::NiNode * skeleton, RE::NiNode * root)
{
	if ((refr == RE::PlayerCharacter::GetSingleton() && g_playerOnly) || !g_playerOnly || HasOverlays(refr))
	{
		if (g_actorUpdateManager.isReverting() || g_actorUpdateManager.isNewGame())
		{
			g_actorUpdateManager.AddOverlayUpdate(refr->formID);
		}
		else
		{
			std::uint32_t armorMask = armor->GetSlotMask().underlying();
			std::uint32_t addonMask = addon->GetSlotMask().underlying();
			BuildOverlays(armorMask, addonMask, refr, root, object);
		}
	}
}

void OverlayInterface::Visit(std::function<void(std::uint32_t)> visitor)
{
	std::lock_guard<std::recursive_mutex> locker(overlays.m_lock);
	for(auto overlay : overlays.m_data)
	{
		visitor(overlay);
	}
}

void OverlayInterface::PrintDiagnostics()
{
	Console_Print("OverlayInterface Diagnostics:");
	std::lock_guard<std::recursive_mutex> locker(overlays.m_lock);
	Console_Print("\t%llu actors with overlays", overlays.m_data.size());
}

void SKSETaskUpdateOverlays::Run()
{
	if (g_enableOverlays)
	{
		RE::TESForm* form = RE::TESForm::LookupByID(m_formId);
		RE::TESObjectREFR* reference = form ? form->As<RE::TESObjectREFR>() : nullptr;
		if (reference && g_overlayInterface.HasOverlays(reference))
		{
			// Body
			for (std::uint32_t i = 0; i < g_numBodyOverlays; i++)
			{
				auto nodeName = std::format(BODY_NODE, i);
				SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
				SKSETaskInstallOverlay(reference, nodeName.c_str(), BODY_MESH, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody).Run();
			}
			for (std::uint32_t i = 0; i < g_numSpellBodyOverlays; i++)
			{
				auto nodeName = std::format(BODY_NODE_SPELL, i);
				SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
				SKSETaskInstallOverlay(reference, nodeName.c_str(), BODY_MAGIC_MESH, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody).Run();
			}

			// Hand
			for (std::uint32_t i = 0; i < g_numHandOverlays; i++)
			{
				auto nodeName = std::format(HAND_NODE, i);
				SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
				SKSETaskInstallOverlay(reference, nodeName.c_str(), HAND_MESH, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands).Run();
			}
			for (std::uint32_t i = 0; i < g_numSpellHandOverlays; i++)
			{
				auto nodeName = std::format(HAND_NODE_SPELL, i);
				SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
				SKSETaskInstallOverlay(reference, nodeName.c_str(), HAND_MAGIC_MESH, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands).Run();
			}

			// Feet
			for (std::uint32_t i = 0; i < g_numFeetOverlays; i++)
			{
				auto nodeName = std::format(FEET_NODE, i);
				SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
				SKSETaskInstallOverlay(reference, nodeName.c_str(), FEET_MESH, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet).Run();
			}
			for (std::uint32_t i = 0; i < g_numSpellFeetOverlays; i++)
			{
				auto nodeName = std::format(FEET_NODE_SPELL, i);
				SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
				SKSETaskInstallOverlay(reference, nodeName.c_str(), FEET_MAGIC_MESH, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet).Run();
			}

			// Face
			if (g_enableFaceOverlays)
			{
				for (std::uint32_t i = 0; i < g_numFaceOverlays; i++)
				{
					auto nodeName = std::format(FACE_NODE, i);
					SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
					SKSETaskInstallFaceOverlay(reference, nodeName.c_str(), FACE_MESH, RE::BGSHeadPart::HeadPartType::kFace, RE::BSShaderMaterial::Feature::kFaceGen).Run();
				}
				for (std::uint32_t i = 0; i < g_numSpellFaceOverlays; i++)
				{
					auto nodeName = std::format(FACE_NODE_SPELL, i);
					SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
					SKSETaskInstallFaceOverlay(reference, nodeName.c_str(), FACE_MAGIC_MESH, RE::BGSHeadPart::HeadPartType::kFace, RE::BSShaderMaterial::Feature::kFaceGen).Run();
				}
			}
		}
	}
}

void SKSETaskUpdateOverlays::Dispose()
{
	delete this;
}

SKSETaskUpdateOverlays::SKSETaskUpdateOverlays(RE::TESObjectREFR* refr)
{
	m_formId = refr->formID;
}

void SKSETaskRemoveOverlays::Run()
{
	RE::TESForm* form = RE::TESForm::LookupByID(m_formId);
	RE::TESObjectREFR* reference = form ? form->As<RE::TESObjectREFR>() : nullptr;
	if (reference)
	{
		// Body
		for (std::uint32_t i = 0; i < g_numBodyOverlays; i++)
		{
			auto nodeName = std::format(BODY_NODE, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}
		for (std::uint32_t i = 0; i < g_numSpellBodyOverlays; i++)
		{
			auto nodeName = std::format(BODY_NODE_SPELL, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}

		// Hand
		for (std::uint32_t i = 0; i < g_numHandOverlays; i++)
		{
			auto nodeName = std::format(HAND_NODE, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}
		for (std::uint32_t i = 0; i < g_numSpellHandOverlays; i++)
		{
			auto nodeName = std::format(HAND_NODE_SPELL, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}

		// Feet
		for (std::uint32_t i = 0; i < g_numFeetOverlays; i++)
		{
			auto nodeName = std::format(FEET_NODE, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}
		for (std::uint32_t i = 0; i < g_numSpellFeetOverlays; i++)
		{
			auto nodeName = std::format(FEET_NODE_SPELL, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}

		// Face
		for (std::uint32_t i = 0; i < g_numFaceOverlays; i++)
		{
			auto nodeName = std::format(FACE_NODE, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}
		for (std::uint32_t i = 0; i < g_numSpellFaceOverlays; i++)
		{
			auto nodeName = std::format(FACE_NODE_SPELL, i);
			SKSETaskUninstallOverlay(reference, nodeName.c_str()).Run();
		}
	}
}

void SKSETaskRemoveOverlays::Dispose()
{
	delete this;
}

SKSETaskRemoveOverlays::SKSETaskRemoveOverlays(RE::TESObjectREFR* refr)
{
	m_formId = refr->formID;
}

void SKSETaskRevertOverlays::Run()
{
	RE::TESForm* form = RE::TESForm::LookupByID(m_formId);
	RE::TESObjectREFR* reference = form ? form->As<RE::TESObjectREFR>() : nullptr;
	if (reference)
	{
		for (std::uint32_t i = 0; i < g_numBodyOverlays; i++)
		{
			auto nodeName = std::format(BODY_NODE, i);
			SKSETaskRevertOverlay(reference, nodeName.c_str(), (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody, m_resetDiffuse).Run();
		}
		for (std::uint32_t i = 0; i < g_numSpellBodyOverlays; i++)
		{
			auto nodeName = std::format(BODY_NODE_SPELL, i);
			SKSETaskRevertOverlay(reference, nodeName.c_str(), (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kBody, m_resetDiffuse).Run();
		}

		// Hand
		for (std::uint32_t i = 0; i < g_numHandOverlays; i++)
		{
			auto nodeName = std::format(HAND_NODE, i);
			SKSETaskRevertOverlay(reference, nodeName.c_str(), (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands, m_resetDiffuse).Run();
		}
		for (std::uint32_t i = 0; i < g_numSpellHandOverlays; i++)
		{
			auto nodeName = std::format(HAND_NODE_SPELL, i);
			SKSETaskRevertOverlay(reference, nodeName.c_str(), (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kHands, m_resetDiffuse).Run();
		}

		// Feet
		for (std::uint32_t i = 0; i < g_numFeetOverlays; i++)
		{
			auto nodeName = std::format(FEET_NODE, i);
			SKSETaskRevertOverlay(reference, nodeName.c_str(), (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, m_resetDiffuse).Run();
		}
		for (std::uint32_t i = 0; i < g_numSpellFeetOverlays; i++)
		{
			auto nodeName = std::format(FEET_NODE_SPELL, i);
			SKSETaskRevertOverlay(reference, nodeName.c_str(), (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, (std::uint32_t)RE::BGSBipedObjectForm::BipedObjectSlot::kFeet, m_resetDiffuse).Run();
		}

		// Face
		for (std::uint32_t i = 0; i < g_numFaceOverlays; i++)
		{
			auto nodeName = std::format(FACE_NODE, i);
			SKSETaskRevertFaceOverlay(reference, nodeName.c_str(), RE::BGSHeadPart::HeadPartType::kFace, RE::BSShaderMaterial::Feature::kFaceGen, m_resetDiffuse).Run();
		}
		for (std::uint32_t i = 0; i < g_numSpellFaceOverlays; i++)
		{
			auto nodeName = std::format(FACE_NODE_SPELL, i);
			SKSETaskRevertFaceOverlay(reference, nodeName.c_str(), RE::BGSHeadPart::HeadPartType::kFace, RE::BSShaderMaterial::Feature::kFaceGen, m_resetDiffuse).Run();
		}
	}
}

void SKSETaskRevertOverlays::Dispose()
{
	delete this;
}

SKSETaskRevertOverlays::SKSETaskRevertOverlays(RE::TESObjectREFR* refr, bool resetDiffuse)
{
	m_formId = refr->formID;
	m_resetDiffuse = resetDiffuse;
}
