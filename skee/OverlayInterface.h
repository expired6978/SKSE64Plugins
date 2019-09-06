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

#include <set>

#define FACE_NODE "Face [Ovl%d]"
#define FACE_NODE_SPELL "Face [SOvl%d]"
#define FACE_MESH "meshes\\actors\\character\\character assets\\face_overlay.nif"
#define FACE_MAGIC_MESH "meshes\\actors\\character\\character assets\\face_magicoverlay.nif"

#define HAIR_NODE "Hair [Ovl%d]"
#define HAIR_NODE_SPELL "Hair [SOvl%d]"
#define HAIR_MESH "meshes\\actors\\character\\character assets\\hair_overlay.nif"
#define HAIR_MAGIC_MESH "meshes\\actors\\character\\character assets\\hair_magicoverlay.nif"

#define BODY_NODE "Body [Ovl%d]"
#define BODY_NODE_SPELL "Body [SOvl%d]"
#define BODY_MESH "meshes\\actors\\character\\character assets\\body_overlay.nif"
#define BODY_MAGIC_MESH "meshes\\actors\\character\\character assets\\body_magicoverlay.nif"

#define HAND_NODE "Hands [Ovl%d]"
#define HAND_NODE_SPELL "Hands [SOvl%d]"
#define HAND_MESH "meshes\\actors\\character\\character assets\\hands_overlay.nif"
#define HAND_MAGIC_MESH "meshes\\actors\\character\\character assets\\hands_magicoverlay.nif"

#define FEET_NODE "Feet [Ovl%d]"
#define FEET_NODE_SPELL "Feet [SOvl%d]"
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

class OverlayHolder : public std::set<UInt64>
{
public:
	void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	bool Load(SKSESerializationInterface * intfc, UInt32 kVersion);
};

class OverlayInterface
	: public IPluginInterface
	, public IAddonAttachmentInterface
{
public:
	enum
	{
		kCurrentPluginVersion = 1,
		kSerializationVersion1 = 1,
		kSerializationVersion = kSerializationVersion1
	};
	virtual UInt32 GetVersion();

	virtual void Save(SKSESerializationInterface * intfc, UInt32 kVersion);
	virtual bool Load(SKSESerializationInterface * intfc, UInt32 kVersion);
	virtual void Revert();

	virtual bool HasOverlays(TESObjectREFR * reference);
	virtual void AddOverlays(TESObjectREFR * reference);
	virtual void RemoveOverlays(TESObjectREFR * reference);
	virtual void RevertOverlays(TESObjectREFR * reference, bool resetDiffuse);
	virtual void RevertOverlay(TESObjectREFR * reference, BSFixedString nodeName, UInt32 armorMask, UInt32 addonMask, bool resetDiffuse);

	virtual void RevertHeadOverlays(TESObjectREFR * reference, bool resetDiffuse);
	virtual void RevertHeadOverlay(TESObjectREFR * reference, BSFixedString nodeName, UInt32 partType, UInt32 shaderType, bool resetDiffuse);

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

#ifdef _DEBUG
	void DumpMap();
#endif

private:
	std::string defaultTexture;
	OverlayHolder overlays;

	// Inherited via IAddonAttachmentInterface
	virtual void OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root) override;
};
