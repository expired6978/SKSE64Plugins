#ifndef __CDXEDITABLESCENE__
#define __CDXEDITABLESCENE__

#pragma once

#include "CDXScene.h"
#include "CDXBrush.h"
#include "CDXTypes.h"

typedef std::vector<CDXBrush*> CDXBrushList;

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

protected:
	CDXBrush::BrushType		m_currentBrush;
	CDXBrushList			m_brushes;
};

#endif
