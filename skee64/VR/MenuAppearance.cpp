#include "pch.h"
#include "MenuAppearance.h"
#include "MenuAppearancePolicy.h"
#include "MenuAppearanceImage.h"
#include "MenuBackgroundProjection.h"
#include <DirectXPackedVector.h>
#include <cstring>
#include "ScaleformUtils.h"
#include "SKEEHooks.h"
#include "Utilities.h"
#include <DirectXTex.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <objbase.h>
#include <mutex>
#include <atomic>
#include <memory>

namespace SKEE::MenuAppearance
{
	namespace
	{
		int backgroundColor = -1;
		int textColor = -1;
		std::string imageURL;
		DirectX::ScratchImage imagePixels;
		std::uint32_t imageWidth{}, imageHeight{};
		RE::NiPointer<RE::NiSourceTexture> residentTexture;
		std::unique_ptr<RE::BSScaleformExternalTexture> residentExternal;
		std::mutex imageMutex;
		bool mountFailed{};
		// UI callbacks only read this state or enqueue work. They must never
		// acquire the renderer lock while Scaleform owns its movie lock.
		std::atomic<int> mountState{ 0 }; // 0 idle, 1 queued, 2 ready, 3 failed
		std::atomic<double> projectionScale{ 0 }; // 0 pending, -1 unavailable
		constexpr char kTextureName[] = "RaceMenuNGBackground";

