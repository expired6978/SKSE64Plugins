#pragma once

#include "IPluginInterface.h"
#include "SafeDataHolder.h"
#include <cstdint>
#include <RE/B/BGSHeadPart.h>
#include <RE/B/BSFaceGenNiNode.h>
#include <RE/B/BSShaderMaterial.h>
#include <RE/B/BSGeometry.h>
#include <RE/B/BSTextureSet.h>
#include <RE/N/NiNode.h>
#include <RE/T/TESModelTextureSwap.h>
#include <RE/T/TESObjectARMA.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/T/TESObjectREFR.h>

#include <unordered_set>
#include <functional>

#define FACE_NODE "Face [Ovl{}]"
#define FACE_NODE_SPELL "Face [SOvl{}]"
#define FACE_MESH "meshes\\actors\\character\\character assets\\face_overlay.nif"
#define FACE_MAGIC_MESH "meshes\\actors\\character\\character assets\\face_magicoverlay.nif"

#define HAIR_NODE "Hair [Ovl{}]"
#define HAIR_NODE_SPELL "Hair [SOvl{}]"
#define HAIR_MESH "meshes\\actors\\character\\character assets\\hair_overlay.nif"
#define HAIR_MAGIC_MESH "meshes\\actors\\character\\character assets\\hair_magicoverlay.nif"

#define BODY_NODE "Body [Ovl{}]"
#define BODY_NODE_SPELL "Body [SOvl{}]"
#define BODY_MESH "meshes\\actors\\character\\character assets\\body_overlay.nif"
#define BODY_MAGIC_MESH "meshes\\actors\\character\\character assets\\body_magicoverlay.nif"

#define HAND_NODE "Hands [Ovl{}]"
#define HAND_NODE_SPELL "Hands [SOvl{}]"
#define HAND_MESH "meshes\\actors\\character\\character assets\\hands_overlay.nif"
#define HAND_MAGIC_MESH "meshes\\actors\\character\\character assets\\hands_magicoverlay.nif"

#define FEET_NODE "Feet [Ovl{}]"
#define FEET_NODE_SPELL "Feet [SOvl{}]"
#define FEET_MESH "meshes\\actors\\character\\character assets\\feet_overlay.nif"
#define FEET_MAGIC_MESH "meshes\\actors\\character\\character assets\\feet_magicoverlay.nif"

class SKSETaskRevertOverlay : public SKSE::detail::TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	SKSETaskRevertOverlay(RE::TESObjectREFR * refr, RE::BSFixedString nodeName, std::uint32_t armorMask, std::uint32_t addonMask, bool resetDiffuse);

	std::uint32_t			m_formId;
	RE::BSFixedString	m_nodeName;
	std::uint32_t			m_armorMask;
	std::uint32_t			m_addonMask;
	bool			m_resetDiffuse;
};

class SKSETaskRevertFaceOverlay : public SKSE::detail::TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	SKSETaskRevertFaceOverlay(RE::TESObjectREFR * refr, RE::BSFixedString nodeName, RE::BGSHeadPart::HeadPartType partType, RE::BSShaderMaterial::Feature shaderType, bool resetDiffuse);

	std::uint32_t			m_formId;
	RE::BSFixedString	m_nodeName;
	RE::BGSHeadPart::HeadPartType	m_partType;
	RE::BSShaderMaterial::Feature	m_shaderType;
	bool			m_resetDiffuse;
};

class SKSETaskInstallFaceOverlay : public SKSE::detail::TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	SKSETaskInstallFaceOverlay(RE::TESObjectREFR * refr, RE::BSFixedString nodeName, RE::BSFixedString overlayPath, RE::BGSHeadPart::HeadPartType partType, RE::BSShaderMaterial::Feature shaderType);

	std::uint32_t			m_formId;
	RE::BSFixedString	m_nodeName;
	RE::BSFixedString	m_overlayPath;
	RE::BGSHeadPart::HeadPartType	m_partType;
	RE::BSShaderMaterial::Feature	m_shaderType;
};

class SKSETaskInstallOverlay : public SKSE::detail::TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	SKSETaskInstallOverlay(RE::TESObjectREFR * refr, RE::BSFixedString nodeName, RE::BSFixedString overlayPath, std::uint32_t armorMask, std::uint32_t addonMask);

	std::uint32_t			m_formId;
	RE::BSFixedString	m_nodeName;
	RE::BSFixedString	m_overlayPath;
	std::uint32_t			m_armorMask;
	std::uint32_t			m_addonMask;
};

class SKSETaskModifyOverlay : public SKSE::detail::TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();
	virtual void Modify(RE::TESObjectREFR * reference, RE::NiAVObject * targetNode, RE::NiNode * parent) = 0;

	SKSETaskModifyOverlay(RE::TESObjectREFR * refr, RE::BSFixedString nodeName);

	std::uint32_t			m_formId;
	RE::BSFixedString	m_nodeName;
};

class SKSETaskUninstallOverlay : public SKSETaskModifyOverlay
{
public:
	virtual void Modify(RE::TESObjectREFR * reference, RE::NiAVObject * targetNode, RE::NiNode * parent);

	SKSETaskUninstallOverlay(RE::TESObjectREFR * refr, RE::BSFixedString nodeName) : SKSETaskModifyOverlay(refr, nodeName){};
};

