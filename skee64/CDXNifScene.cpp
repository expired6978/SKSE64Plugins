#include "CDXD3DDevice.h"
#include "CDXCamera.h"
#include "CDXNifScene.h"
#include "SKEEHooks.h"
#include "CDXNifMesh.h"
#include "CDXNifBrush.h"
#include "CDXMaterial.h"
#include "CDXShader.h"
#include "CDXBrushMesh.h"





#include "FileUtils.h"
#include "Utilities.h"

#include "CDXShaderCompile.h"
#include "CDXBSShaderResource.h"

#include "REX/W32/D3D11_3.h"
#include <cstdint>



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
	m_brushes.emplace_back(std::make_unique<CDXNifMaskAddBrush>());
	m_brushes.emplace_back(std::make_unique<CDXNifMaskSubtractBrush>());
	m_brushes.emplace_back(std::make_unique<CDXNifInflateBrush>());
	m_brushes.emplace_back(std::make_unique<CDXNifDeflateBrush>());
	m_brushes.emplace_back(std::make_unique<CDXNifSmoothBrush>());
	m_brushes.emplace_back(std::make_unique<CDXNifMoveBrush>());
}

bool CDXNifScene::Setup(const CDXInitParams & initParams)
{
	utils::ScopedCriticalSection locker(&RE::BSGraphics::Renderer::GetSingleton()->GetRendererData().lock);
	if (m_renderTexture)
		Release();

	auto device = initParams.device->GetDevice();
	if (!device.Get()) {
		SKSE::log::error("{} - Failed to acquire device3", __FUNCTION__);
		return false;
	}

	auto deviceContext = initParams.device->GetDeviceContext();
	if (!deviceContext.Get()) {
		SKSE::log::error("{} - Failed to acquire deviceContext4", __FUNCTION__);
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

bool CDXNifScene::CreateRenderTarget(CDXD3DDevice * pDevice, std::uint32_t width, std::uint32_t height)
{
	auto device = pDevice->GetDevice();

	RE::BSScaleformImageLoader * imageLoader = RE::BSScaleformManager::GetSingleton()->imageLoader.get();
	if (!imageLoader) {
		SKSE::log::error("{} - No image loader found", __FUNCTION__);
		return false;
	}

	m_renderTexture.reset(SKEE::CreateSourceTexture("headMesh"));
	if (!m_renderTexture) {
		SKSE::log::error("{} - Failed to create head mesh", __FUNCTION__);
		return false;
	}

	auto rendererData = new RE::NiTexture::RendererData(width, height);
	if (auto srcTex = static_cast<RE::NiSourceTexture*>(m_renderTexture.get())) {
		srcTex->rendererTexture = reinterpret_cast<RE::BSGraphics::Texture*>(rendererData);
	}

	REX::W32::D3D11_TEXTURE2D_DESC textureDesc;
	ZeroMemory(&textureDesc, sizeof(textureDesc));

	textureDesc.width = width;
	textureDesc.height = height;
	textureDesc.mipLevels = 1;
	textureDesc.arraySize = 1;
	textureDesc.format = REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.sampleDesc.count = 1;
	textureDesc.usage = REX::W32::D3D11_USAGE_DEFAULT;
	textureDesc.bindFlags = REX::W32::D3D11_BIND_RENDER_TARGET | REX::W32::D3D11_BIND_SHADER_RESOURCE;
	textureDesc.cpuAccessFlags = 0;
	textureDesc.miscFlags = 0;

	HRESULT result = device->CreateTexture2D(&textureDesc, NULL, reinterpret_cast<REX::W32::ID3D11Texture2D**>(&rendererData->texture));
	if (FAILED(result)) {
		SKSE::log::error("{} - Failed to create render texture.", __FUNCTION__);
		return false;
	}

	REX::W32::D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
	ZeroMemory(&renderTargetViewDesc, sizeof(renderTargetViewDesc));
	renderTargetViewDesc.format = REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM;
	renderTargetViewDesc.viewDimension = REX::W32::D3D11_RTV_DIMENSION_TEXTURE2D;
	renderTargetViewDesc.texture2D.mipSlice = 0;

	result = device->CreateRenderTargetView(reinterpret_cast<REX::W32::ID3D11Resource*>(rendererData->texture), &renderTargetViewDesc, m_renderTargetView.ReleaseAndGetAddressOf());
	if (FAILED(result)) {
		SKSE::log::error("{} - Failed to create render target view.", __FUNCTION__);
		return false;
	}

	REX::W32::D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
	ZeroMemory(&shaderResourceViewDesc, sizeof(shaderResourceViewDesc));
	shaderResourceViewDesc.format = REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM;
	shaderResourceViewDesc.viewDimension = REX::W32::D3D11_SRV_DIMENSION_TEXTURE2D;
	shaderResourceViewDesc.texture2D.mostDetailedMip = 0;
	shaderResourceViewDesc.texture2D.mipLevels = 1;

	result = device->CreateShaderResourceView(reinterpret_cast<REX::W32::ID3D11Resource*>(rendererData->texture), &shaderResourceViewDesc, reinterpret_cast<REX::W32::ID3D11ShaderResourceView**>(&rendererData->resourceView));
	if (FAILED(result)) {
		SKSE::log::error("{} - Failed to create shader resource view.", __FUNCTION__);
		return false;
	}

	RE::BSScaleformExternalTexture extTex;
		extTex.SetTexture(m_renderTexture.get());
		imageLoader->AddTexture(extTex);

	REX::W32::D3D11_TEXTURE2D_DESC depthBufferDesc;
	REX::W32::D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	REX::W32::D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;

	// Initialize the description of the depth buffer.
	ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));

	// Set up the description of the depth buffer.
	depthBufferDesc.width = width;
	depthBufferDesc.height = height;
	depthBufferDesc.mipLevels = 1;
	depthBufferDesc.arraySize = 1;
	depthBufferDesc.format = REX::W32::DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthBufferDesc.sampleDesc.count = 1;
	depthBufferDesc.sampleDesc.quality = 0;
	depthBufferDesc.usage = REX::W32::D3D11_USAGE_DEFAULT;
	depthBufferDesc.bindFlags = REX::W32::D3D11_BIND_DEPTH_STENCIL;
	depthBufferDesc.cpuAccessFlags = 0;
	depthBufferDesc.miscFlags = 0;

	// Create the texture for the depth buffer using the filled out description.
	result = device->CreateTexture2D(&depthBufferDesc, NULL, m_depthStencilBuffer.ReleaseAndGetAddressOf());
	if (FAILED(result)) {
		SKSE::log::error("{} - Failed to create DepthStencilBuffer", __FUNCTION__);
		return false;
	}

	// Initialize the description of the stencil state.
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

	// Set up the description of the stencil state.
	depthStencilDesc.depthEnable = true;
	depthStencilDesc.depthWriteMask = REX::W32::D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.depthFunc = REX::W32::D3D11_COMPARISON_LESS;

	depthStencilDesc.stencilEnable = true;
	depthStencilDesc.stencilReadMask = 0xFF;
	depthStencilDesc.stencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing.
	depthStencilDesc.frontFace.stencilFailOp = REX::W32::D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.frontFace.stencilDepthFailOp = REX::W32::D3D11_STENCIL_OP_INCR;
	depthStencilDesc.frontFace.stencilPassOp = REX::W32::D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.frontFace.stencilFunc = REX::W32::D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing.
	depthStencilDesc.backFace.stencilFailOp = REX::W32::D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.backFace.stencilDepthFailOp = REX::W32::D3D11_STENCIL_OP_DECR;
	depthStencilDesc.backFace.stencilPassOp = REX::W32::D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.backFace.stencilFunc = REX::W32::D3D11_COMPARISON_ALWAYS;

	// Create the depth stencil state.
	result = device->CreateDepthStencilState(&depthStencilDesc, m_depthStencilState.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		SKSE::log::error("{} - Failed to create DepthStencilState", __FUNCTION__);
		return false;
	}

	// Initialize the depth stencil view.
	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

	// Set up the depth stencil view description.
	depthStencilViewDesc.format = REX::W32::DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc.viewDimension = REX::W32::D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.texture2D.mipSlice = 0;

	// Create the depth stencil view.
	result = device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &depthStencilViewDesc, m_depthStencilView.ReleaseAndGetAddressOf());
	if (FAILED(result))
	{
		SKSE::log::error("{} - Failed to create DepthStencilView", __FUNCTION__);
		return false;
	}

	return true;
}

