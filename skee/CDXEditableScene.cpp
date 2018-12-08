#include "CDXEditableScene.h"
#include "CDXShader.h"

#ifdef FIXME

extern CDXModelViewerCamera		g_Camera;
extern CDXUndoStack				g_undoStack;

CDXEditableScene::CDXEditableScene() : CDXScene()
{
	m_currentBrush = CDXBrush::kBrushType_None;
}

void CDXEditableScene::CreateBrushes()
{
	m_brushes.push_back(new CDXMaskAddBrush);
	m_brushes.push_back(new CDXMaskSubtractBrush);
	m_brushes.push_back(new CDXInflateBrush);
	m_brushes.push_back(new CDXDeflateBrush);
	m_brushes.push_back(new CDXSmoothBrush);
	m_brushes.push_back(new CDXMoveBrush);
}

void CDXEditableScene::ReleaseBrushes()
{
	for (auto brush : m_brushes)
		delete brush;

	m_brushes.clear();
}

void CDXEditableScene::Setup(ID3D11Device * pDevice)
{
	CreateBrushes();
	CDXScene::Setup(pDevice);
}

void CDXEditableScene::Release()
{
	CDXScene::Release();
	ReleaseBrushes();
	g_undoStack.Release();
}

CDXBrush * CDXEditableScene::GetBrush(CDXBrush::BrushType brushType)
{
	for (auto brush : m_brushes) {
		if (brush->GetType() == brushType)
			return brush;
	}

	return NULL;
}

CDXBrush * CDXEditableScene::GetCurrentBrush()
{
	return GetBrush(m_currentBrush);
}

void CDXEditableScene::SetCurrentBrush(CDXBrush::BrushType brushType)
{
	m_currentBrush = brushType;
}

#endif