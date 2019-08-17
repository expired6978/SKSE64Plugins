#include "CDXNifTextureRenderer.h"
#include "CDXD3DDevice.h"
#include "CDXTypes.h"

#include "FileUtils.h"

#include "skse64/GameFormComponents.h"
#include "skse64/GameStreams.h"
#include "skse64/NiRenderer.h"
#include "skse64/NiTextures.h"


bool CDXNifTextureRenderer::Init(CDXD3DDevice* device, CDXPixelShaderCache * cache)
{
	ShaderFileData shaderData;

	const char * shaderPath = "SKSE/Plugins/NiOverride/texture.fx";

	std::vector<char> vsb;
	BSResourceNiBinaryStream vs(shaderPath);
	if (!vs.IsValid()) {
		_ERROR("%s - Failed to read %s", __FUNCTION__, shaderPath);
		return false;
	}
	BSFileUtil::ReadAll(&vs, vsb);
	shaderData.pSourceName = "texture.fx";
	shaderData.pSrcData = &vsb.at(0);
	shaderData.SrcDataSize = vsb.size();

	return Initialize(device, shaderData, cache);
}

bool CDXNifTextureRenderer::ApplyMasksToTexture(CDXD3DDevice* device, NiPointer<NiTexture> texture, std::map<SInt32, MaskData> & masks, NiPointer<NiTexture> & output)
{
	auto rendererData = texture->rendererData;
	if (!rendererData)
	{
		_ERROR("%s - Texture has no rendererData", __FUNCTION__);
		return false;
	}

	bool activeLayers = false;
	for (const auto & mask : masks)
	{
		float a = ((mask.second.color >> 24) & 0xFF) / 255.0f;
		if (a > 0.0f)
		{
			activeLayers = true;
			break;
		}
	}

	// There are no active layers, use the source texture instead
	if (!activeLayers)
	{
		output = texture;
		return true;
	}

	if (!SetTexture(device, rendererData->resourceView, DXGI_FORMAT_R8G8B8A8_UNORM))
	{
		return false;
	}

	m_resources.clear();

	std::vector<NiPointer<NiTexture>> textures;
	for (const auto & mask : masks)
	{
		NiPointer<NiTexture> texture;
		if (mask.second.texture.length() > 0)
		{
			LoadTexture(mask.second.texture, 1, texture, false);
		}

		float a = ((mask.second.color >> 24) & 0xFF) / 255.0f;
		if (a > 0.0f)
		{
			float r = ((mask.second.color >> 16) & 0xFF) / 255.0f;
			float g = ((mask.second.color >> 8) & 0xFF) / 255.0f;
			float b = (mask.second.color & 0xFF) / 255.0f;
			AddLayer((texture && texture->rendererData) ? texture->rendererData->resourceView : nullptr, static_cast<TextureType>(mask.second.textureType), mask.second.technique.c_str(), XMFLOAT4(r, g, b, a));
		}

		if (texture) {
			textures.push_back(texture);
		}
	}

	// Will promote caching of textures for live-editing as this will hold onto them until cleanup
	m_textures = textures;

	EnterCriticalSection(&g_renderManager->lock);
	BackupRenderState(device);
	Render(device);
	RestoreRenderState(device);
	LeaveCriticalSection(&g_renderManager->lock);

	output = CreateSourceTexture("RuntimeTexture");
	NiTexture::RendererData * sourceData = new NiTexture::RendererData(m_bitmapWidth, m_bitmapHeight);
	sourceData->texture = m_renderTargetTexture.Get();
	sourceData->texture->AddRef();
	sourceData->resourceView = m_shaderResourceView.Get();
	sourceData->resourceView->AddRef();
	output->rendererData = sourceData;

	return true;
}