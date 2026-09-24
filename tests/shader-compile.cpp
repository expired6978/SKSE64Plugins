#include "CDXShaderCompile.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    void CompileFile(const std::filesystem::path& path, const char* entry, const char* profile)
    {
        std::ifstream input(path, std::ios::binary);
        Check(static_cast<bool>(input), "Shader input unavailable");
        const std::string source((std::istreambuf_iterator<char>(input)), {});
        REX::W32::ComPtr<REX::W32::ID3DBlob> bytecode, errors;
        const auto name = path.string();
        const auto result = CompileShaderFromData(source.data(), source.size(), name.c_str(),
            entry, profile, bytecode.GetAddressOf(), errors.GetAddressOf());
        if (FAILED(result) && errors.Get())
            std::cerr.write(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        Check(SUCCEEDED(result), "Production shader compiler failed");
        Check(bytecode.Get() && bytecode->GetBufferSize() > 0, "No compiled shader bytecode");
    }
}

int main(int argc, char** argv)
{
    try {
        Check(argc == 2, "Expected shader source root");
        const std::filesystem::path root(argv[1]);
        struct Shader { const char* path; const char* entry; const char* profile; };
        constexpr Shader shaders[]{
            {"CharGen/brush_vs.hlsl", "BrushVShader", "vs_5_0"},
            {"CharGen/brush_ps.hlsl", "BrushPShader", "ps_5_0"},
            {"CharGen/shader_vs.hlsl", "LightVertexShader", "vs_5_0"},
            {"CharGen/shader_ps.hlsl", "LightPixelShader", "ps_5_0"},
            {"CharGen/wireframe_vs.hlsl", "WireframeVertexShader", "vs_5_0"},
            {"CharGen/wireframe_ps.hlsl", "WireframePixelShader", "ps_5_0"},
            {"CharGen/wireframe_gs.hlsl", "WireframeGeometryShader", "gs_5_0"},
            {"NiOverride/texture.fx", "TextureVertex", "vs_5_0"}
        };
        std::size_t count = 0;
        for (const auto& shader : shaders) {
            CompileFile(root/shader.path, shader.entry, shader.profile);
            ++count;
        }
        for (const auto& file : std::filesystem::directory_iterator(root/"NiOverride/Effects")) {
            if (file.path().extension() != ".fx") continue;
            auto entry = file.path().stem().string();
            for (auto& ch : entry) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            CompileFile(file.path(), entry.c_str(), "ps_5_0");
            ++count;
        }
        Check(count > std::size(shaders), "No tint effect shaders tested");
        REX::W32::ComPtr<REX::W32::ID3DBlob> bytecode, errors;
        Check(CompileShaderFromData(nullptr, 0, nullptr, "main", "ps_5_0",
            bytecode.GetAddressOf(), errors.GetAddressOf()) == E_INVALIDARG, "Invalid input accepted");
        constexpr char invalid[] = "this is not HLSL";
        Check(FAILED(CompileShaderFromData(invalid, sizeof(invalid)-1, "invalid.hlsl", "main", "ps_5_0",
            bytecode.GetAddressOf(), errors.GetAddressOf())), "Malformed HLSL accepted");
        Check(!bytecode.Get() && errors.Get() && errors->GetBufferSize() > 0,
            "Malformed HLSL did not return compiler diagnostics");
        std::cout << "Production D3DCompile: " << count
            << " shader sources compiled; invalid input and malformed HLSL checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
