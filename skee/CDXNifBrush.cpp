#include "CDXNifBrush.h"
#include "CDXNifCommands.h"

#ifdef FIXME

CDXStrokePtr CDXNifMaskAddBrush::CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh)
{
	return std::make_shared<CDXNifMaskAddStroke>(brush, mesh);
}

CDXStrokePtr CDXNifMaskSubtractBrush::CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh)
{
	return std::make_shared<CDXNifMaskSubtractStroke>(brush, mesh);
}

CDXStrokePtr CDXNifInflateBrush::CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh)
{
	return std::make_shared<CDXNifInflateStroke>(brush, mesh);
}

CDXStrokePtr CDXNifDeflateBrush::CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh)
{
	return std::make_shared<CDXNifDeflateStroke>(brush, mesh);
}

CDXStrokePtr CDXNifSmoothBrush::CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh)
{
	return std::make_shared<CDXNifSmoothStroke>(brush, mesh);
}

CDXStrokePtr CDXNifMoveBrush::CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh)
{
	return std::make_shared<CDXNifMoveStroke>(brush, mesh);
}

#endif