		void MeasureProjection(RE::GFxMovie* identity)
		{
			// Game-thread only. No GPU readback, renderer lock or live debugger.
			auto* ui = RE::UI::GetSingleton();
			auto menu = ui ? ui->GetMenu<RE::RaceSexMenu>() : RE::GPtr<RE::RaceSexMenu>{};
			if (!menu || menu->uiMovie.get() != identity) return;
			projectionScale.store(-1);
			if (!REL::Module::IsVR()) { projectionScale.store(1); return; }
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* nodes = player ? player->GetVRNodeData() : nullptr;
			auto* quad = nodes ? nodes->InWorldUIQuadGeo.get() : nullptr;
			auto* data = quad ? quad->GetGeometryRuntimeData().rendererData : nullptr;
			if (!data || !data->rawVertexData || quad->GetTrishapeRuntimeData().vertexCount != 4) {
				SKSE::log::warn("RaceMenu background projection unavailable: no four-vertex CPU UI surface");
				return;
			}
			auto desc = data->vertexDesc;
			const auto stride = desc.GetSize();
			const auto pos = desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_POSITION);
			const auto uv = desc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0);
			if (!desc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX) || !desc.HasFlag(RE::BSGraphics::Vertex::VF_UV) ||
				stride > 128 || pos + 12 > stride || uv + 4 > stride) return;
			std::array<SurfaceVertex, 4> vertices{};
			for (std::size_t i = 0; i < vertices.size(); ++i) {
				float xyz[3]; std::uint16_t tex[2];
				std::memcpy(xyz, data->rawVertexData + i*stride + pos, sizeof(xyz));
				std::memcpy(tex, data->rawVertexData + i*stride + uv, sizeof(tex));
				vertices[i] = {xyz[0], xyz[1], xyz[2], DirectX::PackedVector::XMConvertHalfToFloat(tex[0]), DirectX::PackedVector::XMConvertHalfToFloat(tex[1])};
			}
			const auto surface = SurfaceAspect(vertices);
			const auto frame = menu->uiMovie->GetVisibleFrameRect();
			const auto width = frame.right - frame.left, height = frame.bottom - frame.top;
			const auto scale = surface * height / width;
			if (!(width > 0 && height > 0 && surface > 0 && scale >= 0.05 && scale <= 20 && std::isfinite(scale))) return;
			// Ni scene transforms have uniform scale, so yaw/size/translation do not
			// change this metric. Visible frame accounts for Scaleform viewport fit.
			projectionScale.store(scale);
			SKSE::log::info("RaceMenu background projection measured: surface U/V={:.6f}, visible frame={}x{}, X/Y={:.6f}", surface, width, height, scale);
		}

		struct COMScope
		{
			HRESULT status = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			~COMScope() { if (SUCCEEDED(status)) CoUninitialize(); }
		};

		void PrepareImage(const std::filesystem::path& runtimeDirectory)
		{
			// Decode once. img:// is an engine image lookup, not a generic DDS reader.
			// Upload/register explicitly when the menu requests the image, after D3D
			// initialization. Never publish or reuse a filesystem-derived cache.
			imageURL.clear();
			imagePixels.Release();
			imageWidth = imageHeight = 0;
			if (runtimeDirectory.empty()) return;
			const auto source = runtimeDirectory / L"Data/Interface/RaceMenu/racemenu.png";
			if (!std::filesystem::exists(source)) return;
			const auto size = std::filesystem::file_size(source);
			if (size < 33 || size > 16 * 1024 * 1024) {
				SKSE::log::warn("RaceMenu background PNG exceeds the supported file bounds");
				return;
			}
			std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
			std::ifstream input(source, std::ios::binary);
			if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size()) || !BoundedPNG(bytes.data(), bytes.size())) {
				SKSE::log::warn("RaceMenu background PNG has an invalid header/dimensions (maximum 4096x4096)");
				return;
			}
			COMScope com;
			if (FAILED(com.status) && com.status != RPC_E_CHANGED_MODE) return;
			auto result = DecodeBackgroundPNG(bytes.data(), bytes.size(), imagePixels);
			if (FAILED(result)) {
				SKSE::log::warn("RaceMenu background PNG decoding failed: {:08X}", static_cast<unsigned>(result));
				return;
			}
			const auto& metadata = imagePixels.GetMetadata();
			imageWidth = static_cast<std::uint32_t>(metadata.width);
			imageHeight = static_cast<std::uint32_t>(metadata.height);
			imageURL = "img://RaceMenuNGBackground";
			SKSE::log::info("RaceMenu background PNG prepared: {}x{}", metadata.width, metadata.height);
		}

		bool MountImage()
		{
			std::scoped_lock lock(imageMutex);
			if (residentTexture) return true;
			if (mountFailed || imageURL.empty()) return false;
			auto* manager = RE::BSScaleformManager::GetSingleton();
			auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
			if (!manager || !manager->imageLoader || !renderer || !renderer->GetRuntimeData().forwarder) return false;
			// Same upload/registration and renderer-data ownership as CDXNifScene.
			utils::ScopedCriticalSection rendererLock(&renderer->GetRendererData().lock);
			auto* device = renderer->GetRuntimeData().forwarder;
			const auto* pixels = imagePixels.GetImage(0, 0, 0);
			if (!pixels) return false;
			REX::W32::D3D11_TEXTURE2D_DESC desc{};
			desc.width = imageWidth;
			desc.height = imageHeight;
			desc.mipLevels = desc.arraySize = desc.sampleDesc.count = 1;
			desc.format = REX::W32::DXGI_FORMAT_B8G8R8A8_UNORM;
			desc.usage = REX::W32::D3D11_USAGE_IMMUTABLE;
			desc.bindFlags = REX::W32::D3D11_BIND_SHADER_RESOURCE;
			REX::W32::D3D11_SUBRESOURCE_DATA data{};
			data.sysMem = pixels->pixels;
			data.sysMemPitch = static_cast<std::uint32_t>(pixels->rowPitch);
			data.sysMemSlicePitch = static_cast<std::uint32_t>(pixels->slicePitch);
			REX::W32::ComPtr<REX::W32::ID3D11Texture2D> gpu;
			REX::W32::ComPtr<REX::W32::ID3D11ShaderResourceView> resourceView;
			auto result = device->CreateTexture2D(&desc, &data, gpu.ReleaseAndGetAddressOf());
			if (SUCCEEDED(result)) result = device->CreateShaderResourceView(gpu.Get(), nullptr, resourceView.ReleaseAndGetAddressOf());
			if (FAILED(result)) {
				mountFailed = true;
				imagePixels.Release();
				SKSE::log::warn("RaceMenu background GPU upload failed: {:08X}", static_cast<unsigned>(result));
				return false;
			}
			RE::NiPointer<RE::NiSourceTexture> texture{ SKEE::CreateSourceTexture(kTextureName) };
			if (!texture) { mountFailed = true; imagePixels.Release(); return false; }
			auto* rendererData = new RE::NiTexture::RendererData(imageWidth, imageHeight);
			rendererData->texture = gpu.Detach();
			rendererData->resourceView = resourceView.Detach();
			texture->rendererTexture = reinterpret_cast<RE::BSGraphics::Texture*>(rendererData);
			// VR SetTexture calls AddTexture internally. Its return indicates an
			// existing registration, not successful creation: a new entry returns
			// false. Keep the external owner alive, since ReleaseTexture removes
			// its registration. A second explicit AddTexture would add a reference.
			auto external = std::make_unique<RE::BSScaleformExternalTexture>();
			external->SetTexture(texture.get());
			if (external->gamebryoTexture.get() != texture.get()) {
				mountFailed = true;
				imagePixels.Release();
				SKSE::log::warn("RaceMenu background Scaleform registration failed");
				return false;
			}
			// One shared immutable texture for the process, not another AddTexture
			// reference on each menu reopen. Movie clips are unloaded separately.
			residentTexture = texture;
			residentExternal = std::move(external);
			imagePixels.Release();
			SKSE::log::info("RaceMenu background registered: {} ({}x{})", kTextureName, imageWidth, imageHeight);
			return true;
		}

		class MountBackgroundFunction final : public RE::GFxFunctionHandler
		{
			void Call(Params& args) override
			{
				projectionScale.store(0);
				const auto* projectionTasks = SKSE::GetTaskInterface();
				if (projectionTasks) projectionTasks->AddTask([identity = args.movie] { MeasureProjection(identity); });
				else projectionScale.store(-1);
				int expected = 0;
				if (mountState.compare_exchange_strong(expected, 1)) {
					const auto* tasks = SKSE::GetTaskInterface();
					if (!tasks || imageURL.empty()) mountState.store(3);
					else tasks->AddTask([] {
						try {
							SKSE::log::info("RaceMenu background registration task started on game thread");
							mountState.store(MountImage() ? 2 : 3);
						} catch (const std::exception& error) {
							SKSE::log::warn("RaceMenu background registration failed: {}", error.what());
							mountState.store(3);
						}
					});
				}
				if (args.retVal) args.retVal->SetBoolean(mountState.load() == 2);
			}
		};
		class BackgroundStateFunction final : public RE::GFxFunctionHandler
		{
			void Call(Params& args) override
			{
				if (args.retVal) args.retVal->SetNumber(mountState.load());
			}
		};
		class BackgroundProjectionFunction final : public RE::GFxFunctionHandler
		{
			void Call(Params& args) override
			{
				if (args.retVal) args.retVal->SetNumber(projectionScale.load());
			}
		};
	}

	void Configure(const std::string& background, const std::string& text, const std::string& runtimeDirectory)
	{
		backgroundColor = ParseColor(background);
		textColor = ParseColor(text);
		if (!background.empty() && backgroundColor < 0) SKSE::log::warn("Invalid Menu Appearance sBackgroundColor; retaining default");
		if (!text.empty() && textColor < 0) SKSE::log::warn("Invalid Menu Appearance sTextColor; retaining default");
		try { PrepareImage(runtimeDirectory); }
		catch (const std::exception& error) {
			imageURL.clear();
			SKSE::log::warn("RaceMenu background preparation failed; retaining colour background: {}", error.what());
		}
	}

	void Register(RE::GFxMovie* view, RE::GFxValue* root)
	{
		ScaleformUtils::RegisterNumber(root, "menuBackgroundColor", backgroundColor);
		ScaleformUtils::RegisterNumber(root, "menuTextColor", textColor);
		ScaleformUtils::RegisterString(root, view, "menuBackgroundImage", imageURL.c_str());
		ScaleformUtils::RegisterNumber(root, "menuBackgroundImageWidth", imageWidth);
		ScaleformUtils::RegisterNumber(root, "menuBackgroundImageHeight", imageHeight);
		// VR BSScaleformImageLoader::LoadImage constructs registered images with
		// a fixed 64x64 GImageInfo wrapper (RVA FA1ED0). These are clip bounds,
		// not GPU dimensions; retain the decoded dimensions for aspect fitting.
		ScaleformUtils::RegisterNumber(root, "menuBackgroundClipWidth", REL::Module::IsVR() ? 64 : imageWidth);
		ScaleformUtils::RegisterNumber(root, "menuBackgroundClipHeight", REL::Module::IsVR() ? 64 : imageHeight);
		static RE::GPtr<MountBackgroundFunction> handler{ new MountBackgroundFunction{} };
		RE::GFxValue function;
		view->CreateFunction(&function, handler.get());
		root->SetMember("MountMenuBackgroundImage", function);
		static RE::GPtr<BackgroundStateFunction> stateHandler{ new BackgroundStateFunction{} };
		view->CreateFunction(&function, stateHandler.get());
		root->SetMember("GetMenuBackgroundImageState", function);
		static RE::GPtr<BackgroundProjectionFunction> projectionHandler{ new BackgroundProjectionFunction{} };
		view->CreateFunction(&function, projectionHandler.get());
		root->SetMember("GetMenuBackgroundProjectionScale", function);
	}
}
