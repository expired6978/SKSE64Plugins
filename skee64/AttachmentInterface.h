#pragma once

#include "skee64/IPluginInterface.h"
#include "skse64/GameTypes.h"
#include "skse64/GameThreads.h"
#include "skse64/NiTypes.h"

#include <vector>
#include <unordered_set>
#include <mutex>

class Actor;

class SKSEAttachSkinnedMesh : public TaskDelegate
{
public:
	virtual void Run() override;
	virtual void Dispose() override { delete this; }

	SKSEAttachSkinnedMesh(TESObjectREFR* ref, const BSFixedString& nifPath, const BSFixedString& name, bool firstPerson, bool replace, const std::vector<BSFixedString>& filter);

protected:
	UInt32							m_formId;
	BSFixedString					m_nifPath;
	BSFixedString					m_name;
	bool							m_firstPerson;
	bool							m_replace;
	std::vector<BSFixedString>		m_filter;
};

class SKSEDetachSkinnedMesh : public TaskDelegate
{
public:
	virtual void Run() override;
	virtual void Dispose() override { delete this; }

	SKSEDetachSkinnedMesh(TESObjectREFR* ref, const BSFixedString& name, bool firstPerson);

protected:
	UInt32							m_formId;
	BSFixedString					m_name;
	bool							m_firstPerson;
};

class SKSEDetachAllSkinnedMeshes : public TaskDelegate
{
public:
	virtual void Run() override;
	virtual void Dispose() override { delete this; }

	SKSEDetachAllSkinnedMeshes(UInt32 formId) : m_formId(formId) { }

protected:
	UInt32	m_formId;
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

	virtual bool AttachMesh(TESObjectREFR* ref, const char* nifPath, const char* name, bool firstPerson, bool replace, const char** filter, skee_u32 filterSize, NiAVObject*& out, char* err, skee_u64 errBufLen) override;
	virtual bool DetachMesh(TESObjectREFR* ref, const char* name, bool firstPerson) override;

protected:
	std::mutex m_attachedLock;
	std::unordered_set<UInt32> m_attached;
};