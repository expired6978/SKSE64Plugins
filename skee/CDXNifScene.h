#ifdef FIXME

#ifndef __CDXNIFSCENE__
#define __CDXNIFSCENE__

#pragma once

#include "CDXEditableScene.h"

class BSRenderTargetGroup;
class BSScaleformImageLoader;
class Actor;
class NiNode;

class CDXNifScene : public CDXEditableScene
{
public:
	CDXNifScene();

	virtual void Setup(ID3D11Device * pDevice);
	virtual void Release();
	virtual void CreateBrushes();

	BSRenderTargetGroup * GetTextureGroup() { return m_textureGroup; }

	void SetWorkingActor(Actor * actor) { m_actor = actor; }
	Actor* GetWorkingActor() { return m_actor; }

	void SetImportRoot(NiNode * node) { m_importRoot = node; }
	NiNode * GetImportRoot() { return m_importRoot; }

	void ReleaseImport();

protected:
	Actor				* m_actor;
	BSRenderTargetGroup * m_textureGroup;
	NiNode				* m_importRoot;
};

#endif

#endif