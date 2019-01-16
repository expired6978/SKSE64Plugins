#pragma once

#include "CDXMaterial.h"

#include "skse64/NiTypes.h"
#include "skse64/NiTextures.h"

class CDXNifMaterial : public CDXMaterial
{
public:
	// Holds a reference to the NiTexture to keep it from being destroyed
	void SetNiTexture(int index, NiTexture* texture);
	
private:
	NiPointer<NiTexture> m_pTextures[5];
};