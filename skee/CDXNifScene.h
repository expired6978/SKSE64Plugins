#ifndef __CDXNIFSCENE__
#define __CDXNIFSCENE__

#pragma once

#include "CDXEditableScene.h"
#include "skse64/NiTypes.h"

class CDXD3DDevice;
class BSRenderTargetGroup;
class BSScaleformImageLoader;
class Actor;
class NiNode;
class NiTexture;

class CDXNifScene : public CDXEditableScene
{
public:
	CDXNifScene();

	virtual bool Setup(const CDXInitParams & initParams) override;
	virtual void Release() override;
	virtual void CreateBrushes() override;

	virtual void Begin(CDXCamera * camera, CDXD3DDevice * device) override;
	virtual void End(CDXCamera * camera, CDXD3DDevice * device) override;

	bool CreateRenderTarget(CDXD3DDevice * device, UInt32 width, UInt32 height);

	ID3D11RenderTargetView* GetRenderTargetView() { return m_renderTargetView; }
	NiTexture* GetTexture() { return m_renderTexture; }

	void SetWorkingActor(Actor * actor) { m_actor = actor; }
	Actor* GetWorkingActor() { return m_actor; }

	void SetImportRoot(NiNode * node) { m_importRoot = node; }
	NiNode * GetImportRoot() { return m_importRoot; }

	void ReleaseImport();

	void BackupRenderState(CDXD3DDevice * device);
	void RestoreRenderState(CDXD3DDevice * device);

protected:
	Actor*						m_actor;
	NiPointer<NiNode>			m_importRoot;
	ID3D11RenderTargetView*		m_renderTargetView;
	NiPointer<NiTexture>		m_renderTexture;

	ID3D11Texture2D*			m_depthStencilBuffer;
	ID3D11DepthStencilState*	m_depthStencilState;
	ID3D11DepthStencilView*		m_depthStencilView;

	struct BACKUP_DX11_STATE
	{
		UINT                        ScissorRectsCount, ViewportsCount;
		D3D11_RECT                  ScissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		D3D11_VIEWPORT              Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
		ID3D11RasterizerState*      RS;
		ID3D11BlendState*           BlendState;
		FLOAT                       BlendFactor[4];
		UINT                        SampleMask;
		UINT                        StencilRef;
		ID3D11DepthStencilState*    DepthStencilState;
		ID3D11ShaderResourceView*   PSShaderResource;
		ID3D11SamplerState*         PSSampler;
		ID3D11PixelShader*          PS;
		ID3D11VertexShader*         VS;
		UINT                        PSInstancesCount, VSInstancesCount;
		ID3D11ClassInstance*        PSInstances[256], *VSInstances[256];   // 256 is max according to PSSetShader documentation
		D3D11_PRIMITIVE_TOPOLOGY    PrimitiveTopology;
		ID3D11Buffer*               IndexBuffer, *VertexBuffer, *VSConstantBuffer;
		UINT                        IndexBufferOffset, VertexBufferStride, VertexBufferOffset;
		DXGI_FORMAT                 IndexBufferFormat;
		ID3D11InputLayout*          InputLayout;
		ID3D11RenderTargetView*		RenderTargetViews[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
		ID3D11DepthStencilView*		DepthStencilView;
	};

	BACKUP_DX11_STATE m_backupState;
};

#endif
