#include "CDXD3DDevice.h"
#include "CDXCamera.h"
#include "CDXNifScene.h"
#include "CDXNifMesh.h"
#include "CDXNifBrush.h"
#include "CDXMaterial.h"
#include "CDXShader.h"
#include "CDXBrushMesh.h"

#include "common/ICriticalSection.h"

#include "skse64/GameTypes.h"
#include "skse64/GameStreams.h"

#include "skse64/NiRenderer.h"
#include "skse64/NiTextures.h"
#include "skse64/NiProperties.h"
#include "skse64/NiNodes.h"

#include "skse64/ScaleformLoader.h"

#include "FileUtils.h"
#include "Utilities.h"

#include "CDXShaderCompile.h"
#include "CDXBSShaderResource.h"

#include <d3d11_3.h>

using namespace DirectX;

extern float	g_sculptBackgroundA;
extern float	g_sculptBackgroundR;
extern float	g_sculptBackgroundG;
extern float	g_sculptBackgroundB;
extern bool		g_sculptDrawBrushPoint;
extern bool		g_sculptDrawBrushRadius;

CDXNifScene::CDXNifScene() : CDXEditableScene()
{
	m_renderTexture = nullptr;
	m_renderTargetView = nullptr;
	m_currentBrush = CDXBrush::kBrushType_Smooth;
	m_actor = nullptr;
	m_depthStencilBuffer = nullptr;
	m_depthStencilState = nullptr;
	m_depthStencilView = nullptr;
}

void CDXNifScene::CreateBrushes()
{
	m_brushes.push_back(new CDXNifMaskAddBrush);
	m_brushes.push_back(new CDXNifMaskSubtractBrush);
	m_brushes.push_back(new CDXNifInflateBrush);
	m_brushes.push_back(new CDXNifDeflateBrush);
	m_brushes.push_back(new CDXNifSmoothBrush);
	m_brushes.push_back(new CDXNifMoveBrush);
}

bool CDXNifScene::Setup(const CDXInitParams & initParams)
{
	ScopedCriticalSection locker(&g_renderManager->lock);
	if (m_renderTexture)
		Release();

	auto device = initParams.device->GetDevice();
	if (!device) {
		_ERROR("%s - Failed to acquire device3", __FUNCTION__);
		return false;
	}

	auto deviceContext = initParams.device->GetDeviceContext();
	if (!deviceContext) {
		_ERROR("%s - Failed to acquire deviceContext4", __FUNCTION__);
		return false;
	}

	CDXBSShaderResource vsBrush("SKSE/Plugins/CharGen/Shaders/brush_vs.hlsl", "BrushVShader");
	CDXBSShaderResource pcvsBrush("SKSE/Plugins/CharGen/Shaders/Compiled/brush_vs.cso");
	CDXBSShaderResource psBrush("SKSE/Plugins/CharGen/Shaders/brush_ps.hlsl", "BrushPShader");
	CDXBSShaderResource pcpsBrush("SKSE/Plugins/CharGen/Shaders/Compiled/brush_ps.cso");

	CDXBrushMesh * bMesh = new CDXBrushMesh;
	if (bMesh->Create(initParams.device, false, DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f), initParams.factory, &vsBrush, &pcvsBrush, &psBrush, &pcpsBrush))
	{
		bMesh->SetDrawPoint(g_sculptDrawBrushPoint);
		bMesh->SetDrawRadius(g_sculptDrawBrushRadius);
		bMesh->SetVisible(false);
		AddMesh(bMesh);
	}
	else
	{
		delete bMesh;
	}
	
	CDXBrushMesh * bmMesh = new CDXBrushMesh;
	if (bmMesh->Create(initParams.device, true, DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f), DirectX::XMVectorSet(1.0f, 0.0f, 1.0f, 1.0f), initParams.factory, &vsBrush, &pcvsBrush, &psBrush, &pcpsBrush))
	{
		bmMesh->SetDrawPoint(g_sculptDrawBrushPoint);
		bmMesh->SetDrawRadius(g_sculptDrawBrushRadius);
		bmMesh->SetVisible(false);
		AddMesh(bmMesh);
	}
	else
	{
		delete bmMesh;
	}

	bool ret = CDXEditableScene::Setup(initParams);
	if (!ret) {
		return false;
	}

	return CreateRenderTarget(initParams.device, initParams.viewportWidth, initParams.viewportHeight);
}

