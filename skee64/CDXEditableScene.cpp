#include "CDXEditableScene.h"
#include "CDXShader.h"
#include "SculptTrace.h"

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
	EndPaint();
	m_brushes.clear();
}

bool CDXEditableScene::Setup(const CDXInitParams & initParams)
{
	++m_editorGeneration;
	CreateBrushes();
	return CDXScene::Setup(initParams);
}

void CDXEditableScene::Release()
{
	// End while stroke meshes still exist; undo data is then released as usual.
	EndPaint();
	++m_editorGeneration; // Pending history UI tasks belong to the previous editor.
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
	SKEE::SculptTrace::Count(SKEE::SculptTrace::Event::BrushChange);
	if (brushType != m_currentBrush) EndPaint();
	m_currentBrush = brushType;
}

bool CDXEditableScene::BeginPaint(CDXCamera* camera, int x, int y)
{
	SKEE::SculptTrace::Scope trace(SKEE::SculptTrace::Event::Begin);
	EndPaint(); // A missing release or duplicate press must not stack strokes.
	auto brush = GetCurrentBrush();
	if (!brush || m_meshes.empty()) return false;
	m_paintSession.Begin(brush);
	CDXBrushPickerBegin picker(brush);
	picker.SetMirror(brush->IsMirror());
	const auto hit = Pick(camera, x, y, picker);
	if (!hit) EndPaint(); // A masked/no-op begin can still have created stroke objects.
	return hit;
}

void CDXEditableScene::UpdatePaint(CDXCamera* camera, int x, int y)
{
	SKEE::SculptTrace::Scope trace(SKEE::SculptTrace::Event::Paint);
	auto brush = m_paintSession.Get();
	if (!brush) {
		SKEE::SculptTrace::Scope rejected(SKEE::SculptTrace::Event::RejectedPaint);
		return;
	}
	CDXBrushPickerUpdate picker(brush);
	picker.SetMirror(brush->IsMirror());
	Pick(camera, x, y, picker); // Move brush needs off-mesh rays during an active drag.
}

void CDXEditableScene::EndPaint()
{
	if (!m_paintSession.Get()) return;
	SKEE::SculptTrace::Scope trace(SKEE::SculptTrace::Event::End);
	m_paintSession.End();
}
