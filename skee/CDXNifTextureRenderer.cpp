#include "CDXNifTextureRenderer.h"
#include "CDXD3DDevice.h"
#include "CDXTypes.h"
#include "CDXBSShaderResource.h"

#include "FileUtils.h"

#include "skse64/GameFormComponents.h"
#include "skse64/GameStreams.h"
#include "skse64/NiRenderer.h"
#include "skse64/NiTextures.h"


bool CDXNifTextureRenderer::Init(CDXD3DDevice* device, CDXPixelShaderCache * cache)
{
	CDXBSShaderResource sourceFile("SKSE/Plugins/NiOverride/Shaders/texture.fx", "TextureVertex");
	CDXBSShaderResource compiledFile("SKSE/Plugins/NiOverride/Shaders/Compiled/texture.cso");
	CDXShaderFactory factory;

	return Initialize(device, &factory, &sourceFile, &compiledFile, cache);
}

bool CDXNifTextureRenderer::ApplyMasksToTexture(CDXD3DDevice* device, NiPointer<NiTexture> texture, std::map<SInt32, MaskData> & masks, const BSFixedString & name, NiPointer<NiTexture> & output)
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
			AddLayer((texture && texture->rendererData) ? texture->rendererData->resourceView : nullptr, mask.second.textureType, mask.second.technique.c_str(), XMFLOAT4(r, g, b, a));
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

	output = CreateSourceTexture(name);
	NiTexture::RendererData * sourceData = new NiTexture::RendererData(GetWidth(), GetHeight());
	sourceData->texture = GetTexture().Get();
	sourceData->texture->AddRef();
	sourceData->resourceView = GetResourceView().Get();
	sourceData->resourceView->AddRef();
	output->rendererData = sourceData;

	return true;
}