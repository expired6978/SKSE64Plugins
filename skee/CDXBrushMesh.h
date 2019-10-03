#pragma once

#include "CDXMesh.h"
#include "CDXPicker.h"

class CDXBrush;
class CDXShaderFile;

class CDXBrushMesh : public CDXMesh
{
public:
	CDXBrushMesh() : m_dashed(false), m_drawPoint(true), m_drawRadius(true), CDXMesh() { };

	bool Create(CDXD3DDevice * pDevice, bool dashed, DirectX::XMVECTOR ringColor, DirectX::XMVECTOR dotColor, CDXShaderFactory* factory, CDXShaderFile* pixelShader, CDXShaderFile* precompiledPixelShader, CDXShaderFile* vertexShader, CDXShaderFile* precompiledVertexShader);

	virtual void Render(CDXD3DDevice * pDevice, CDXShader * shader) override;
	virtual bool Pick(CDXRayInfo & rayInfo, CDXPickInfo & pickInfo) override;

	const CDXMatrix & GetSphereTransform() { return m_sphere.m_transform; };
	void SetSphereTransform(const CDXMatrix & mat) { m_sphere.m_transform = mat; }
	void SetDrawPoint(bool draw) { m_drawPoint = draw; }
	void SetDrawRadius(bool draw) { m_drawRadius = draw; }

protected:
	struct Sphere
	{
		std::vector<CDXMesh::ColoredPrimitive> m_vertices;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
		std::vector<CDXMeshIndex> m_indices;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
		CDXMatrix m_transform;
	};

	void ComputeSphere(std::vector<CDXMesh::ColoredPrimitive>& vertices, std::vector<CDXMeshIndex> & indices, float radius, int sliceCount, int stackCount, DirectX::XMVECTOR color);

private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_solidState;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_matrixBuffer;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_layout;
	Sphere m_sphere;
	bool m_dashed;
	bool m_drawPoint;
	bool m_drawRadius;
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