#pragma once

class TESObjectREFR;
class NiAVObject;
class TESObjectARMO;
class TESObjectARMA;
class NiNode;
class NiAVObject;
class NiTransform;
class TESForm;
class BaseExtraList;
class BGSTextureSet;

using skee_u64 = uint64_t;
using skee_u32 = uint32_t;
using skee_i32 = int32_t;
using skee_u16 = uint16_t;
using skee_u8 = uint8_t;

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
	virtual void OnAttach(TESObjectREFR * refr, TESObjectARMO * armor, TESObjectARMA * addon, NiAVObject * object, bool isFirstPerson, NiNode * skeleton, NiNode * root) = 0;
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
		kCurrentPluginVersion = kPluginVersion4,
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
		virtual void Visit(TESObjectREFR*) = 0;
	};

	class MorphValueVisitor
	{
	public:
		virtual void Visit(TESObjectREFR *, const char*, const char*, float) = 0;
	};
	
	class MorphVisitor
	{
	public:
		virtual void Visit(TESObjectREFR *, const char*) = 0;
	};

	virtual void SetMorph(TESObjectREFR * actor, const char * morphName, const char * morphKey, float relative) = 0;
	virtual float GetMorph(TESObjectREFR * actor, const char * morphName, const char * morphKey) = 0;
	virtual void ClearMorph(TESObjectREFR * actor, const char * morphName, const char * morphKey) = 0;

	virtual float GetBodyMorphs(TESObjectREFR * actor, const char * morphName) = 0;
	virtual void ClearBodyMorphNames(TESObjectREFR * actor, const char * morphName) = 0;

	virtual void VisitMorphs(TESObjectREFR * actor, MorphVisitor & visitor) = 0;
	virtual void VisitKeys(TESObjectREFR * actor, const char * name, MorphKeyVisitor & visitor) = 0;
	virtual void VisitMorphValues(TESObjectREFR * actor, MorphValueVisitor & visitor) = 0;

	virtual void ClearMorphs(TESObjectREFR * actor) = 0;

	virtual void ApplyVertexDiff(TESObjectREFR * refr, NiAVObject * rootNode, bool erase = false) = 0;

	virtual void ApplyBodyMorphs(TESObjectREFR * refr, bool deferUpdate = true) = 0;
	virtual void UpdateModelWeight(TESObjectREFR * refr, bool immediate = false) = 0;

	virtual void SetCacheLimit(skee_u64 limit) = 0;
	virtual bool HasMorphs(TESObjectREFR * actor) = 0;
	virtual skee_u32 EvaluateBodyMorphs(TESObjectREFR * actor) = 0;

	virtual bool HasBodyMorph(TESObjectREFR * actor, const char * morphName, const char * morphKey) = 0;
	virtual bool HasBodyMorphName(TESObjectREFR * actor, const char * morphName) = 0;
	virtual bool HasBodyMorphKey(TESObjectREFR * actor, const char * morphKey) = 0;
	virtual void ClearBodyMorphKeys(TESObjectREFR * actor, const char * morphKey) = 0;
	virtual void VisitStrings(StringVisitor & visitor) = 0;
	virtual void VisitActors(ActorVisitor & visitor) = 0;
	virtual skee_u64 ClearMorphCache() = 0;
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

	virtual bool HasNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool HasNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool HasNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool HasNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;

	virtual void AddNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, Position& position) = 0; // X,Y,Z
	virtual void AddNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, Rotation& rotation) = 0; // Euler angles
	virtual void AddNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, float scale) = 0;
	virtual void AddNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name, skee_u32 scaleMode) = 0;

	virtual Position GetNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual Rotation GetNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual float GetNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual skee_u32 GetNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;

	virtual bool RemoveNodeTransformPosition(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool RemoveNodeTransformRotation(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool RemoveNodeTransformScale(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual bool RemoveNodeTransformScaleMode(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;

	virtual bool RemoveNodeTransform(TESObjectREFR* refr, bool firstPerson, bool isFemale, const char* node, const char* name) = 0;
	virtual void RemoveAllReferenceTransforms(TESObjectREFR* refr) = 0;

	virtual bool GetOverrideNodeTransform(TESObjectREFR* refr, bool firstPerson, bool isFemale, const char* node, const char* name, skee_u16 key, NiTransform* result) = 0;

	virtual void UpdateNodeAllTransforms(TESObjectREFR* ref) = 0;

	virtual void VisitNodes(TESObjectREFR* refr, bool firstPerson, bool isFemale, NodeVisitor& visitor) = 0;
	virtual void UpdateNodeTransforms(TESObjectREFR* ref, bool firstPerson, bool isFemale, const char* node) = 0;
};

class IAttachmentInterface : public IPluginInterface
{
public:
	virtual bool AttachMesh(TESObjectREFR* ref, const char* nifPath, const char* name, bool firstPerson, bool replace, const char** filter, skee_u32 filterSize, NiAVObject*& out, char* err, skee_u64 errBufLen) = 0;
	virtual bool DetachMesh(TESObjectREFR* ref, const char* name, bool firstPerson) = 0;
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
		TESForm* form = nullptr;
		BaseExtraList* extraData = nullptr;

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
		void SetDirect(TESForm* _baseForm, BaseExtraList* _extraData)
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

	virtual skee_u32 GetItemUniqueID(TESObjectREFR* reference, Identifier& identifier, bool makeUnique) = 0; // Make unique will create an identifier if it does not exist for the specified item
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

	virtual TESForm* GetFormFromUniqueID(skee_u32 uniqueID) = 0;
	virtual TESForm* GetOwnerOfUniqueID(skee_u32 uniqueID) = 0;

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
	using CommandCallback = bool (*)(TESObjectREFR* ref, const char* argumentString);
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
	virtual bool HasOverlays(TESObjectREFR* reference) = 0;
	virtual void AddOverlays(TESObjectREFR* reference, bool defer = true) = 0;
	virtual void RemoveOverlays(TESObjectREFR* reference, bool defer = true) = 0;
	virtual void RevertOverlays(TESObjectREFR* reference, bool resetDiffuse, bool defer = true) = 0;
	virtual void RevertOverlay(TESObjectREFR* reference, const char* nodeName, skee_u32 armorMask, skee_u32 addonMask, bool resetDiffuse, bool defer = true) = 0;
	virtual void EraseOverlays(TESObjectREFR* reference, bool defer = true) = 0;
	virtual void RevertHeadOverlays(TESObjectREFR* reference, bool resetDiffuse, bool defer = true) = 0;
	virtual void RevertHeadOverlay(TESObjectREFR* reference, const char* nodeName, skee_u32 partType, skee_u32 shaderType, bool resetDiffuse, bool defer = true) = 0;
	enum class OverlayType { Normal, Spell };
	enum class OverlayLocation { Body, Hand, Feet, Face };
	virtual skee_u32 GetOverlayCount(OverlayType type, OverlayLocation location) = 0;
	virtual const char* GetOverlayFormat(OverlayType type, OverlayLocation location) = 0;

	using OverlayInstallCallback = void (*)(TESObjectREFR* ref, NiAVObject* node);
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
		virtual void TextureSet(const BGSTextureSet* textureSet) = 0;
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
		virtual BGSTextureSet* TextureSet() { return nullptr; }
	};

	
	virtual bool HasArmorAddonNode(TESObjectREFR* refr, bool firstPerson, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, bool debug) = 0;

	virtual bool HasArmorOverride(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index) = 0;
	virtual void AddArmorOverride(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value) = 0;
	virtual bool GetArmorOverride(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& visitor) = 0;
	virtual void RemoveArmorOverride(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index) = 0;
	virtual void SetArmorProperties(TESObjectREFR* refr, bool immediate) = 0;
	virtual void SetArmorProperty(TESObjectREFR* refr, bool firstPerson, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate) = 0;
	virtual bool GetArmorProperty(TESObjectREFR* refr, bool firstPerson, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& value) = 0;
	virtual void ApplyArmorOverrides(TESObjectREFR* refr, TESObjectARMO* armor, TESObjectARMA* addon, NiAVObject* object, bool immediate) = 0;
	virtual void RemoveAllArmorOverrides() = 0;
	virtual void RemoveAllArmorOverridesByReference(TESObjectREFR* reference) = 0;
	virtual void RemoveAllArmorOverridesByArmor(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor) = 0;
	virtual void RemoveAllArmorOverridesByAddon(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon) = 0;
	virtual void RemoveAllArmorOverridesByNode(TESObjectREFR* refr, bool isFemale, TESObjectARMO* armor, TESObjectARMA* addon, const char* nodeName) = 0;

	virtual bool HasNodeOverride(TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index) = 0;
	virtual void AddNodeOverride(TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value) = 0;
	virtual bool GetNodeOverride(TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& visitor) = 0;
	virtual void RemoveNodeOverride(TESObjectREFR* refr, bool isFemale, const char* nodeName, skee_u16 key, skee_u8 index) = 0;
	virtual void SetNodeProperties(TESObjectREFR* refr, bool immediate) = 0;
	virtual void SetNodeProperty(TESObjectREFR* refr, bool firstPerson, const char* nodeName, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate) = 0;
	virtual bool GetNodeProperty(TESObjectREFR* refr, bool firstPerson, const char* nodeName, skee_u16 key, skee_u8 index, GetVariant& value) = 0;
	virtual void ApplyNodeOverrides(TESObjectREFR* refr, NiAVObject* object, bool immediate) = 0;
	virtual void RemoveAllNodeOverrides() = 0;
	virtual void RemoveAllNodeOverridesByReference(TESObjectREFR* reference) = 0;
	virtual void RemoveAllNodeOverridesByNode(TESObjectREFR* refr, bool isFemale, const char* nodeName) = 0;

	virtual bool HasSkinOverride(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index) = 0;
	virtual void AddSkinOverride(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, SetVariant& value) = 0;
	virtual bool GetSkinOverride(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, GetVariant& visitor) = 0;
	virtual void RemoveSkinOverride(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index) = 0;
	virtual void SetSkinProperties(TESObjectREFR* refr, bool immediate) = 0;
	virtual void SetSkinProperty(TESObjectREFR* refr, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, SetVariant& value, bool immediate) = 0;
	virtual bool GetSkinProperty(TESObjectREFR* refr, bool firstPerson, skee_u32 slotMask, skee_u16 key, skee_u8 index, GetVariant& value) = 0;
	virtual void ApplySkinOverrides(TESObjectREFR* refr, bool firstPerson, TESObjectARMO* armor, TESObjectARMA* addon, skee_u32 slotMask, NiAVObject* object, bool immediate) = 0;
	virtual void RemoveAllSkinOverrides() = 0;
	virtual void RemoveAllSkinOverridesByReference(TESObjectREFR* reference) = 0;
	virtual void RemoveAllSkinOverridesBySlot(TESObjectREFR* refr, bool isFemale, bool firstPerson, skee_u32 slotMask) = 0;
};

class IPresetInterface : public IPluginInterface
{
public:
	enum
	{
		kPluginVersion1 = 1,
		kCurrentPluginVersion = kPluginVersion1,
	};

	// TODO: Create wrapper to Get/Set PresetData
	// SaveJsonPreset
	// LoadJsonPreset
};
