#pragma once

namespace RE
{
	class Actor;
	class BGSTextureSet;
	class BSGeometry;
	class ExtraDataList;
	class NiAVObject;
	class NiBinaryExtraData;
	class NiNode;
	class NiSkinPartition;
	class NiTransform;
	class TESForm;
	class TESObjectARMA;
	class TESObjectARMO;
	class TESObjectREFR;
}

using skee_u64 = std::uint64_t;
using skee_u32 = std::uint32_t;
using skee_i32 = std::int32_t;
using skee_u16 = std::uint16_t;
using skee_u8 = std::uint8_t;

class IPluginInterface
{
public:
	IPluginInterface() { };
	virtual ~IPluginInterface() { };

	virtual skee_u32 GetVersion() = 0;
	virtual void Revert() = 0;
};

class IInterfaceMap 
{
public:
	virtual IPluginInterface * QueryInterface(const char * name) = 0;
	virtual bool AddInterface(const char * name, IPluginInterface * pluginInterface) = 0;
	virtual IPluginInterface * RemoveInterface(const char * name) = 0;
};

struct InterfaceExchangeMessage
{
	enum
	{
		kMessage_ExchangeInterface = 0x9E3779B9
	};

	IInterfaceMap * interfaceMap = nullptr;
};

class IAddonAttachmentInterface
{
public:
	virtual void OnAttach(RE::TESObjectREFR * refr, RE::TESObjectARMO * armor, RE::TESObjectARMA * addon, RE::NiAVObject * object, bool isFirstPerson, RE::NiNode * skeleton, RE::NiNode * root) = 0;
};

// Allows mods which change an actor's rendered equipment without changing the
// inventory entry (variants, transmog, outfit visualizers, hidden equipment,
// and similar systems) to describe the resulting visual state to RaceMenu.
//
// RaceMenu never owns provider or visitor objects. Providers must remain alive
// until unregistered and must coordinate unregistration with any in-flight
// query. Only POD values and virtual visitors cross the DLL boundary; STL
// containers and std::function deliberately do not.
class IVisualEquipmentProvider
{
public:
	enum ResolveResult : skee_u32
	{
		kUnhandled = 0,
		kHandled = 1
	};

	class ArmorAddonVisitor
	{
	public:
		virtual ~ArmorAddonVisitor() = default;

		// Return true to continue enumeration, false to stop.
		virtual bool Visit(RE::TESObjectARMO* armor, RE::TESObjectARMA* addon) = 0;
	};

	virtual ~IVisualEquipmentProvider() = default;

	// Resolve the effective rendered slot mask for a source inventory armor or
	// one of its source armor addons. sourceAddon is null for an armor-level
	// query. A handled result may return a zero mask to mean visually hidden.
	virtual ResolveResult ResolveSlotMask(
		RE::Actor* actor,
		RE::TESObjectARMO* sourceArmor,
		RE::TESObjectARMA* sourceAddon,
		skee_u32 sourceMask,
		skee_u32* effectiveMask) = 0;

	// Enumerate the effective rendered (ARMO, ARMA) pairs corresponding to one
	// source pair. A handled result with no visits means deliberately hidden.
	// Return kUnhandled to retain RaceMenu's ordinary source-pair traversal;
	// visits made by an unhandled provider are discarded.
	virtual ResolveResult VisitArmorAddons(
		RE::Actor* actor,
		RE::TESObjectARMO* sourceArmor,
		RE::TESObjectARMA* sourceAddon,
		ArmorAddonVisitor* visitor) = 0;
};

class IVisualEquipmentInterface : public IPluginInterface
{
public:
	enum
	{
		kPluginVersion1 = 1,
		kCurrentPluginVersion = kPluginVersion1,
	};

	// Higher priority providers are queried first. The first provider returning
	// kHandled owns that query. Provider keys must be unique.
	virtual bool RegisterProvider(const char* key, skee_i32 priority, IVisualEquipmentProvider* provider) = 0;
	virtual bool UnregisterProvider(const char* key, IVisualEquipmentProvider* provider) = 0;

