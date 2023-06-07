#pragma once

#include "IPluginInterface.h"

class TESObjectARMO;
class TESObjectARMA;
class TESModelTextureSwap;
class BGSTextureSet;
class TESObjectREFR;
class BSFaceGenNiNode;

struct SKSESerializationInterface;

class NiNode;
class BSGeometry;

#include "skse64/GameThreads.h"
#include "skse64/GameTypes.h"

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

class SKSETaskRevertOverlay : public TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	SKSETaskRevertOverlay(TESObjectREFR * refr, BSFixedString nodeName, UInt32 armorMask, UInt32 addonMask, bool resetDiffuse);

	UInt32			m_formId;
	BSFixedString	m_nodeName;
	UInt32			m_armorMask;
	UInt32			m_addonMask;
	bool			m_resetDiffuse;
};

class SKSETaskRevertFaceOverlay : public TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	SKSETaskRevertFaceOverlay(TESObjectREFR * refr, BSFixedString nodeName, UInt32 partType, UInt32 shaderType, bool resetDiffuse);

	UInt32			m_formId;
	BSFixedString	m_nodeName;
	UInt32			m_partType;
	UInt32			m_shaderType;
	bool			m_resetDiffuse;
};

class SKSETaskInstallFaceOverlay : public TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	SKSETaskInstallFaceOverlay(TESObjectREFR * refr, BSFixedString nodeName, BSFixedString overlayPath, UInt32 partType, UInt32 shaderType);

	UInt32			m_formId;
	BSFixedString	m_nodeName;
	BSFixedString	m_overlayPath;
	UInt32			m_partType;
	UInt32			m_shaderType;
};

class SKSETaskInstallOverlay : public TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	SKSETaskInstallOverlay(TESObjectREFR * refr, BSFixedString nodeName, BSFixedString overlayPath, UInt32 armorMask, UInt32 addonMask);

	UInt32			m_formId;
	BSFixedString	m_nodeName;
	BSFixedString	m_overlayPath;
	UInt32			m_armorMask;
	UInt32			m_addonMask;
};

class SKSETaskModifyOverlay : public TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();
	virtual void Modify(TESObjectREFR * reference, NiAVObject * targetNode, NiNode * parent) = 0;

	SKSETaskModifyOverlay(TESObjectREFR * refr, BSFixedString nodeName);

	UInt32			m_formId;
	BSFixedString	m_nodeName;
};

class SKSETaskUninstallOverlay : public SKSETaskModifyOverlay
{
public:
	virtual void Modify(TESObjectREFR * reference, NiAVObject * targetNode, NiNode * parent);

	SKSETaskUninstallOverlay(TESObjectREFR * refr, BSFixedString nodeName) : SKSETaskModifyOverlay(refr, nodeName){};
};

class SKSETaskUpdateOverlays : public TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	SKSETaskUpdateOverlays(TESObjectREFR* refr);
	
private:
	UInt32	m_formId;
};

class SKSETaskRemoveOverlays : public TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	SKSETaskRemoveOverlays(TESObjectREFR* refr);

private:
	UInt32	m_formId;
};

class SKSETaskRevertOverlays : public TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose();

	SKSETaskRevertOverlays(TESObjectREFR* refr, bool resetDiffuse);

private:
	UInt32	m_formId;
	bool	m_resetDiffuse;
};

class OverlayHolder : public SafeDataHolder<std::unordered_set<UInt32>>
{
public:
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion);

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

	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion);
	virtual void Revert() override;

	virtual bool HasOverlays(TESObjectREFR * reference) override;
	virtual void AddOverlays(TESObjectREFR * reference, bool immediate = false) override;
	virtual void RemoveOverlays(TESObjectREFR * reference, bool immediate = false) override;
	virtual void RevertOverlays(TESObjectREFR * reference, bool resetDiffuse, bool immediate = false) override;
	virtual void RevertOverlay(TESObjectREFR * reference, const char* nodeName, skee_u32 armorMask, skee_u32 addonMask, bool resetDiffuse, bool immediate = false) override;
	virtual void EraseOverlays(TESObjectREFR * reference, bool immediate = false) override;
	virtual void RevertHeadOverlays(TESObjectREFR * reference, bool resetDiffuse, bool immediate = false) override;
	virtual void RevertHeadOverlay(TESObjectREFR * reference, const char* nodeName, skee_u32 partType, skee_u32 shaderType, bool resetDiffuse, bool immediate = false) override;
	virtual skee_u32 GetOverlayCount(OverlayType type, OverlayLocation location) override;
	virtual const char* GetOverlayFormat(OverlayType type, OverlayLocation location) override;

	virtual bool RegisterInstallCallback(const char* key, OverlayInstallCallback cb) override;
	virtual bool UnregisterInstallCallback(const char* key) override;

	virtual void SetupOverlay(UInt32 primaryCount, const char * primaryPath, const char * primaryNode, UInt32 secondaryCount, const char * secondaryPath, const char * secondaryNode, TESObjectREFR * refr, NiNode * boneTree, NiAVObject * resultNode);

	virtual void UninstallOverlay(const char * nodeName, TESObjectREFR * refr, NiNode * parent);
	virtual void InstallOverlay(const char * nodeName, const char * path, TESObjectREFR * refr, BSGeometry * source, NiNode * destination, BGSTextureSet * textureSet = NULL);
	virtual void ResetOverlay(const char * nodeName, TESObjectREFR * refr, BSGeometry * source, NiNode * destination, BGSTextureSet * textureSet = NULL, bool resetDiffuse = false); // Re-applies the skin's textures

	virtual std::string & GetDefaultTexture();
	virtual void SetDefaultTexture(const std::string & newTexture);

	// Relinks an overlay node by name to the new source
	virtual void RelinkOverlay(const char * nodeName, TESObjectREFR * refr, BSGeometry * source, NiNode * skeleton);

	// Relinks structured name of overlays to new source
	virtual void RelinkOverlays(UInt32 primaryCount, const char * primaryNode, UInt32 secondaryCount, const char * secondaryNode, TESObjectREFR * refr, NiNode * boneTree, NiAVObject * resultNode);

	// Builds default overlays
	virtual void BuildOverlays(UInt32 armorMask, UInt32 addonMask, TESObjectREFR * refr, NiNode * boneTree, NiAVObject * resultNode);

	// Relinks default overlays
	virtual void RebuildOverlays(UInt32 armorMask, UInt32 addonMask, TESObjectREFR * refr, NiNode * boneTree, NiAVObject * resultNode);

	void QueueOverlayBuild(TESObjectREFR* reference, bool immediate = false);

	void Visit(std::function<void(UInt32)> visitor);
	void PrintDiagnostics();

private:
	std::string defaultTexture;
	OverlayHolder overlays;
	OverlayCallbackHolder m_callbacks;

	// Inherited via IAddonAttachmentInterface
	virtual void OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root) override;
};
