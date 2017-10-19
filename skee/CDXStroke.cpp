#ifdef FIXME

#include "CDXStroke.h"
#include "CDXBrush.h"

CDXStroke::CDXStroke(CDXBrush * brush, CDXEditableMesh * mesh)
{
	m_brush = brush;
	m_mesh = mesh;
	m_origin = CDXVec3(0.0f, 0.0f, 0.0f);
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
	CDXMeshVert* pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	// Do what we have now
	for (auto it : m_current)
		pVertices[it.first].Position += it.second;

	m_mesh->UnlockVertices();
}

void CDXBasicHitStroke::Undo()
{
	CDXMeshVert* pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	// Undo what we did
	for (auto it : m_current)
		pVertices[it.first].Position -= it.second;

	m_mesh->UnlockVertices();
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
	CDXMeshVert* pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	// Do what we have now
	for (auto it : m_current)
		pVertices[it.first].Color = it.second;

	m_mesh->UnlockVertices();
}

void CDXMaskAddStroke::Undo()
{
	CDXMeshVert* pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	// Undo what we did
	for (auto it : m_previous)
		pVertices[it.first].Color = it.second;

	m_mesh->UnlockVertices();
}

void CDXMaskAddStroke::Update(CDXStroke::Info * info)
{
	CDXMeshVert * pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	// Place the new info in the new map, update the colors
	CDXColor color = COLOR_SELECTED;
	auto ret = m_current.emplace(info->index, color);
	if (ret.second)
		m_previous.emplace(info->index, pVertices[info->index].Color);

	pVertices[info->index].Color = color;

	m_mesh->UnlockVertices();
}

CDXStroke::StrokeType CDXMaskSubtractStroke::GetStrokeType()
{
	return kStrokeType_Mask_Subtract;
}

void CDXMaskSubtractStroke::Update(CDXStroke::Info * info)
{
	CDXMeshVert * pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	// Place the new info in the new map, update the colors
	CDXColor color = COLOR_UNSELECTED;
	if (pVertices[info->index].Color != color) {
		auto ret = m_current.emplace(info->index, color);
		if (ret.second)
			m_previous.emplace(info->index, pVertices[info->index].Color);

		pVertices[info->index].Color = color;
	}
	m_mesh->UnlockVertices();
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
	CDXMeshVert * pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	CDXVec3 vertexNormal = ((InflateInfo*)info)->normal;
	CDXVec3 difference = vertexNormal * info->strength * info->falloff;	

	m_current.emplace(info->index, CDXVec3(0, 0, 0));
	m_current[info->index] += difference;
	pVertices[info->index].Position += difference;
	m_mesh->UnlockVertices();
}

CDXStroke::StrokeType CDXDeflateStroke::GetStrokeType()
{
	return kStrokeType_Deflate;
}

void CDXDeflateStroke::Update(CDXStroke::Info * info)
{
	CDXMeshVert * pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	CDXVec3 vertexNormal = ((InflateInfo*)info)->normal;
	CDXVec3 difference = vertexNormal * info->strength * info->falloff;

	m_current.emplace(info->index, CDXVec3(0, 0, 0));
	m_current[info->index] -= difference;
	pVertices[info->index].Position -= difference;
	m_mesh->UnlockVertices();
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
	CDXMeshVert * pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	CDXVec3 newPos = CDXVec3(0, 0, 0);

	UInt32 totalCount = 0;
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
		
		CDXVec3 sum = pVertices[m1].Position + pVertices[m2].Position + pVertices[m3].Position;
		newPos += sum / 3;
		totalCount++;
		return false;
	});

	newPos /= totalCount;

	CDXVec3 difference = (newPos - pVertices[info->index].Position) * info->strength * info->falloff;
	
	m_current.emplace(info->index, CDXVec3(0,0,0));
	m_current[info->index] += difference;
	pVertices[info->index].Position += difference;
	m_mesh->UnlockVertices();
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
	CDXMeshVert* pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	// Do what we have now
	for (auto it : m_current)
		pVertices[it.first].Position += it.second;

	m_mesh->UnlockVertices();
}

void CDXMoveStroke::Undo()
{
	CDXMeshVert* pVertices = m_mesh->LockVertices();
	if (!pVertices)
		return;

	// Undo what we did
	for (auto it : m_current)
		pVertices[it.first].Position -= it.second;

	m_mesh->UnlockVertices();
}

void CDXMoveStroke::Update(CDXStroke::Info * info)
{
	CDXMeshVert * pVertices = m_mesh->LockVertices();

	m_previous.emplace(info->index, pVertices[info->index].Position);
	CDXVec3 newPosition = m_previous[info->index] + ((MoveInfo*)info)->offset * info->strength * info->falloff;
	CDXVec3 difference = newPosition - pVertices[info->index].Position;

	m_current.emplace(info->index, CDXVec3(0, 0, 0));
	m_current[info->index] += difference;
	pVertices[info->index].Position += difference;

	m_mesh->UnlockVertices();
}

void CDXMoveStroke::End()
{
	m_hitIndices.clear();
}

#endif