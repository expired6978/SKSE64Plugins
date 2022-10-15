#include "CDXEditableScene.h"
#include "CDXShader.h"

extern CDXUndoStack				g_undoStack;

CDXEditableScene::CDXEditableScene() : CDXScene()
{
	m_currentBrush = CDXBrush::kBrushType_None;
}

void CDXEditableScene::CreateBrushes()
{
	m_brushes.emplace_back(std::make_unique<CDXMaskAddBrush>());
	m_brushes.emplace_back(std::make_unique<CDXMaskSubtractBrush>());
	m_brushes.emplace_back(std::make_unique<CDXInflateBrush>());
	m_brushes.emplace_back(std::make_unique<CDXDeflateBrush>());
	m_brushes.emplace_back(std::make_unique<CDXSmoothBrush>());
	m_brushes.emplace_back(std::make_unique<CDXMoveBrush>());
}

void CDXEditableScene::ReleaseBrushes()
{
	m_brushes.clear();
}

bool CDXEditableScene::Setup(const CDXInitParams & initParams)
{
	CreateBrushes();
	return CDXScene::Setup(initParams);
}

void CDXEditableScene::Release()
{
	CDXScene::Release();
	ReleaseBrushes();
	g_undoStack.Release();
}

CDXBrush * CDXEditableScene::GetBrush(CDXBrush::BrushType brushType)
{
	for (const auto& brush : m_brushes) {
		if (brush->GetType() == brushType)
			return brush.get();
	}

	return nullptr;
}

CDXBrush * CDXEditableScene::GetCurrentBrush()
{
	return GetBrush(m_currentBrush);
}

void CDXEditableScene::SetCurrentBrush(CDXBrush::BrushType brushType)
{
	m_currentBrush = brushType;
}
