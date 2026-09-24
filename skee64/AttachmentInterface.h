#pragma once

#include "IPluginInterface.h"
#include <RE/B/BSFixedString.h>
#include <RE/N/NiAVObject.h>
#include <RE/T/TESObjectREFR.h>

#include <vector>
#include <unordered_set>
#include <mutex>
#include <cstdint>
class SKSEAttachSkinnedMesh : public SKEETaskDelegate
{
public:
	virtual void Run() override;
	virtual void Dispose() override { delete this; }

	SKSEAttachSkinnedMesh(RE::TESObjectREFR* ref, const RE::BSFixedString& nifPath, const RE::BSFixedString& name, bool firstPerson, bool replace, const std::vector<RE::BSFixedString>& filter);

protected:
	std::uint32_t							m_formId;
	RE::BSFixedString					m_nifPath;
	RE::BSFixedString					m_name;
	bool							m_firstPerson;
	bool							m_replace;
	std::vector<RE::BSFixedString>		m_filter;
};

class SKSEDetachSkinnedMesh : public SKEETaskDelegate
{
public:
	virtual void Run() override;
	virtual void Dispose() override { delete this; }

	SKSEDetachSkinnedMesh(RE::TESObjectREFR* ref, const RE::BSFixedString& name, bool firstPerson);

protected:
	std::uint32_t							m_formId;
	RE::BSFixedString					m_name;
	bool							m_firstPerson;
};

class SKSEDetachAllSkinnedMeshes : public SKEETaskDelegate
{
public:
	virtual void Run() override;
	virtual void Dispose() override { delete this; }

	SKSEDetachAllSkinnedMeshes(std::uint32_t formId) : m_formId(formId) { }

protected:
	std::uint32_t	m_formId;
};

class AttachmentInterface : public IAttachmentInterface
{
public:
	static const char* ATTACHMENT_HOLDER;

	enum
	{
		kCurrentPluginVersion = 1,
		kSerializationVersion = 0
	};
	virtual skee_u32 GetVersion() override { return kCurrentPluginVersion; };
	virtual void Revert() override;

	virtual bool AttachMesh(RE::TESObjectREFR* ref, const char* nifPath, const char* name, bool firstPerson, bool replace, const char** filter, skee_u32 filterSize, RE::NiAVObject*& out, char* err, skee_u64 errBufLen) override;
	virtual bool DetachMesh(RE::TESObjectREFR* ref, const char* name, bool firstPerson) override;

protected:
	std::mutex m_attachedLock;
	std::unordered_set<std::uint32_t> m_attached;
};