class SKSETaskUpdateOverlays : public SKSE::detail::TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	SKSETaskUpdateOverlays(RE::TESObjectREFR* refr);
	
private:
	std::uint32_t	m_formId;
};

class SKSETaskRemoveOverlays : public SKSE::detail::TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	SKSETaskRemoveOverlays(RE::TESObjectREFR* refr);

private:
	std::uint32_t	m_formId;
};

class SKSETaskRevertOverlays : public SKSE::detail::TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	SKSETaskRevertOverlays(RE::TESObjectREFR* refr, bool resetDiffuse);

private:
	std::uint32_t	m_formId;
	bool	m_resetDiffuse;
};

class OverlayHolder : public SafeDataHolder<std::unordered_set<std::uint32_t>>
{
public:
	void Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion);
	bool Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion);

	friend class OverlayInterface;
};

class OverlayCallbackHolder : public SafeDataHolder<std::map<std::string, IOverlayInterface::OverlayInstallCallback>>
{
public:
	friend class OverlayInterface;
};

class OverlayInterface
	: public IOverlayInterface
	, public IAddonAttachmentInterface
{
public:
	virtual skee_u32 GetVersion() override;

	void Save(SKSE::SerializationInterface * intfc, std::uint32_t kVersion);
	bool Load(SKSE::SerializationInterface * intfc, std::uint32_t kVersion);
	virtual void Revert() override;

	virtual bool HasOverlays(RE::TESObjectREFR * reference) override;
	virtual void AddOverlays(RE::TESObjectREFR * reference, bool immediate = false) override;
	virtual void RemoveOverlays(RE::TESObjectREFR * reference, bool immediate = false) override;
	virtual void RevertOverlays(RE::TESObjectREFR * reference, bool resetDiffuse, bool immediate = false) override;
	virtual void RevertOverlay(RE::TESObjectREFR * reference, const char* nodeName, skee_u32 armorMask, skee_u32 addonMask, bool resetDiffuse, bool immediate = false) override;
	virtual void EraseOverlays(RE::TESObjectREFR * reference, bool immediate = false) override;
	virtual void RevertHeadOverlays(RE::TESObjectREFR * reference, bool resetDiffuse, bool immediate = false) override;
	virtual void RevertHeadOverlay(RE::TESObjectREFR * reference, const char* nodeName, skee_u32 partType, skee_u32 shaderType, bool resetDiffuse, bool immediate = false) override;
	virtual skee_u32 GetOverlayCount(OverlayType type, OverlayLocation location) override;
	virtual const char* GetOverlayFormat(OverlayType type, OverlayLocation location) override;

	virtual bool RegisterInstallCallback(const char* key, OverlayInstallCallback cb) override;
	virtual bool UnregisterInstallCallback(const char* key) override;

	virtual void SetupOverlay(std::uint32_t primaryCount, const char * primaryPath, const char * primaryNode, std::uint32_t secondaryCount, const char * secondaryPath, const char * secondaryNode, RE::TESObjectREFR * refr, RE::NiNode * boneTree, RE::NiAVObject * resultNode);

	virtual void UninstallOverlay(const char * nodeName, RE::TESObjectREFR * refr, RE::NiNode * parent);
	virtual void InstallOverlay(const char * nodeName, const char * path, RE::TESObjectREFR * refr, RE::BSGeometry * source, RE::NiNode * destination, RE::BGSTextureSet * textureSet = NULL);
	virtual void ResetOverlay(const char * nodeName, RE::TESObjectREFR * refr, RE::BSGeometry * source, RE::NiNode * destination, RE::BGSTextureSet * textureSet = NULL, bool resetDiffuse = false); // Re-applies the skin's textures

	virtual std::string & GetDefaultTexture();
	virtual void SetDefaultTexture(const std::string & newTexture);

	// Relinks an overlay node by name to the new source
	virtual void RelinkOverlay(const char * nodeName, RE::TESObjectREFR * refr, RE::BSGeometry * source, RE::NiNode * skeleton);

	// Relinks structured name of overlays to new source
	virtual void RelinkOverlays(std::uint32_t primaryCount, const char * primaryNode, std::uint32_t secondaryCount, const char * secondaryNode, RE::TESObjectREFR * refr, RE::NiNode * boneTree, RE::NiAVObject * resultNode);

	// Builds default overlays
	virtual void BuildOverlays(std::uint32_t armorMask, std::uint32_t addonMask, RE::TESObjectREFR * refr, RE::NiNode * boneTree, RE::NiAVObject * resultNode);

	// Relinks default overlays
	virtual void RebuildOverlays(std::uint32_t armorMask, std::uint32_t addonMask, RE::TESObjectREFR * refr, RE::NiNode * boneTree, RE::NiAVObject * resultNode);

	void QueueOverlayBuild(RE::TESObjectREFR* reference, bool immediate = false);

	void Visit(std::function<void(std::uint32_t)> visitor);
	void PrintDiagnostics();

private:
	std::string defaultTexture;
	OverlayHolder overlays;
	OverlayCallbackHolder m_callbacks;

	// Inherited via IAddonAttachmentInterface
	virtual void OnAttach(RE::TESObjectREFR * refr, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::NiAVObject * object, bool isFirstPerson, RE::NiNode * skeleton, RE::NiNode * root) override;
};
