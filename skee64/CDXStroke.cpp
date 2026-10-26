#include "CDXStroke.h"
#include "CDXBrush.h"
#include <cstdint>

using namespace DirectX;

CDXStroke::CDXStroke(CDXBrush * brush, CDXEditableMesh * mesh)
{
	m_brush = brush;
	m_mesh = mesh;
	m_origin = XMVectorZero();
	m_mirror = false;
}

CDXUndoCommand::UndoType CDXBasicStroke::GetUndoType()
{
	return CDXUndoCommand::kUndoType_Stroke;
}

void CDXBasicStroke::Begin(CDXPickInfo & pickInfo)
{
	m_origin = pickInfo.origin;
}

void CDXBasicHitStroke::Redo()
{
	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices)
		return;

	// Do what we have now
	for (auto it : m_current)
		XMStoreFloat3(&pVertices[it.first].Position, XMVectorAdd(XMLoadFloat3(&pVertices[it.first].Position), it.second));

	vertexAccess.Release();
}

void CDXBasicHitStroke::Undo()
{
	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices)
		return;

	// Undo what we did
	for (auto it : m_current)
		XMStoreFloat3(&pVertices[it.first].Position, XMVectorSubtract(XMLoadFloat3(&pVertices[it.first].Position), it.second));

	vertexAccess.Release();
}

CDXMaskAddStroke::~CDXMaskAddStroke()
{
	m_previous.clear();
	m_current.clear();
}

CDXStroke::StrokeType CDXMaskAddStroke::GetStrokeType()
{
	return kStrokeType_Mask_Add;
}

void CDXMaskAddStroke::Redo()
{
	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices)
		return;

	// Do what we have now
	for (auto it : m_current)
		pVertices[it.first].Color = it.second;

	vertexAccess.Release();
}

void CDXMaskAddStroke::Undo()
{
	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices)
		return;

	// Undo what we did
	for (auto it : m_previous)
		pVertices[it.first].Color = it.second;

	vertexAccess.Release();
}

void CDXMaskAddStroke::Update(CDXStroke::Info * info)
{
	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices)
		return;

	// Place the new info in the new map, update the colors
	CDXColor color;
	XMStoreFloat3(&color, COLOR_SELECTED);
	auto ret = m_current.emplace(info->index, color);
	if (ret.second)
		m_previous.emplace(info->index, pVertices[info->index].Color);

	pVertices[info->index].Color = color;

	vertexAccess.Release();
}

CDXStroke::StrokeType CDXMaskSubtractStroke::GetStrokeType()
{
	return kStrokeType_Mask_Subtract;
}

void CDXMaskSubtractStroke::Update(CDXStroke::Info * info)
{
	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices)
		return;

	// Place the new info in the new map, update the colors
	CDXColor color;
	XMStoreFloat3(&color, COLOR_UNSELECTED);

	if (XMVector3NotEqual(XMLoadFloat3(&pVertices[info->index].Color), COLOR_UNSELECTED)) {
		auto ret = m_current.emplace(info->index, color);
		if (ret.second)
			m_previous.emplace(info->index, pVertices[info->index].Color);

		pVertices[info->index].Color = color;
	}
	vertexAccess.Release();
}

CDXInflateStroke::~CDXInflateStroke()
{
	m_current.clear();
}

CDXStroke::StrokeType CDXInflateStroke::GetStrokeType()
{
	return kStrokeType_Inflate;
}

void CDXInflateStroke::Update(CDXStroke::Info * info)
{
	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices)
		return;

	CDXVec vertexNormal = ((InflateInfo*)info)->normal;
	CDXVec difference = vertexNormal * (float)(info->strength * info->falloff);

	m_current.emplace(info->index, XMVectorZero());
	m_current[info->index] += difference;
	XMStoreFloat3(&pVertices[info->index].Position, XMVectorAdd(XMLoadFloat3(&pVertices[info->index].Position), difference));
	vertexAccess.Release();
}

CDXStroke::StrokeType CDXDeflateStroke::GetStrokeType()
{
	return kStrokeType_Deflate;
}

