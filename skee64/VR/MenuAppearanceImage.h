#pragma once
#include "MenuAppearancePolicy.h"
#include <DirectXTex.h>

namespace SKEE::MenuAppearance
{
	// The caller owns COM initialization. Shared by production and offline tests.
	inline HRESULT DecodeBackgroundPNG(const std::uint8_t* bytes, std::size_t size, DirectX::ScratchImage& converted)
	{
		if (!BoundedPNG(bytes, size)) return E_INVALIDARG;
		DirectX::ScratchImage decoded;
		auto result = DirectX::LoadFromWICMemory(bytes, size, DirectX::WIC_FLAGS_FORCE_RGB, nullptr, decoded);
		if (FAILED(result)) return result;
		const auto& metadata = decoded.GetMetadata();
		if (metadata.width > 4096 || metadata.height > 4096 || metadata.arraySize != 1 || metadata.depth != 1) return E_INVALIDARG;
		return DirectX::Convert(decoded.GetImages(), decoded.GetImageCount(), metadata,
			DXGI_FORMAT_B8G8R8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, converted);
	}
}