void CDXNifScene::Release()
{
	utils::ScopedCriticalSection locker(&RE::BSGraphics::Renderer::GetSingleton()->GetRendererData().lock);
	if(m_renderTexture) {
		RE::BSScaleformImageLoader * imageLoader = RE::BSScaleformManager::GetSingleton()->imageLoader.get();
		RE::BSScaleformExternalTexture extTex;
		extTex.SetTexture(m_renderTexture.get());
		imageLoader->RemoveTexture(extTex);
		m_renderTexture = nullptr;
	}
	
	m_renderTargetView = nullptr;
	ReleaseImport();
	m_actor = nullptr;
	m_depthStencilBuffer = nullptr;
	m_depthStencilState = nullptr;
	m_depthStencilView = nullptr;

	CDXEditableScene::Release();
}

void CDXNifScene::ReleaseImport()
{
	if (m_importRoot) {
		m_importRoot->DecRefCount();
	}

	m_importRoot = nullptr;
}

void CDXNifScene::Begin(CDXCamera * camera, CDXD3DDevice * device)
{
	BackupRenderState(device);

	auto deviceContext = device->GetDeviceContext();

	// Setup the viewport for rendering.
	REX::W32::D3D11_VIEWPORT viewport;
	viewport.width = (float)camera->GetWidth();
	viewport.height = (float)camera->GetHeight();
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewport.topLeftX = 0.0f;
	viewport.topLeftY = 0.0f;

	// Create the viewport.
	deviceContext->RSSetViewports(1, &viewport);

	REX::W32::ID3D11RenderTargetView* views[] = { m_renderTargetView.Get() };
	deviceContext->OMSetRenderTargets(1, views, m_depthStencilView.Get());

	// Set the depth stencil state.
	deviceContext->OMSetDepthStencilState(m_depthStencilState.Get(), 1);

	float color[4];
	// Setup the color to clear the buffer to.
	color[0] = g_sculptBackgroundR;
	color[1] = g_sculptBackgroundG;
	color[2] = g_sculptBackgroundB;
	color[3] = g_sculptBackgroundA;

	// Clear the back buffer.
	deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), color);

	// Clear the depth buffer.
	deviceContext->ClearDepthStencilView(m_depthStencilView.Get(), REX::W32::D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void CDXNifScene::End(CDXCamera * camera, CDXD3DDevice * device)
{
	RestoreRenderState(device);
}