	// Public query surface for RaceMenu and other consumers.
	virtual IVisualEquipmentProvider::ResolveResult ResolveSlotMask(
		RE::Actor* actor,
		RE::TESObjectARMO* sourceArmor,
		RE::TESObjectARMA* sourceAddon,
		skee_u32 sourceMask,
		skee_u32* effectiveMask) = 0;
	virtual IVisualEquipmentProvider::ResolveResult VisitArmorAddons(
		RE::Actor* actor,
		RE::TESObjectARMO* sourceArmor,
		RE::TESObjectARMA* sourceAddon,
		IVisualEquipmentProvider::ArmorAddonVisitor* visitor) = 0;

	// Reapply RaceMenu visual state after a provider changes an actor's mapping.
	// Calls must be made while the actor is valid.
	virtual bool NotifyChanged(RE::Actor* actor) = 0;
};

class IBodyMorphInterface : public IPluginInterface
{
public:
	enum
	{
		kPluginVersion1 = 1,
		kPluginVersion2,
		kPluginVersion3,
		kPluginVersion4,
		kPluginVersion5, // Added AddMorphShapeCallback
		kCurrentPluginVersion = kPluginVersion5,
		kSerializationVersion1 = 1,
		kSerializationVersion2,
		kSerializationVersion3,
		kSerializationVersion = kSerializationVersion3
	};
	class MorphKeyVisitor
	{
	public:
		virtual void Visit(const char*, float) = 0;
	};

	class StringVisitor
	{
	public:
		virtual void Visit(const char*) = 0;
	};

	class ActorVisitor
	{
	public:
		virtual void Visit(RE::TESObjectREFR*) = 0;
	};

	class MorphValueVisitor
	{
	public:
		virtual void Visit(RE::TESObjectREFR *, const char*, const char*, float) = 0;
	};
	
	class MorphVisitor
	{
	public:
		virtual void Visit(RE::TESObjectREFR *, const char*) = 0;
	};

	virtual void SetMorph(RE::TESObjectREFR * actor, const char * morphName, const char * morphKey, float relative) = 0;
	virtual float GetMorph(RE::TESObjectREFR * actor, const char * morphName, const char * morphKey) = 0;
	virtual void ClearMorph(RE::TESObjectREFR * actor, const char * morphName, const char * morphKey) = 0;

	virtual float GetBodyMorphs(RE::TESObjectREFR * actor, const char * morphName) = 0;
	virtual void ClearBodyMorphNames(RE::TESObjectREFR * actor, const char * morphName) = 0;

	virtual void VisitMorphs(RE::TESObjectREFR * actor, MorphVisitor & visitor) = 0;
	virtual void VisitKeys(RE::TESObjectREFR * actor, const char * name, MorphKeyVisitor & visitor) = 0;
	virtual void VisitMorphValues(RE::TESObjectREFR * actor, MorphValueVisitor & visitor) = 0;

	virtual void ClearMorphs(RE::TESObjectREFR * actor) = 0;

	virtual void ApplyVertexDiff(RE::TESObjectREFR * refr, RE::NiAVObject * rootNode, bool erase = false) = 0;

	virtual void ApplyBodyMorphs(RE::TESObjectREFR * refr, bool deferUpdate = true) = 0;
	virtual void UpdateModelWeight(RE::TESObjectREFR * refr, bool immediate = false) = 0;

	virtual void SetCacheLimit(skee_u64 limit) = 0;
	virtual bool HasMorphs(RE::TESObjectREFR * actor) = 0;
	virtual skee_u32 EvaluateBodyMorphs(RE::TESObjectREFR * actor) = 0;

