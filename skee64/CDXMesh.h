#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <utility>

#include "CDXTypes.h"
#include <cstdint>



class CDXD3DDevice;
class CDXMaterial;
class CDXPicker;
class CDXShader;
class CDXEditableMesh;
class CDXRayInfo;
class CDXPickInfo;

class CDXMesh
{
public:
	CDXMesh();
	virtual ~CDXMesh();

	bool InitializeBuffers(CDXD3DDevice * pDevice, std::uint32_t vertexCount, std::uint32_t indexCount, std::function<void(CDXMeshVert*, CDXMeshIndex*)> fillFunction);

	virtual const char* GetName() const { return ""; }
	virtual void Render(CDXD3DDevice * pDevice, CDXShader * shader);
	virtual bool Pick(CDXRayInfo & rayInfo, CDXPickInfo & pickInfo);
	virtual bool IsEditable() const { return false; }
	virtual bool IsLocked() const { return true; }
	virtual bool IsMorphable() const { return false; }
	virtual bool ShowWireframe() const { return false; }

	void SetVisible(bool visible);
	virtual bool IsVisible() const;

	void SetMaterial(const std::shared_ptr<CDXMaterial>& material);
	std::shared_ptr<CDXMaterial> GetMaterial();

	enum LockMode
	{
		READ = REX::W32::D3D11_MAP_READ,
		WRITE = REX::W32::D3D11_MAP_WRITE_NO_OVERWRITE
	};

	void SetTopology(REX::W32::D3D_PRIMITIVE_TOPOLOGY topo) { std::lock_guard guard(m_dataMutex); m_topology = topo; }

	virtual CDXMeshVert * LockVertices(const LockMode type = READ);
	virtual CDXMeshIndex * LockIndices();

	virtual void UnlockVertices(const LockMode type);
	virtual void UnlockIndices(bool write = false);
	// Null results also own a lock: guards balance all exits, including throws.
	class VertexAccess
	{
	public:
		VertexAccess(CDXMesh& mesh, LockMode mode = READ) : m_mesh(&mesh), m_mode(mode), m_data(mesh.LockVertices(mode)) {}
		~VertexAccess() { Release(); }
		VertexAccess(const VertexAccess&) = delete;
		VertexAccess& operator=(const VertexAccess&) = delete;
		CDXMeshVert* Get() const { return m_data; }
		void Release() { if (auto* mesh = std::exchange(m_mesh, nullptr)) mesh->UnlockVertices(m_mode); }
	private:
		CDXMesh* m_mesh;
		LockMode m_mode;
		CDXMeshVert* m_data;
	};
	class IndexAccess
	{
	public:
		explicit IndexAccess(CDXMesh& mesh) : m_mesh(&mesh), m_data(mesh.LockIndices()) {}
		~IndexAccess() { Release(); }
		IndexAccess(const IndexAccess&) = delete;
		IndexAccess& operator=(const IndexAccess&) = delete;
		CDXMeshIndex* Get() const { return m_data; }
		void Release() { if (auto* mesh = std::exchange(m_mesh, nullptr)) mesh->UnlockIndices(); }
	private:
		CDXMesh* m_mesh;
		CDXMeshIndex* m_data;
	};
	// Upload edited CPU vertices once before drawing, never while picking.
	bool FlushVertices();

	REX::W32::ComPtr<REX::W32::ID3D11Buffer> GetVertexBuffer();
	REX::W32::ComPtr<REX::W32::ID3D11Buffer> GetIndexBuffer();

	CDXMatrix GetTransform() { std::lock_guard guard(m_dataMutex); return m_transform; }
	void SetTransform(const CDXMatrix & mat) { std::lock_guard guard(m_dataMutex); m_transform = mat; }

	std::uint32_t GetIndexCount();
	std::uint32_t GetFaceCount();
	std::uint32_t GetVertexCount();

protected:
	// Narrow GPU boundary permits deterministic failure/order tests without
	// replacing CPU ownership, locking, initialization or flush implementation.
	virtual HRESULT CreateMeshBuffer(REX::W32::ID3D11Device* device,
		const REX::W32::D3D11_BUFFER_DESC& desc, const REX::W32::D3D11_SUBRESOURCE_DATA& data,
		REX::W32::ID3D11Buffer** result);
	virtual HRESULT MapVertexBuffer(REX::W32::ID3D11DeviceContext* context,
		REX::W32::D3D11_MAPPED_SUBRESOURCE& resource);
	// CPU storage, dirty publication and upload share this authority regardless
	// of CDX_MUTEX. Recursion permits nested normal reads and Render -> Flush.
	mutable std::recursive_mutex m_dataMutex;
	bool					m_visible;
	REX::W32::ComPtr<REX::W32::ID3D11Buffer>	m_vertexBuffer;
	std::uint32_t					m_vertCount;

	struct ColoredPrimitive
	{
		CDXVec	Position;
		CDXVec	Color;
	};
	std::unique_ptr<CDXMeshVert[]> m_vertices;
	std::unique_ptr<ColoredPrimitive[]> m_primitive;
	bool m_verticesDirty{false};
	REX::W32::ComPtr<REX::W32::ID3D11Buffer>	m_indexBuffer;
	std::uint32_t					m_indexCount;
	std::unique_ptr<CDXMeshIndex[]>	m_indices;
	REX::W32::D3D_PRIMITIVE_TOPOLOGY	m_topology;
	std::shared_ptr<CDXMaterial> m_material;
	CDXMatrix				m_transform;
	CDXD3DDevice		*	m_pDevice{};

};

DirectX::XMVECTOR CalculateFaceNormal(std::uint32_t f, CDXMeshIndex * faces, CDXMeshVert * vertices);
bool IntersectSphere(float radius, float & dist, CDXVec & center, CDXVec & rayOrigin, CDXVec & rayDir);
bool IntersectTriangle(const CDXVec& orig, const CDXVec& dir, CDXVec& v0, CDXVec& v1, CDXVec& v2, float* t, float* u, float* v);
