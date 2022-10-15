#ifndef __CDXNIFSCENE__
#define __CDXNIFSCENE__

#pragma once

#include "CDXEditableScene.h"
#include "CDXRenderState.h"
#include "skse64/NiTypes.h"

#include <wrl/client.h>

class CDXD3DDevice;
class BSRenderTargetGroup;
class BSScaleformImageLoader;
class Actor;
class NiNode;
class NiTexture;

class CDXNifScene : public CDXEditableScene, public CDXRenderState
{
public:
	CDXNifScene();

	virtual bool Setup(const CDXInitParams & initParams) override;
	virtual void Release() override;
	virtual void CreateBrushes() override;

	virtual void Begin(CDXCamera * camera, CDXD3DDevice * device) override;
	virtual void End(CDXCamera * camera, CDXD3DDevice * device) override;

	bool CreateRenderTarget(CDXD3DDevice * device, UInt32 width, UInt32 height);

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> GetRenderTargetView() { return m_renderTargetView; }
	NiTexture* GetTexture() { return m_renderTexture; }

	void SetWorkingActor(Actor * actor) { m_actor = actor; }
	Actor* GetWorkingActor() { return m_actor; }

	void SetImportRoot(NiNode * node) { m_importRoot = node; }
	NiNode * GetImportRoot() { return m_importRoot; }

	void ReleaseImport();

protected:
	Actor*						m_actor;
	NiPointer<NiNode>			m_importRoot;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>		m_renderTargetView;
	NiPointer<NiTexture>		m_renderTexture;

	Microsoft::WRL::ComPtr<ID3D11Texture2D>			m_depthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState>	m_depthStencilState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView>	m_depthStencilView;
};

#endif