	virtual bool HasBodyMorph(RE::TESObjectREFR * actor, const char * morphName, const char * morphKey) = 0;
	virtual bool HasBodyMorphName(RE::TESObjectREFR * actor, const char * morphName) = 0;
	virtual bool HasBodyMorphKey(RE::TESObjectREFR * actor, const char * morphKey) = 0;
	virtual void ClearBodyMorphKeys(RE::TESObjectREFR * actor, const char * morphKey) = 0;
	virtual void VisitStrings(StringVisitor & visitor) = 0;
	virtual void VisitActors(ActorVisitor & visitor) = 0;
	virtual skee_u64 ClearMorphCache() = 0;

	// This callback occurs after RM alters the vertex buffer of the shape, but before it copies it to other partitions, and before re-upload to GPU
	// Order controls the execution order of registered callbacks, descending order
	using MorphShapeCallback = void (*)(RE::TESObjectREFR* refr, RE::NiAVObject* rootNode, RE::BSGeometry* geometry, RE::NiSkinPartition* skinPartition, RE::NiBinaryExtraData* unmodifiedVertexBuffer);
	virtual void AddMorphShapeCallback(MorphShapeCallback cb, skee_u64 order = 0) = 0;
};

class INiTransformInterface : public IPluginInterface
{
public:
	enum
	{
		kPluginVersion1 = 1,
		kPluginVersion2,
		kPluginVersion3,
		kCurrentPluginVersion = kPluginVersion3,
		kSerializationVersion1 = 1,
		kSerializationVersion2,
		kSerializationVersion3,
		kSerializationVersion = kSerializationVersion3
	};
	struct Position
	{
		float x, y, z;
	};
	struct Rotation
	{
		float heading, attitude, bank;
	};

	// Visits all overrides within a set
	class NodeVisitor
	{
	public:
		virtual bool VisitPosition(const char* node, const char* key, Position& position) = 0;
		virtual bool VisitRotation(const char* node, const char* key, Rotation& rotation) = 0;
		virtual bool VisitScale(const char* node, const char* key, float scale) = 0;
		virtual bool VisitScaleMode(const char* node, const char* key, skee_u32 scaleMode) = 0;
	};

