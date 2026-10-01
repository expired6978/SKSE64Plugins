#pragma once

#ifdef CDX_MUTEX
#include <mutex>
#endif

#include <functional>
#include <memory>

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

	void SetTopology(REX::W32::D3D_PRIMITIVE_TOPOLOGY topo) { m_topology = topo; }

	virtual CDXMeshVert * LockVertices(const LockMode type = READ);
	virtual CDXMeshIndex * LockIndices();

	virtual void UnlockVertices(const LockMode type);
	virtual void UnlockIndices(bool write = false);

	REX::W32::ComPtr<REX::W32::ID3D11Buffer> GetVertexBuffer();
	REX::W32::ComPtr<REX::W32::ID3D11Buffer> GetIndexBuffer();

	const CDXMatrix & GetTransform() { return m_transform; };
	void SetTransform(const CDXMatrix & mat) { m_transform = mat; }

	std::uint32_t GetIndexCount();
	std::uint32_t GetFaceCount();
	std::uint32_t GetVertexCount();

protected:
	bool					m_visible;
	REX::W32::ComPtr<REX::W32::ID3D11Buffer>	m_vertexBuffer;
	std::uint32_t					m_vertCount;

	struct ColoredPrimitive
	{
		CDXVec	Position;
		CDXVec	Color;
	};
	union
	{
		std::unique_ptr<CDXMeshVert[]>		m_vertices{};
		std::unique_ptr<ColoredPrimitive[]> m_primitive;
	};
	REX::W32::ComPtr<REX::W32::ID3D11Buffer>	m_indexBuffer;
	std::uint32_t					m_indexCount;
	std::unique_ptr<CDXMeshIndex[]>	m_indices;
	REX::W32::D3D_PRIMITIVE_TOPOLOGY	m_topology;
	std::shared_ptr<CDXMaterial> m_material;
	CDXMatrix				m_transform;
	CDXD3DDevice		*	m_pDevice;

#ifdef CDX_MUTEX
	mutable std::mutex		m_mutex;
#endif
};

DirectX::XMVECTOR CalculateFaceNormal(std::uint32_t f, CDXMeshIndex * faces, CDXMeshVert * vertices);
bool IntersectSphere(float radius, float & dist, CDXVec & center, CDXVec & rayOrigin, CDXVec & rayDir);
bool IntersectTriangle(const CDXVec& orig, const CDXVec& dir, CDXVec& v0, CDXVec& v1, CDXVec& v2, float* t, float* u, float* v);
