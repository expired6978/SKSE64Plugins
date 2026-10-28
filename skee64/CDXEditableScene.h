#ifndef __CDXEDITABLESCENE__
#define __CDXEDITABLESCENE__

#pragma once

#include "CDXScene.h"
#include "CDXBrush.h"
#include "CDXTypes.h"
#include "SculptStrokeSession.h"

#include <memory>

typedef std::vector<std::unique_ptr<CDXBrush>> CDXBrushList;

class CDXEditableScene : public CDXScene
{
public:
	CDXEditableScene();

	virtual bool Setup(const CDXInitParams & initParams) override;
	virtual void Release() override;

	virtual void CreateBrushes();
	virtual void ReleaseBrushes();

	CDXBrushList & GetBrushes() { return m_brushes; }
	CDXBrush * GetBrush(CDXBrush::BrushType brushType);
	CDXBrush * GetCurrentBrush();
	void SetCurrentBrush(CDXBrush::BrushType brushType);
	bool BeginPaint(CDXCamera* camera, int x, int y);
	void UpdatePaint(CDXCamera* camera, int x, int y);
	void EndPaint();
	std::uint64_t GetEditorGeneration() const { return m_editorGeneration; }
	bool HasActivePaint() const { return m_paintSession.Get() != nullptr; }

protected:
	CDXBrush::BrushType		m_currentBrush;
	CDXBrushList			m_brushes;
	SculptStrokeSession<CDXBrush> m_paintSession;
	std::uint64_t m_editorGeneration{};
};

#endif
