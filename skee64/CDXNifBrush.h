#ifndef __CDXNIFBRUSH__
#define __CDXNIFBRUSH__

#pragma once

#include "CDXBrush.h"

class CDXNifMaskAddBrush : public CDXMaskAddBrush
{
public:
	CDXNifMaskAddBrush() : CDXMaskAddBrush() { }
	virtual CDXStrokePtr CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh);
};

class CDXNifMaskSubtractBrush : public CDXMaskSubtractBrush
{
public:
	CDXNifMaskSubtractBrush() : CDXMaskSubtractBrush() { }
	virtual CDXStrokePtr CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh);
};

class CDXNifInflateBrush : public CDXInflateBrush
{
public:
	CDXNifInflateBrush() : CDXInflateBrush() { }
	virtual CDXStrokePtr CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh);
};

class CDXNifDeflateBrush : public CDXDeflateBrush
{
public:
	CDXNifDeflateBrush() : CDXDeflateBrush() { }
	virtual CDXStrokePtr CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh);
};

class CDXNifSmoothBrush : public CDXSmoothBrush
{
public:
	CDXNifSmoothBrush() : CDXSmoothBrush() { }
	virtual CDXStrokePtr CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh);
};

class CDXNifMoveBrush : public CDXMoveBrush
{
public:
	CDXNifMoveBrush() : CDXMoveBrush() { }
	virtual CDXStrokePtr CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh);
};

#endif
