#include "CDXNifMaterial.h"
#include <d3d11_3.h>

void CDXNifMaterial::SetNiTexture(int index, NiTexture* texture)
{
	m_pTextures[index] = texture;

	auto rendererData = m_pTextures[index]->rendererData;
	if (rendererData) {
		SetTexture(index, rendererData->resourceView);
	}
}