void CDXDeflateStroke::Update(CDXStroke::Info * info)
{
	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices)
		return;

	CDXVec vertexNormal = ((InflateInfo*)info)->normal;
	CDXVec difference = vertexNormal * (float)(info->strength * info->falloff);

	m_current.emplace(info->index, XMVectorZero());
	m_current[info->index] -= difference;
	XMStoreFloat3(&pVertices[info->index].Position, XMVectorSubtract(XMLoadFloat3(&pVertices[info->index].Position), difference));
	vertexAccess.Release();
}

CDXSmoothStroke::~CDXSmoothStroke()
{
	m_current.clear();
}

CDXStroke::StrokeType CDXSmoothStroke::GetStrokeType()
{
	return kStrokeType_Smooth;
}

void CDXSmoothStroke::Update(CDXStroke::Info * info)
{
	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices)
		return;

	CDXVec newPos = XMVectorZero();

	std::uint32_t totalCount = 0;
	m_mesh->VisitAdjacencies(info->index, [&](CDXMeshFace & face)
	{
		CDXMeshIndex m1 = face.v1;
		if (m1 >= m_mesh->GetVertexCount())
			return false;

		CDXMeshIndex m2 = face.v2;
		if (m2 >= m_mesh->GetVertexCount())
			return false;
		
		CDXMeshIndex m3 = face.v3;
		if (m3 >= m_mesh->GetVertexCount())
			return false;
		
		CDXVec sum = XMLoadFloat3(&pVertices[m1].Position) + XMLoadFloat3(&pVertices[m2].Position) + XMLoadFloat3(&pVertices[m3].Position);
		newPos += sum / 3;
		totalCount++;
		return false;
	});

	newPos /= (float)totalCount;

	CDXVec difference = (newPos - XMLoadFloat3(&pVertices[info->index].Position)) * (float)(info->strength * info->falloff);
	
	m_current.emplace(info->index, XMVectorZero());
	m_current[info->index] += difference;
	XMStoreFloat3(&pVertices[info->index].Position, XMVectorAdd(XMLoadFloat3(&pVertices[info->index].Position), difference));
	vertexAccess.Release();
}

CDXMoveStroke::~CDXMoveStroke()
{
	m_previous.clear();
	m_current.clear();
	m_hitIndices.clear();
}

CDXStroke::StrokeType CDXMoveStroke::GetStrokeType()
{
	return kStrokeType_Move;
}

void CDXMoveStroke::Begin(CDXPickInfo & pickInfo)
{
	CDXBasicStroke::Begin(pickInfo);
	m_rayInfo = pickInfo.ray;
}

void CDXMoveStroke::Redo()
{
	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices)
		return;

	// Do what we have now
	for (auto it : m_current)
		XMStoreFloat3(&pVertices[it.first].Position, XMVectorAdd(XMLoadFloat3(&pVertices[it.first].Position), it.second));

	vertexAccess.Release();
}

void CDXMoveStroke::Undo()
{
	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices)
		return;

	// Undo what we did
	for (auto it : m_current)
		XMStoreFloat3(&pVertices[it.first].Position, XMVectorSubtract(XMLoadFloat3(&pVertices[it.first].Position), it.second));

	vertexAccess.Release();
}

void CDXMoveStroke::Update(CDXStroke::Info * info)
{
	CDXMesh::VertexAccess vertexAccess(*m_mesh, CDXMesh::LockMode::WRITE);
	auto* pVertices = vertexAccess.Get();
	if (!pVertices)
		return;

	m_previous.emplace(info->index, XMLoadFloat3(&pVertices[info->index].Position));
	CDXVec newPosition = m_previous[info->index] + ((MoveInfo*)info)->offset * (float)(info->strength * info->falloff);
	CDXVec difference = newPosition - XMLoadFloat3(&pVertices[info->index].Position);

	m_current.emplace(info->index, XMVectorZero());
	m_current[info->index] += difference;
	XMStoreFloat3(&pVertices[info->index].Position, XMVectorAdd(XMLoadFloat3(&pVertices[info->index].Position), difference));

	vertexAccess.Release();
}

void CDXMoveStroke::End()
{
	m_hitIndices.clear();
}