	virtual bool HasNodeTransformPosition(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool HasNodeTransformRotation(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool HasNodeTransformScale(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool HasNodeTransformScaleMode(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;

	virtual void AddNodeTransformPosition(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, Position& position) = 0; // X,Y,Z
	virtual void AddNodeTransformRotation(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, Rotation& rotation) = 0; // Euler angles
	virtual void AddNodeTransformScale(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, float scale) = 0;
	virtual void AddNodeTransformScaleMode(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, skee_u32 scaleMode) = 0;

	virtual Position GetNodeTransformPosition(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual Rotation GetNodeTransformRotation(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual float GetNodeTransformScale(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual skee_u32 GetNodeTransformScaleMode(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;

	virtual bool RemoveNodeTransformPosition(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool RemoveNodeTransformRotation(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool RemoveNodeTransformScale(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool RemoveNodeTransformScaleMode(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;

	virtual bool RemoveNodeTransform(RE::TESObjectREFR* refr, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual void RemoveAllReferenceTransforms(RE::TESObjectREFR* refr) = 0;

	virtual bool GetOverrideNodeTransform(RE::TESObjectREFR* refr, bool firstPerson, bool isFemale, const char* node, const char* name, skee_u16 key, RE::NiTransform* result) = 0;

	virtual void UpdateNodeAllTransforms(RE::TESObjectREFR* ref) = 0;

	virtual void VisitNodes(RE::TESObjectREFR* refr, bool firstPerson, bool isFemale, NodeVisitor& visitor) = 0;
	virtual void UpdateNodeTransforms(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node) = 0;
};

class IAttachmentInterface : public IPluginInterface
{
public:
	virtual bool AttachMesh(RE::TESObjectREFR* ref, const char* nifPath, const char* name, bool firstPerson, bool replace, const char** filter, skee_u32 filterSize, RE::NiAVObject*& out, char* err, skee_u64 errBufLen) = 0;
	virtual bool DetachMesh(RE::TESObjectREFR* ref, const char* name, bool firstPerson) = 0;
};

class IItemDataInterface : public IPluginInterface
{
public:
	enum
	{
		kPluginVersion1 = 1,
		kPluginVersion2,
		kPluginVersion3, // Interfaces moved around so that IItemDataInterface appears first
		kCurrentPluginVersion = kPluginVersion3,
		kSerializationVersion1 = 1,
		kSerializationVersion2,
		kSerializationVersion = kSerializationVersion2
	};
	// Use this data structure to form an item query, this will identify a specific item through various means
	struct Identifier
	{
		enum
		{
			kTypeNone = 0,
			kTypeRank = (1 << 0),
			kTypeUID = (1 << 1),
			kTypeSlot = (1 << 2),
			kTypeSelf = (1 << 3),
			kTypeDirect = (1 << 4)
		};

		enum
		{
			kHandSlot_Left = 0,
			kHandSlot_Right
		};

		skee_u16			type = kTypeNone;
		skee_u16			uid = 0;
		skee_u32			ownerForm = 0;
		skee_u32			weaponSlot = 0;
		skee_u32			slotMask = 0;
		skee_u32			rankId = 0;
		RE::TESForm* form = nullptr;
		RE::ExtraDataList* extraData = nullptr;

		void SetRankID(skee_u32 _rank)
		{
			type |= kTypeRank;
			rankId = _rank;
		}
		void SetSlotMask(skee_u32 _slotMask, skee_u32 _weaponSlot = 0)
		{
			type |= kTypeSlot;
			slotMask = _slotMask;
			weaponSlot = _weaponSlot;
		}
		void SetUniqueID(skee_u16 _uid, skee_u32 _ownerForm)
		{
			type |= kTypeUID;
			uid = _uid;
			ownerForm = _ownerForm;
		}
		void SetDirect(RE::TESForm* _baseForm, RE::ExtraDataList* _extraData)
		{
			type |= kTypeDirect;
			form = _baseForm;
			extraData = _extraData;
		}

		bool IsDirect()
		{
			return (type & kTypeDirect) == kTypeDirect;
		}

		bool IsSelf()
		{
			return (type & kTypeSelf) == kTypeSelf;
		}

		void SetSelf()
		{
			type |= kTypeSelf;
		}
	};

	class StringVisitor
	{
	public:
		virtual void Visit(const char*) = 0;
	};

	virtual skee_u32 GetItemUniqueID(RE::TESObjectREFR* reference, Identifier& identifier, bool makeUnique) = 0; // Make unique will create an identifier if it does not exist for the specified item
	virtual void SetItemTextureLayerColor(skee_u32 uniqueID, skee_i32 textureIndex, skee_i32 layerIndex, skee_u32 color) = 0;
	virtual void SetItemTextureLayerType(skee_u32 uniqueID, skee_i32 textureIndex, skee_i32 layerIndex, skee_u32 type) = 0;
	virtual void SetItemTextureLayerBlendMode(skee_u32 uniqueID, skee_i32 textureIndex, skee_i32 layerIndex, const char* blendMode) = 0;
	virtual void SetItemTextureLayerTexture(skee_u32 uniqueID, skee_i32 textureIndex, skee_i32 layerIndex, const char* texture) = 0;

	virtual skee_u32 GetItemTextureLayerColor(skee_u32 uniqueID, skee_i32 textureIndex, skee_i32 layerIndex) = 0;
	virtual skee_u32 GetItemTextureLayerType(skee_u32 uniqueID, skee_i32 textureIndex, skee_i32 layerIndex) = 0;
	virtual bool GetItemTextureLayerBlendMode(skee_u32 uniqueID, skee_i32 textureIndex, skee_i32 layerIndex, StringVisitor& visitor) = 0;
	virtual bool GetItemTextureLayerTexture(skee_u32 uniqueID, skee_i32 textureIndex, skee_i32 layerIndex, StringVisitor& visitor) = 0;

	virtual void ClearItemTextureLayerColor(skee_u32 uniqueID, skee_i32 textureIndex, skee_i32 layerIndex) = 0;
	virtual void ClearItemTextureLayerType(skee_u32 uniqueID, skee_i32 textureIndex, skee_i32 layerIndex) = 0;
	virtual void ClearItemTextureLayerBlendMode(skee_u32 uniqueID, skee_i32 textureIndex, skee_i32 layerIndex) = 0;
	virtual void ClearItemTextureLayerTexture(skee_u32 uniqueID, skee_i32 textureIndex, skee_i32 layerIndex) = 0;
	virtual void ClearItemTextureLayer(skee_u32 uniqueID, skee_i32 textureIndex) = 0;

	virtual RE::TESForm* GetFormFromUniqueID(skee_u32 uniqueID) = 0;
	virtual RE::TESForm* GetOwnerOfUniqueID(skee_u32 uniqueID) = 0;

	// Generic key-value pair string interface
	virtual bool HasItemData(skee_u32 uniqueID, const char* key) = 0;
	virtual bool GetItemData(skee_u32 uniqueID, const char* key, StringVisitor& visitor) = 0;
	virtual void SetItemData(skee_u32 uniqueID, const char* key, const char* value) = 0;
	virtual void ClearItemData(skee_u32 uniqueID, const char* key) = 0;
};

class ICommandInterface : public IPluginInterface
{
public:
	// Return true indicates callback was handled and not to proceed to next command with the same command name
	using CommandCallback = bool (*)(RE::TESObjectREFR* ref, const char* argumentString);
	virtual bool RegisterCommand(const char* command, const char* desc, CommandCallback cb) = 0;
};

class IActorUpdateManager : public IPluginInterface
{
public:
	enum
	{
		kPluginVersion1 = 1,
		kPluginVersion2,
		kCurrentPluginVersion = kPluginVersion2,
	};
	virtual void AddBodyUpdate(skee_u32 formId) = 0;
	virtual void AddTransformUpdate(skee_u32 formId) = 0;
	virtual void AddOverlayUpdate(skee_u32 formId) = 0;
	virtual void AddNodeOverrideUpdate(skee_u32 formId) = 0;
	virtual void AddWeaponOverrideUpdate(skee_u32 formId) = 0;
	virtual void AddAddonOverrideUpdate(skee_u32 formId) = 0;
	virtual void AddSkinOverrideUpdate(skee_u32 formId) = 0;
	virtual void Flush() = 0;
	virtual void AddInterface(IAddonAttachmentInterface* observer) = 0;
	virtual void RemoveInterface(IAddonAttachmentInterface* observer) = 0;

	// Version 2
	using FlushCallback = void (*)(skee_u32* formId, skee_u32 length); // Array of FormIDs of length
	virtual bool RegisterFlushCallback(const char* key, FlushCallback cb) = 0;
	virtual bool UnregisterFlushCallback(const char* key) = 0;
};

class IOverlayInterface : public IPluginInterface
{
public:
	enum
	{
		kPluginVersion1 = 1,
		kPluginVersion2,
		kCurrentPluginVersion = kPluginVersion2,
		kSerializationVersion1 = 1,
		kSerializationVersion = kSerializationVersion1
	};
	virtual bool HasOverlays(RE::TESObjectREFR* reference) = 0;
	virtual void AddOverlays(RE::TESObjectREFR* reference, bool defer = true) = 0;
	virtual void RemoveOverlays(RE::TESObjectREFR* reference, bool defer = true) = 0;
	virtual void RevertOverlays(RE::TESObjectREFR* reference, bool resetDiffuse, bool defer = true) = 0;
	virtual void RevertOverlay(RE::TESObjectREFR* reference, const char* nodeName, skee_u32 armorMask, skee_u32 addonMask, bool resetDiffuse, bool defer = true) = 0;
	virtual void EraseOverlays(RE::TESObjectREFR* reference, bool defer = true) = 0;
	virtual void RevertHeadOverlays(RE::TESObjectREFR* reference, bool resetDiffuse, bool defer = true) = 0;
	virtual void RevertHeadOverlay(RE::TESObjectREFR* reference, const char* nodeName, skee_u32 partType, skee_u32 shaderType, bool resetDiffuse, bool defer = true) = 0;
	enum class OverlayType { Normal, Spell };
	enum class OverlayLocation { Body, Hand, Feet, Face };
	virtual skee_u32 GetOverlayCount(OverlayType type, OverlayLocation location) = 0;
	virtual const char* GetOverlayFormat(OverlayType type, OverlayLocation location) = 0;

	using OverlayInstallCallback = void (*)(RE::TESObjectREFR* ref, RE::NiAVObject* node);
	virtual bool RegisterInstallCallback(const char* key, OverlayInstallCallback cb) = 0;
	virtual bool UnregisterInstallCallback(const char* key) = 0;
};

class IOverrideInterface : public IPluginInterface
{
public:
	enum
	{
		kPluginVersion1 = 1,
		kPluginVersion2, // New version with wrapper interface
		kCurrentPluginVersion = kPluginVersion2,
		kSerializationVersion1 = 1,
		kSerializationVersion2,
		kSerializationVersion3,
		kSerializationVersion = kSerializationVersion3
	};

	class GetVariant
	{
	public:
		virtual void Int(const skee_i32 i) = 0;
		virtual void Float(const float f) = 0;
		virtual void String(const char* str) = 0;
		virtual void Bool(const bool b) = 0;
		virtual void TextureSet(const RE::BGSTextureSet* textureSet) = 0;
	};

	class SetVariant
	{
	public:
		enum class Type { None, Int, Float, String, Bool, TextureSet };
		virtual Type GetType() { return Type::None; } // Return the type you want to set
		virtual skee_i32 Int() { return 0; }
		virtual float Float() { return 0.0f; }
		virtual const char* String() { return nullptr; }
		virtual bool Bool() { return false; }
		virtual RE::BGSTextureSet* TextureSet() { return nullptr; }
	};

	
	virtual bool HasArmorAddonNode(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, bool debug) = 0;

	virtual bool HasArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index) = 0;
	virtual void AddArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value) = 0;
	virtual bool GetArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& visitor) = 0;
	virtual void RemoveArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index) = 0;
	virtual void SetArmorProperties(RE::TESObjectREFR* refr, bool immediate) = 0;
	virtual void SetArmorProperty(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate) = 0;
	virtual bool GetArmorProperty(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& value) = 0;
	virtual void ApplyArmorOverrides(RE::TESObjectREFR* refr, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, RE::NiAVObject* object, bool immediate) = 0;
	virtual void RemoveAllArmorOverrides() = 0;
	virtual void RemoveAllArmorOverridesByReference(RE::TESObjectREFR* reference) = 0;
	virtual void RemoveAllArmorOverridesByArmor(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor) = 0;
	virtual void RemoveAllArmorOverridesByAddon(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon) = 0;
	virtual void RemoveAllArmorOverridesByNode(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, const char* nodeName) = 0;

	virtual bool HasNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index) = 0;
	virtual void AddNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value) = 0;
	virtual bool GetNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& visitor) = 0;
	virtual void RemoveNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index) = 0;
	virtual void SetNodeProperties(RE::TESObjectREFR* refr, bool immediate) = 0;
	virtual void SetNodeProperty(RE::TESObjectREFR* refr, bool firstPerson, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate) = 0;
	virtual bool GetNodeProperty(RE::TESObjectREFR* refr, bool firstPerson, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& value) = 0;
	virtual void ApplyNodeOverrides(RE::TESObjectREFR* refr, RE::NiAVObject* object, bool immediate) = 0;
	virtual void RemoveAllNodeOverrides() = 0;
	virtual void RemoveAllNodeOverridesByReference(RE::TESObjectREFR* reference) = 0;
	virtual void RemoveAllNodeOverridesByNode(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName) = 0;

	virtual bool HasSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index) = 0;
	virtual void AddSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, SetVariant& value) = 0;
	virtual bool GetSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, GetVariant& visitor) = 0;
	virtual void RemoveSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index) = 0;
	virtual void SetSkinProperties(RE::TESObjectREFR* refr, bool immediate) = 0;
	virtual void SetSkinProperty(RE::TESObjectREFR* refr, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate) = 0;
	virtual bool GetSkinProperty(RE::TESObjectREFR* refr, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, GetVariant& value) = 0;
	virtual void ApplySkinOverrides(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon, skee_u32 slotMask, RE::NiAVObject* object, bool immediate) = 0;
	virtual void RemoveAllSkinOverrides() = 0;
	virtual void RemoveAllSkinOverridesByReference(RE::TESObjectREFR* reference) = 0;
	virtual void RemoveAllSkinOverridesBySlot(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask) = 0;
};

class IPresetInterface : public IPluginInterface
{
public:
	enum
	{
		kPluginVersion1 = 1,
		kCurrentPluginVersion = kPluginVersion1,
	};