bool CDXNifScene::CreateRenderTarget(CDXD3DDevice * pDevice, UInt32 width, UInt32 height)
{
	auto device = pDevice->GetDevice();

	BSScaleformImageLoader * imageLoader = GFxLoader::GetSingleton()->imageLoader;
	if (!imageLoader) {
		_ERROR("%s - No image loader found", __FUNCTION__);
		return false;
	}

	m_renderTexture = CreateSourceTexture("headMesh");
	if (!m_renderTexture) {
		_ERROR("%s - Failed to create head mesh", __FUNCTION__);
		return false;
	}

	auto rendererData = m_renderTexture->rendererData = new NiTexture::RendererData(width, height);

	D3D11_TEXTURE2D_DESC textureDesc;
	ZeroMemory(&textureDesc, sizeof(textureDesc));

	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;

	HRESULT result = device->CreateTexture2D(&textureDesc, NULL, &rendererData->texture);
	if (FAILED(result)) {
		_ERROR("%s - Failed to create render texture.", __FUNCTION__);
		return false;
	}

	D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
	ZeroMemory(&renderTargetViewDesc, sizeof(renderTargetViewDesc));
	renderTargetViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	renderTargetViewDesc.Texture2D.MipSlice = 0;

	result = device->CreateRenderTargetView(rendererData->texture, &renderTargetViewDesc, &m_renderTargetView);
	if (FAILED(result)) {
		_ERROR("%s - Failed to create render target view.", __FUNCTION__);
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
	ZeroMemory(&shaderResourceViewDesc, sizeof(shaderResourceViewDesc));
	shaderResourceViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
	shaderResourceViewDesc.Texture2D.MipLevels = 1;

	result = device->CreateShaderResourceView(rendererData->texture, &shaderResourceViewDesc, &rendererData->resourceView);
	if (FAILED(result)) {
		_ERROR("%s - Failed to create shader resource view.", __FUNCTION__);
		return false;
	}

	imageLoader->AddVirtualImage(m_renderTexture);

	D3D11_TEXTURE2D_DESC depthBufferDesc;
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;

	// Initialize the description of the depth buffer.
	ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));

	// Set up the description of the depth buffer.
	depthBufferDesc.Width = width;
	depthBufferDesc.Height = height;
	depthBufferDesc.MipLevels = 1;
	depthBufferDesc.ArraySize = 1;
	depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.SampleDesc.Quality = 0;
	depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthBufferDesc.CPUAccessFlags = 0;
	depthBufferDesc.MiscFlags = 0;

	// Create the texture for the depth buffer using the filled out description.
	result = device->CreateTexture2D(&depthBufferDesc, NULL, &m_depthStencilBuffer);
	if (FAILED(result)) {
		_ERROR("%s - Failed to create DepthStencilBuffer", __FUNCTION__);
		return false;
	}

	// Initialize the description of the stencil state.
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

	// Set up the description of the stencil state.
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing.
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing.
	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Create the depth stencil state.
	result = device->CreateDepthStencilState(&depthStencilDesc, &m_depthStencilState);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create DepthStencilState", __FUNCTION__);
		return false;
	}

	// Initialize the depth stencil view.
	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

	// Set up the depth stencil view description.
	depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	// Create the depth stencil view.
	result = device->CreateDepthStencilView(m_depthStencilBuffer, &depthStencilViewDesc, &m_depthStencilView);
	if (FAILED(result))
	{
		_ERROR("%s - Failed to create DepthStencilView", __FUNCTION__);
		return false;
	}

	return true;
}

void CDXNifScene::Release()
{
	ScopedCriticalSection locker(&g_renderManager->lock);
	if(m_renderTexture) {
		BSScaleformImageLoader * imageLoader = GFxLoader::GetSingleton()->imageLoader;
		UInt8 ret = imageLoader->ReleaseVirtualImage(m_renderTexture);
		m_renderTexture = nullptr;
	}
	if (m_renderTargetView) {
		m_renderTargetView->Release();
		m_renderTargetView = nullptr;
	}

	ReleaseImport();

	m_actor = nullptr;

	if (m_depthStencilBuffer) {
		m_depthStencilBuffer->Release();
		m_depthStencilBuffer = nullptr;
	}
	if (m_depthStencilState) {
		m_depthStencilState->Release();
		m_depthStencilState = nullptr;
	}
	if (m_depthStencilView) {
		m_depthStencilView->Release();
		m_depthStencilView = nullptr;
	}

	CDXEditableScene::Release();
}

void CDXNifScene::ReleaseImport()
{
	if (m_importRoot) {
		m_importRoot->DecRef();
	}

	m_importRoot = nullptr;
}

void CDXNifScene::Begin(CDXCamera * camera, CDXD3DDevice * device)
{
	BackupRenderState(device);

	auto deviceContext = device->GetDeviceContext();

	// Setup the viewport for rendering.
	D3D11_VIEWPORT viewport;
	viewport.Width = (float)camera->GetWidth();
	viewport.Height = (float)camera->GetHeight();
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;

	// Create the viewport.
	deviceContext->RSSetViewports(1, &viewport);

	deviceContext->OMSetRenderTargets(1, &m_renderTargetView, m_depthStencilView);

	// Set the depth stencil state.
	deviceContext->OMSetDepthStencilState(m_depthStencilState, 1);

	float color[4];
	// Setup the color to clear the buffer to.
	color[0] = g_sculptBackgroundR;
	color[1] = g_sculptBackgroundG;
	color[2] = g_sculptBackgroundB;
	color[3] = g_sculptBackgroundA;

	// Clear the back buffer.
	deviceContext->ClearRenderTargetView(m_renderTargetView, color);

	// Clear the depth buffer.
	deviceContext->ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void CDXNifScene::End(CDXCamera * camera, CDXD3DDevice * device)
{
	RestoreRenderState(device);
}
