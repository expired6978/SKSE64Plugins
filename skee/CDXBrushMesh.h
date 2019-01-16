#pragma once

#include "CDXMesh.h"
#include "CDXPicker.h"

class CDXBrush;
struct ShaderFileData;

class CDXBrushMesh : public CDXMesh
{
public:
	CDXBrushMesh();

	bool Create(CDXD3DDevice * pDevice, bool dashed, DirectX::XMVECTOR ringColor, DirectX::XMVECTOR dotColor, const ShaderFileData & vertexShaderData, const ShaderFileData & pixelShaderData);

	virtual void Render(CDXD3DDevice * pDevice, CDXShader * shader) override;
	virtual void Release() override;
	virtual bool Pick(CDXRayInfo & rayInfo, CDXPickInfo & pickInfo) override;

	const CDXMatrix & GetSphereTransform() { return m_sphere.m_transform; };
	void SetSphereTransform(const CDXMatrix & mat) { m_sphere.m_transform = mat; }

protected:
	struct Sphere
	{
		std::vector<CDXMesh::ColoredPrimitive> m_vertices;
		ID3D11Buffer* m_vertexBuffer;
		std::vector<CDXMeshIndex> m_indices;
		ID3D11Buffer* m_indexBuffer;
		CDXMatrix m_transform;
	};

	void ComputeSphere(std::vector<CDXMesh::ColoredPrimitive>& vertices, std::vector<CDXMeshIndex> & indices, float radius, int sliceCount, int stackCount, DirectX::XMVECTOR color);

private:
	ID3D11VertexShader * m_vertexShader;
	ID3D11PixelShader* m_pixelShader;
	ID3D11RasterizerState* m_solidState;
	ID3D11Buffer* m_matrixBuffer;
	ID3D11InputLayout* m_layout;
	Sphere m_sphere;
	bool m_dashed;
};

class CDXBrushTranslator : public CDXPicker
{
public:
	CDXBrushTranslator(CDXBrush * brush, CDXBrushMesh * brushMesh, CDXBrushMesh * brushMeshMirror)
		: m_brush(brush)
		, m_brushMesh(brushMesh)
		, m_brushMeshMirror(brushMeshMirror)
	{ }

	virtual bool Pick(CDXPickInfo & pickInfo, CDXMesh * mesh, bool isMirror) override;
	virtual bool Mirror() const override;

private:
	CDXBrush * m_brush;
	CDXBrushMesh * m_brushMesh;
	CDXBrushMesh * m_brushMeshMirror;
};