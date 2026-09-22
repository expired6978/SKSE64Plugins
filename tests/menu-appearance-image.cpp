#include <windows.h>
#include <wincodec.h>
#include "../skee64/MenuAppearanceImage.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
#include <iostream>

int wmain(int argc, wchar_t** argv)
{
	assert(argc == 2);
	assert(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)));
	{
		const std::filesystem::path directory(argv[1]);
		DirectX::ScratchImage original;
		assert(SUCCEEDED(original.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 3, 2, 1, 1)));
		const std::uint8_t pixel[] = {47, 83, 127, 64};
		const auto* image = original.GetImage(0,0,0);
		for (std::size_t y=0;y<2;++y) for(std::size_t x=0;x<3;++x) std::memcpy(image->pixels+y*image->rowPitch+x*4,pixel,4);
		auto source = directory/L"test-background.png";
		assert(SUCCEEDED(DirectX::SaveToWICFile(*image, DirectX::WIC_FLAGS_NONE, GUID_ContainerFormatPng, source.c_str())));
		std::ifstream input(source,std::ios::binary);
		std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
		DirectX::ScratchImage converted;
		assert(SUCCEEDED(SKEE::MenuAppearance::DecodeBackgroundPNG(bytes.data(),bytes.size(),converted)));
		assert(converted.GetMetadata().width == 3 && converted.GetMetadata().height == 2);
		assert(converted.GetMetadata().format == DXGI_FORMAT_B8G8R8A8_UNORM);
		auto texture = directory/L"test-background.dds";
		assert(SUCCEEDED(DirectX::SaveToDDSFile(converted.GetImages(),converted.GetImageCount(),converted.GetMetadata(),DirectX::DDS_FLAGS_NONE,texture.c_str())));
		DirectX::ScratchImage loaded;
		assert(SUCCEEDED(DirectX::LoadFromDDSFile(texture.c_str(),DirectX::DDS_FLAGS_NONE,nullptr,loaded)));
		assert(loaded.GetMetadata().width == 3 && loaded.GetMetadata().height == 2);
		const auto* decoded = loaded.GetImage(0,0,0);
		assert(decoded->pixels[0]==127 && decoded->pixels[1]==83 && decoded->pixels[2]==47 && decoded->pixels[3]==64);
		// A valid-looking header followed by truncated data must not publish a texture.
		DirectX::ScratchImage rejected;
		assert(FAILED(SKEE::MenuAppearance::DecodeBackgroundPNG(bytes.data(),33,rejected)));
		bytes[18]=32;
		assert(FAILED(SKEE::MenuAppearance::DecodeBackgroundPNG(bytes.data(),bytes.size(),rejected)));
		std::cout << "Production PNG decoder: dimensions, RGB, straight alpha, legacy DDS round trip, truncated/oversized rejection passed\n";
	}
	CoUninitialize();
}