	enum ApplyTypes
	{
		kPresetApplyFace = (0 << 0),
		kPresetApplyOverrides = (1 << 0),
		kPresetApplyBodyMorphs = (1 << 1),
		kPresetApplyTransforms = (1 << 2),
		kPresetApplySkinOverrides = (1 << 3),
		kPresetApplyAll = kPresetApplyFace | kPresetApplyOverrides | kPresetApplyBodyMorphs | kPresetApplyTransforms | kPresetApplySkinOverrides
	};

	// FilePath e.g. SKSE\\Plugins\\CharGen\\Exported\\name.jslot
	// TintPath e.g. Textures\\CharGen\\Exported\\name.dds
	// TintPath optional but recommended for correct look

	virtual bool SavePreset(const char* filePath, const char* tintPath, RE::Actor* actor) = 0;
	virtual bool LoadPreset(const char* filePath, const char* tintPath, RE::Actor* actor, ApplyTypes applyTypes = kPresetApplyAll) = 0; // Details may be saved to the RE::TESNPC, make sure this character is unique!
};

class IFormTagInterface : public IPluginInterface
{
public:
	enum
	{
		kPluginVersion1 = 1,
		kCurrentPluginVersion = kPluginVersion1,
	};

	virtual bool AddTag(RE::TESForm* form, const char* tag) = 0;
	virtual bool RemoveTag(RE::TESForm* form, const char* tag) = 0;
	virtual bool HasTags(RE::TESForm* form) = 0;
	virtual bool HasTag(RE::TESForm* form, const char* tag) = 0;

	class FormVisitor
	{
	public:
		virtual void Visit(RE::TESForm*) = 0;
	};

	class TagVisitor
	{
	public:
		virtual void Visit(const char*) = 0;
	};

	// Visits all Tags on a given EditorID
	virtual void GetTags(RE::TESForm* form, TagVisitor& visitor) = 0;

	// Visits all EditorIDs which have tags
	virtual void GetForms(FormVisitor& visitor) = 0;

	virtual bool AddPartTag(uint32_t partType, const char* tag, const char* label) = 0;
	virtual bool RemovePartTag(uint32_t partType, const char* tag) = 0;
	virtual bool HasPartTags(uint32_t partType) = 0;
	virtual bool HasPartTag(uint32_t partType, const char* tag) = 0;

	class PartTagVisitor
	{
	public:
		virtual void Visit(const char* name, const char* label) = 0;
	};
	virtual void GetPartTags(uint32_t partType, PartTagVisitor& visitor) = 0;
};