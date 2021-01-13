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
	virtual void Run();
	virtual void Dispose() { delete this; }

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
	virtual void Run();
	virtual void Dispose() { delete this; }

	SKSEDetachSkinnedMesh(TESObjectREFR* ref, const BSFixedString& name, bool firstPerson);

protected:
	UInt32							m_formId;
	BSFixedString					m_name;
	bool							m_firstPerson;
};

class SKSEDetachAllSkinnedMeshes : public TaskDelegate
{
public:
	virtual void Run();
	virtual void Dispose() { delete this; }

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
	virtual UInt32 GetVersion() { return kCurrentPluginVersion; };
	virtual void Revert();

	virtual bool AttachMesh(TESObjectREFR* ref, const char* nifPath, const char* name, bool firstPerson, bool replace, const char** filter, UInt32 filterSize, NiAVObject*& out, char* err, size_t errBufLen);
	virtual bool DetachMesh(TESObjectREFR* ref, const char* name, bool firstPerson);

protected:
	std::mutex m_attachedLock;
	std::unordered_set<UInt32> m_attached;
};