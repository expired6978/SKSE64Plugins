#pragma once

#include "skse64/GameThreads.h"
#include "skse64/GameTypes.h"

#include "skse64/NiTypes.h"

#include <functional>

class NiSourceTexture;
class NiRenderedTexture;
class NiTriBasedGeom;
class TESNPC;
class BGSHeadPart;
class BSFaceGenNiNode;
class BSGeometry;
class NiGeometry;
class NiTriStripsData;

class SKSETaskExportHead : public TaskDelegate
{
	virtual void Run();
	virtual void Dispose() { delete this; }

public:
	SKSETaskExportHead(Actor * actor, BSFixedString nifPath, BSFixedString ddsPath);

	UInt32			m_formId;
	BSFixedString	m_nifPath;
	BSFixedString	m_ddsPath;
};

class SKSETaskExportTintMask : public TaskDelegate
{
	virtual void Run();
	virtual void Dispose() { delete this; }

public:
	SKSETaskExportTintMask(BSFixedString filePath, BSFixedString fileName) : m_filePath(filePath), m_fileName(fileName) {};

	BSFixedString	m_filePath;
	BSFixedString	m_fileName;
};

class SKSETaskRefreshTintMask : public TaskDelegate
{
	virtual void Run();
	virtual void Dispose() { delete this; };

public:
	SKSETaskRefreshTintMask::SKSETaskRefreshTintMask(Actor * actor, BSFixedString ddsPath);

	UInt32			m_formId;
	BSFixedString	m_ddsPath;
};

BGSTextureSet * GetTextureSetForPart(TESNPC * npc, BGSHeadPart * headPart);
std::pair<BGSTextureSet*, BGSHeadPart*> GetTextureSetForPartByName(TESNPC * npc, BSFixedString partName);
BSGeometry * GetHeadGeometry(Actor * actor, UInt32 partType);
void ExportTintMaskDDS(Actor * actor, BSFixedString filePath);
NiAVObject * GetObjectByHeadPart(BSFaceGenNiNode * faceNode, BGSHeadPart * headPart);

bool SaveRenderedDDS(NiRenderedTexture * pkTexture, const char * pcFileName);

bool VisitObjects(NiAVObject * parent, std::function<bool(NiAVObject*)> functor);

NiTransform GetGeometryTransform(BSGeometry * geometry);
NiTransform GetLegacyGeometryTransform(NiGeometry * geometry);

UInt16	GetStripLengthSum(NiTriStripsData * strips);
void GetTriangleIndices(NiTriStripsData * strips, UInt16 i, UInt16 v0, UInt16 v1, UInt16 v2);
