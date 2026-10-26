#include "CDXD3DDevice.h"
#include "CDXMesh.h"
#include "CDXEditableMesh.h"
#include "CDXMaterial.h"
#include "CDXPicker.h"
#include "CDXShader.h"
#include "CDXTriangleList.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <future>
#include <semaphore>
#include <thread>
#include <chrono>
#include <atomic>

// Rendering shaders are outside this buffer/picking assay. Production mesh
// allocation, locks, raycast and upload code is linked unchanged.
static unsigned solidPasses{}, wirePasses{};
CDXShader::CDXShader() {}
// Test-only material construction; shaders/textures are deliberately not
// qualified by this buffer assay. Only the initialized wireframe flag is read.
CDXMaterial::CDXMaterial() { SetWireframe(false); }
CDXMaterial::~CDXMaterial() {}
void CDXShader::RenderShader(CDXD3DDevice*, const std::shared_ptr<CDXMaterial>& material) {
    material && material->IsWireframe() ? ++wirePasses : ++solidPasses;
}
bool CDXShader::VSSetTransformBuffer(CDXD3DDevice*, TransformBuffer&) { return true; }
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
class Mesh : public CDXEditableMesh
{
public:
    std::atomic<unsigned> reads{}, writes{}, unlocks{};
    CDXMeshVert* LockVertices(LockMode mode = READ) override { mode == READ ? ++reads : ++writes; return CDXMesh::LockVertices(mode); }
    void UnlockVertices(LockMode mode) override { ++unlocks; CDXMesh::UnlockVertices(mode); }
    unsigned failBufferBind{};
    bool emptyBufferSuccess{};
    unsigned maps{};
    bool pauseMap{};
    bool nullMappedData{};
    std::binary_semaphore mapEntered{0}, resumeMap{0};
    bool TryStorage() { if (!m_dataMutex.try_lock()) return false; m_dataMutex.unlock(); return true; }
    bool Dirty() { std::lock_guard guard(m_dataMutex); return m_verticesDirty; }
    REX::W32::ComPtr<REX::W32::ID3D11Buffer> SwapVertexBuffer(REX::W32::ComPtr<REX::W32::ID3D11Buffer> next) {
        std::lock_guard guard(m_dataMutex); return std::exchange(m_vertexBuffer, std::move(next));
    }
protected:
    HRESULT CreateMeshBuffer(REX::W32::ID3D11Device* device,
        const REX::W32::D3D11_BUFFER_DESC& desc, const REX::W32::D3D11_SUBRESOURCE_DATA& data,
        REX::W32::ID3D11Buffer** result) override {
        if (desc.bindFlags == failBufferBind) return static_cast<HRESULT>(0x80004005L);
        if (emptyBufferSuccess) return 0;
        return CDXMesh::CreateMeshBuffer(device, desc, data, result);
    }
    HRESULT MapVertexBuffer(REX::W32::ID3D11DeviceContext* context,
        REX::W32::D3D11_MAPPED_SUBRESOURCE& resource) override {
        ++maps;
        if (pauseMap) { mapEntered.release(); resumeMap.acquire(); }
        const auto result = CDXMesh::MapVertexBuffer(context, resource);
        if (result >= 0 && nullMappedData) resource.data = nullptr;
        return result;
    }
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
        const auto fill = [](auto* v, auto* i) {
            v[0].Position = {-1,-1,0}; v[1].Position = {0,1,0}; v[2].Position = {1,-1,0};
            i[0]=0; i[1]=1; i[2]=2;
        };
        Mesh failed;
        Check(!failed.InitializeBuffers(nullptr,3,3,fill) && !failed.FlushVertices(), "Null-device mesh admitted");
        CDXD3DDevice missingContext(gpu, {});
        Check(!failed.InitializeBuffers(&missingContext,3,3,fill), "Null-context mesh admitted");
        for (auto bind : {D3D11_BIND_VERTEX_BUFFER,D3D11_BIND_INDEX_BUFFER}) {
            failed.failBufferBind = bind;
            Check(!failed.InitializeBuffers(&device,3,3,fill), "Injected GPU allocation failure accepted");
            Check(failed.GetVertexCount()==0 && !failed.GetVertexBuffer().Get() && !failed.FlushVertices(), "Partial initialization published");
        }
        struct UnwindProbe {};
        bool absentStorage = false;
        try { CDXMesh::VertexAccess access(failed,CDXMesh::WRITE); absentStorage = !access.Get(); throw UnwindProbe{}; }
        catch (const UnwindProbe&) {}
        Check(absentStorage,"Failed mesh has storage");
        Check(std::async(std::launch::async,[&] { return failed.TryStorage(); }).get(), "Null/throw access leaked mesh lock");
        Mesh mesh;
        Check(mesh.InitializeBuffers(&device, 3, 3, [](auto* v, auto* i) {
            v[0].Position = {-1,-1,0}; v[1].Position = {0,1,0}; v[2].Position = {1,-1,0};
            i[0]=0; i[1]=1; i[2]=2;
        }), "Production mesh initialization failed");
        const auto originalBuffer = mesh.GetVertexBuffer();
        for (auto bind : {D3D11_BIND_VERTEX_BUFFER,D3D11_BIND_INDEX_BUFFER}) {
            mesh.failBufferBind = bind;
            Check(!mesh.InitializeBuffers(&device,3,3,fill), "Failed reinitialization accepted");
            Check(mesh.GetVertexBuffer().Get()==originalBuffer.Get() && mesh.GetVertexCount()==3, "Failed replacement destroyed old mesh");
        }
        mesh.failBufferBind = 0;
        Check(!mesh.InitializeBuffers(&device,3,4,fill),"Incomplete triangle-list initialization accepted");
        Check(!mesh.InitializeBuffers(&device,65536,3,fill),"Unrepresentable vertex count accepted");
        Check(!mesh.InitializeBuffers(&device,3,3,[](auto*,auto*) { throw UnwindProbe{}; }),"Non-standard fill exception escaped admission");
        Check(!mesh.InitializeBuffers(&device,3,3,[&](auto* v,auto* i) { fill(v,i); i[2]=3; }),"Out-of-range index accepted");
        Check(mesh.GetVertexBuffer().Get()==originalBuffer.Get(),"Rejected fill changed published mesh");
        mesh.emptyBufferSuccess=true;
        Check(!mesh.InitializeBuffers(&device,3,3,fill),"Empty GPU success admitted");
        mesh.emptyBufferSuccess=false;
        std::vector<CDXMeshIndex> triangles{9};
        const std::uint16_t lengths[]{4,3};
        const CDXMeshIndex strips[]{0,1,2,3,4,5,6};
        Check(ExpandTriangleStrips(lengths,strips,3,triangles) && triangles==std::vector<CDXMeshIndex>({0,1,2,2,1,3,4,5,6}),"Strip expansion lost winding or joined strips");
        Check(!ExpandTriangleStrips(lengths,strips,4,triangles) && triangles.size()==9,"Strip count failure partially published");
        const std::uint16_t shortStrip[]{2};
        Check(!ExpandTriangleStrips(shortStrip,{strips,2},0,triangles),"Short strip accepted");
        auto* cpu = mesh.LockVertices();
        Check(cpu != nullptr, "CPU mesh copy was discarded"); mesh.UnlockVertices(CDXMesh::READ);
        CDXRayInfo ray; ray.origin = DirectX::XMVectorSet(0,0,-2,0); ray.direction = DirectX::XMVectorSet(0,0,1,0);
        CDXPickInfo hit;
        for (unsigned i=0; i<1000; ++i) Check(mesh.Pick(ray, hit), "CPU hover ray missed triangle");
        Check(mesh.writes.load() == 0 && mesh.reads.load() == mesh.unlocks.load(), "Hover wrote mesh or leaked a lock");
        Check(mesh.FlushVertices(), "Clean flush failed");
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
        D3D11_BUFFER_DESC desc{}; mesh.GetVertexBuffer()->GetDesc(&desc);
        desc.usage = D3D11_USAGE_STAGING; desc.bindFlags = 0; desc.cpuAccessFlags = D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Buffer> staging;
        Check(gpu->CreateBuffer(&desc, nullptr, staging.GetAddressOf()) >= 0, "Readback buffer failed");
        // Actual D3D Map failure: READ-only staging storage cannot be mapped
        // WRITE_DISCARD. Restore the real buffer and prove dirty retry survived.
        { CDXMesh::VertexAccess edit(mesh,CDXMesh::WRITE); edit.Get()[0].Position.z=2; }
        auto realBuffer = mesh.SwapVertexBuffer(staging);
        Check(!mesh.FlushVertices() && mesh.Dirty(), "Actual Map failure lost dirty edits");
        mesh.SwapVertexBuffer(realBuffer);
        Check(mesh.FlushVertices(), "Map failure did not recover");
        { CDXMesh::VertexAccess edit(mesh,CDXMesh::WRITE); edit.Get()[0].Position.z=2.25F; }
        mesh.nullMappedData=true;
        Check(!mesh.FlushVertices() && mesh.Dirty(),"Null successful map discarded edits");
        mesh.nullMappedData=false;
        Check(mesh.FlushVertices(),"Null-map recovery failed");
        // Exercise production editable-mesh double rendering. Shader binding is
        // stubbed, but both base Render transactions and D3D uploads are real.
        auto material = std::make_shared<CDXMaterial>();
        mesh.SetMaterial(material);
        CDXShader shader;
        { CDXMesh::VertexAccess edit(mesh,CDXMesh::WRITE); edit.Get()[0].Position.z=2.5F; }
        const auto beforeMaps = mesh.maps;
        mesh.Render(&device,&shader);
        Check(mesh.maps==beforeMaps+1 && solidPasses==1 && wirePasses==1 && !material->IsWireframe(),"Wireframe double render did not batch its upload");
        mesh.BuildAdjacency(); mesh.BuildFacemap(); mesh.BuildNormals();
        Check(mesh.Dirty(),"Derived normal construction did not publish edits");
        Check(mesh.FlushVertices(),"Normal upload failed");

