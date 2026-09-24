#include "CDXD3DDevice.h"
#include "CDXMesh.h"
#include "CDXPicker.h"
#include "CDXShader.h"
#include "SculptTrace.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

// Rendering shaders are outside this buffer/picking assay. Production mesh
// allocation, locks, raycast and upload code is linked unchanged.
void CDXShader::RenderShader(CDXD3DDevice*, const std::shared_ptr<CDXMaterial>&) {}
bool CDXShader::VSSetTransformBuffer(CDXD3DDevice*, TransformBuffer&) { return true; }
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
class Mesh : public CDXMesh
{
public:
    unsigned reads{}, writes{}, unlocks{};
    CDXMeshVert* LockVertices(LockMode mode = READ) override { mode == READ ? ++reads : ++writes; return CDXMesh::LockVertices(mode); }
    void UnlockVertices(LockMode mode) override { ++unlocks; CDXMesh::UnlockVertices(mode); }
};
int main()
{
    using namespace REX::W32;
    try {
        ComPtr<ID3D11Device> gpu;
        ComPtr<ID3D11DeviceContext> context;
        // D3D_DRIVER_TYPE_WARP=5, D3D11_SDK_VERSION=7; software device, no headset.
        Check(D3D11CreateDevice(nullptr, static_cast<D3D_DRIVER_TYPE>(5), nullptr, 0,
            nullptr, 0, 7, gpu.GetAddressOf(), nullptr, context.GetAddressOf()) >= 0, "WARP device creation failed");
        CDXD3DDevice device(gpu, context);
        Mesh mesh;
        Check(mesh.InitializeBuffers(&device, 3, 3, [](auto* v, auto* i) {
            v[0].Position = {-1,-1,0}; v[1].Position = {0,1,0}; v[2].Position = {1,-1,0};
            i[0]=0; i[1]=1; i[2]=2;
        }), "Production mesh initialization failed");
        auto* cpu = mesh.LockVertices();
        Check(cpu != nullptr, "CPU mesh copy was discarded"); mesh.UnlockVertices(CDXMesh::READ);
        SKEE::SculptTrace::Start();
        CDXRayInfo ray; ray.origin = DirectX::XMVectorSet(0,0,-2,0); ray.direction = DirectX::XMVectorSet(0,0,1,0);
        CDXPickInfo hit;
        for (unsigned i=0; i<1000; ++i) Check(mesh.Pick(ray, hit), "CPU hover ray missed triangle");
        Check(mesh.writes == 0 && mesh.reads == mesh.unlocks, "Hover wrote mesh or leaked a lock");
        Check(mesh.FlushVertices(), "Clean flush failed");
        Check(SKEE::SculptTrace::Read().samples[static_cast<unsigned>(SKEE::SculptTrace::Event::Upload)].calls == 0, "Hover caused GPU upload");
        // Nested reads during a write must see current CPU values, without mapping.
        auto* edited = mesh.LockVertices(CDXMesh::WRITE);
        edited[0].Position.z = edited[1].Position.z = edited[2].Position.z = 1;
        Check(mesh.LockVertices(CDXMesh::READ) == edited, "Nested read did not use authoritative CPU vertices");
        mesh.UnlockVertices(CDXMesh::READ); mesh.UnlockVertices(CDXMesh::WRITE);
        Check(mesh.Pick(ray, hit) && std::abs(hit.dist-3.F)<0.00001F, "Pick did not see edited mesh before upload");
        device.setDeviceContext({});
        Check(!mesh.FlushVertices(), "Unavailable upload should fail without losing dirty state");
        device.setDeviceContext(context);
        Check(mesh.FlushVertices() && mesh.FlushVertices(), "Upload/retry failed");
        const auto trace = SKEE::SculptTrace::Read(true);
        Check(trace.samples[static_cast<unsigned>(SKEE::SculptTrace::Event::Upload)].calls == 1, "Edits were not batched into one upload");
        D3D11_BUFFER_DESC desc{}; mesh.GetVertexBuffer()->GetDesc(&desc);
        desc.usage = D3D11_USAGE_STAGING; desc.bindFlags = 0; desc.cpuAccessFlags = D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Buffer> staging;
        Check(gpu->CreateBuffer(&desc, nullptr, staging.GetAddressOf()) >= 0, "Readback buffer failed");
        context->CopyResource(staging.Get(), mesh.GetVertexBuffer().Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped) >= 0, "GPU readback failed");
        Check(std::memcmp(mapped.data, cpu, sizeof(CDXMeshVert)*3)==0, "GPU upload differs from authoritative CPU mesh");
        context->Unmap(staging.Get(), 0);
        Mesh empty; Check(!empty.Pick(ray, hit) && empty.reads==empty.unlocks, "Failed pick leaked vertex lock");
        std::cout << "Sculpt: 1000 read-only hover picks, nested CPU edits, upload retry/batching and WARP GPU byte readback passed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