        // Pause production flush inside its locked Map boundary. A second
        // writer cannot enter until the older upload commits and releases.
        { CDXMesh::VertexAccess edit(mesh,CDXMesh::WRITE); edit.Get()[0].Position.z=3; }
        mesh.pauseMap=true;
        auto flush = std::async(std::launch::async,[&] { return mesh.FlushVertices(); });
        const bool reachedMap = mesh.mapEntered.try_acquire_for(std::chrono::seconds(5));
        if (!reachedMap) mesh.resumeMap.release();
        Check(reachedMap,"Flush did not reach bounded barrier");
        const bool blocked = !mesh.TryStorage();
        std::promise<void> writerStarted;
        auto writer = std::async(std::launch::async,[&] {
            writerStarted.set_value();
            CDXMesh::VertexAccess edit(mesh,CDXMesh::WRITE); edit.Get()[0].Position.z=4;
        });
        writerStarted.get_future().get();
        mesh.resumeMap.release();
        const bool uploaded = flush.get(); writer.get(); mesh.pauseMap=false;
        Check(blocked && uploaded && mesh.Dirty(),"Older upload cleared newer CPU edit");
        Check(mesh.FlushVertices() && !mesh.Dirty(),"New generation did not upload");
        context->CopyResource(staging.Get(), mesh.GetVertexBuffer().Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped) >= 0, "GPU readback failed");
        const bool sameBytes = mapped.data && std::memcmp(mapped.data, cpu, sizeof(CDXMeshVert)*3)==0;
        context->Unmap(staging.Get(), 0);
        Check(sameBytes, "GPU upload differs from authoritative CPU mesh");
        Mesh empty; Check(!empty.Pick(ray, hit) && empty.reads.load()==empty.unlocks.load(), "Failed pick leaked vertex lock");
        std::cout << "Sculpt: transactional failures, strip winding, 1000 read-only picks, derived normals, double rendering, upload retry, concurrent edits and WARP byte readback passed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
