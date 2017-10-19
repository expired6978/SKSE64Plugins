#ifdef FIXME

#include "CDXBrush.h"
#include "CDXMesh.h"
#include "CDXUndo.h"
#include "CDXShader.h"
#include "CDXMaterial.h"

double g_brushProperties[CDXBrush::kBrushTypes][CDXBrush::kBrushProperties][CDXBrush::kBrushPropertyValues];

void CDXBrush::InitGlobals()
{
	g_brushProperties[kBrushType_Mask_Add][kBrushProperty_Radius][kBrushPropertyValue_Value] = 0.5;
	g_brushProperties[kBrushType_Mask_Add][kBrushProperty_Radius][kBrushPropertyValue_Interval] = 0.01;
	g_brushProperties[kBrushType_Mask_Add][kBrushProperty_Radius][kBrushPropertyValue_Min] = 0.01;
	g_brushProperties[kBrushType_Mask_Add][kBrushProperty_Radius][kBrushPropertyValue_Max] = 2.00;
	g_brushProperties[kBrushType_Mask_Add][kBrushProperty_Strength][kBrushPropertyValue_Value] = 0.0;
	g_brushProperties[kBrushType_Mask_Add][kBrushProperty_Strength][kBrushPropertyValue_Interval] = 0.0;
	g_brushProperties[kBrushType_Mask_Add][kBrushProperty_Strength][kBrushPropertyValue_Min] = 0.0;
	g_brushProperties[kBrushType_Mask_Add][kBrushProperty_Strength][kBrushPropertyValue_Max] = 0.0;
	g_brushProperties[kBrushType_Mask_Add][kBrushProperty_Falloff][kBrushPropertyValue_Value] = 0.0;
	g_brushProperties[kBrushType_Mask_Add][kBrushProperty_Falloff][kBrushPropertyValue_Interval] = 0.0;
	g_brushProperties[kBrushType_Mask_Add][kBrushProperty_Falloff][kBrushPropertyValue_Min] = 0.0;
	g_brushProperties[kBrushType_Mask_Add][kBrushProperty_Falloff][kBrushPropertyValue_Max] = 0.0;

	g_brushProperties[kBrushType_Mask_Subtract][kBrushProperty_Radius][kBrushPropertyValue_Value] = 0.5;
	g_brushProperties[kBrushType_Mask_Subtract][kBrushProperty_Radius][kBrushPropertyValue_Interval] = 0.01;
	g_brushProperties[kBrushType_Mask_Subtract][kBrushProperty_Radius][kBrushPropertyValue_Min] = 0.01;
	g_brushProperties[kBrushType_Mask_Subtract][kBrushProperty_Radius][kBrushPropertyValue_Max] = 2.00;
	g_brushProperties[kBrushType_Mask_Subtract][kBrushProperty_Strength][kBrushPropertyValue_Value] = 0.0;
	g_brushProperties[kBrushType_Mask_Subtract][kBrushProperty_Strength][kBrushPropertyValue_Interval] = 0.0;
	g_brushProperties[kBrushType_Mask_Subtract][kBrushProperty_Strength][kBrushPropertyValue_Min] = 0.0;
	g_brushProperties[kBrushType_Mask_Subtract][kBrushProperty_Strength][kBrushPropertyValue_Max] = 0.0;
	g_brushProperties[kBrushType_Mask_Subtract][kBrushProperty_Falloff][kBrushPropertyValue_Value] = 0.0;
	g_brushProperties[kBrushType_Mask_Subtract][kBrushProperty_Falloff][kBrushPropertyValue_Interval] = 0.0;
	g_brushProperties[kBrushType_Mask_Subtract][kBrushProperty_Falloff][kBrushPropertyValue_Min] = 0.0;
	g_brushProperties[kBrushType_Mask_Subtract][kBrushProperty_Falloff][kBrushPropertyValue_Max] = 0.0;

	g_brushProperties[kBrushType_Inflate][kBrushProperty_Radius][kBrushPropertyValue_Value] = 0.5;
	g_brushProperties[kBrushType_Inflate][kBrushProperty_Radius][kBrushPropertyValue_Interval] = 0.01;
	g_brushProperties[kBrushType_Inflate][kBrushProperty_Radius][kBrushPropertyValue_Min] = 0.01;
	g_brushProperties[kBrushType_Inflate][kBrushProperty_Radius][kBrushPropertyValue_Max] = 2.00;
	g_brushProperties[kBrushType_Inflate][kBrushProperty_Strength][kBrushPropertyValue_Value] = 0.01;
	g_brushProperties[kBrushType_Inflate][kBrushProperty_Strength][kBrushPropertyValue_Interval] = 0.001;
	g_brushProperties[kBrushType_Inflate][kBrushProperty_Strength][kBrushPropertyValue_Min] = 0.001;
	g_brushProperties[kBrushType_Inflate][kBrushProperty_Strength][kBrushPropertyValue_Max] = 1.000;
	g_brushProperties[kBrushType_Inflate][kBrushProperty_Falloff][kBrushPropertyValue_Value] = 1.0;
	g_brushProperties[kBrushType_Inflate][kBrushProperty_Falloff][kBrushPropertyValue_Interval] = 0.001;
	g_brushProperties[kBrushType_Inflate][kBrushProperty_Falloff][kBrushPropertyValue_Min] = 0.0;
	g_brushProperties[kBrushType_Inflate][kBrushProperty_Falloff][kBrushPropertyValue_Max] = 1.0;

	g_brushProperties[kBrushType_Deflate][kBrushProperty_Radius][kBrushPropertyValue_Value] = 0.5;
	g_brushProperties[kBrushType_Deflate][kBrushProperty_Radius][kBrushPropertyValue_Interval] = 0.01;
	g_brushProperties[kBrushType_Deflate][kBrushProperty_Radius][kBrushPropertyValue_Min] = 0.01;
	g_brushProperties[kBrushType_Deflate][kBrushProperty_Radius][kBrushPropertyValue_Max] = 2.00;
	g_brushProperties[kBrushType_Deflate][kBrushProperty_Strength][kBrushPropertyValue_Value] = 0.01;
	g_brushProperties[kBrushType_Deflate][kBrushProperty_Strength][kBrushPropertyValue_Interval] = 0.001;
	g_brushProperties[kBrushType_Deflate][kBrushProperty_Strength][kBrushPropertyValue_Min] = 0.001;
	g_brushProperties[kBrushType_Deflate][kBrushProperty_Strength][kBrushPropertyValue_Max] = 1.000;
	g_brushProperties[kBrushType_Deflate][kBrushProperty_Falloff][kBrushPropertyValue_Value] = 1.0;
	g_brushProperties[kBrushType_Deflate][kBrushProperty_Falloff][kBrushPropertyValue_Interval] = 0.001;
	g_brushProperties[kBrushType_Deflate][kBrushProperty_Falloff][kBrushPropertyValue_Min] = 0.0;
	g_brushProperties[kBrushType_Deflate][kBrushProperty_Falloff][kBrushPropertyValue_Max] = 1.0;

	g_brushProperties[kBrushType_Smooth][kBrushProperty_Radius][kBrushPropertyValue_Value] = 0.5;
	g_brushProperties[kBrushType_Smooth][kBrushProperty_Radius][kBrushPropertyValue_Interval] = 0.01;
	g_brushProperties[kBrushType_Smooth][kBrushProperty_Radius][kBrushPropertyValue_Min] = 0.01;
	g_brushProperties[kBrushType_Smooth][kBrushProperty_Radius][kBrushPropertyValue_Max] = 2.00;
	g_brushProperties[kBrushType_Smooth][kBrushProperty_Strength][kBrushPropertyValue_Value] = 0.05;
	g_brushProperties[kBrushType_Smooth][kBrushProperty_Strength][kBrushPropertyValue_Interval] = 0.001;
	g_brushProperties[kBrushType_Smooth][kBrushProperty_Strength][kBrushPropertyValue_Min] = 0.001;
	g_brushProperties[kBrushType_Smooth][kBrushProperty_Strength][kBrushPropertyValue_Max] = 1.000;
	g_brushProperties[kBrushType_Smooth][kBrushProperty_Falloff][kBrushPropertyValue_Value] = 1.0;
	g_brushProperties[kBrushType_Smooth][kBrushProperty_Falloff][kBrushPropertyValue_Interval] = 0.001;
	g_brushProperties[kBrushType_Smooth][kBrushProperty_Falloff][kBrushPropertyValue_Min] = 0.0;
	g_brushProperties[kBrushType_Smooth][kBrushProperty_Falloff][kBrushPropertyValue_Max] = 1.0;

	g_brushProperties[kBrushType_Move][kBrushProperty_Radius][kBrushPropertyValue_Value] = 0.5;
	g_brushProperties[kBrushType_Move][kBrushProperty_Radius][kBrushPropertyValue_Interval] = 0.01;
	g_brushProperties[kBrushType_Move][kBrushProperty_Radius][kBrushPropertyValue_Min] = 0.01;
	g_brushProperties[kBrushType_Move][kBrushProperty_Radius][kBrushPropertyValue_Max] = 2.00;
	g_brushProperties[kBrushType_Move][kBrushProperty_Strength][kBrushPropertyValue_Value] = 0.01;
	g_brushProperties[kBrushType_Move][kBrushProperty_Strength][kBrushPropertyValue_Interval] = 0.001;
	g_brushProperties[kBrushType_Move][kBrushProperty_Strength][kBrushPropertyValue_Min] = 0.001;
	g_brushProperties[kBrushType_Move][kBrushProperty_Strength][kBrushPropertyValue_Max] = 1.000;
	g_brushProperties[kBrushType_Move][kBrushProperty_Falloff][kBrushPropertyValue_Value] = 1.0;
	g_brushProperties[kBrushType_Move][kBrushProperty_Falloff][kBrushPropertyValue_Interval] = 0.001;
	g_brushProperties[kBrushType_Move][kBrushProperty_Falloff][kBrushPropertyValue_Min] = 0.0;
	g_brushProperties[kBrushType_Move][kBrushProperty_Falloff][kBrushPropertyValue_Max] = 1.0;
}

CDXBrush::CDXBrush()
{
	m_mirror = true;
}

void CDXBasicBrush::EndStroke()
{
	for (auto stroke : m_strokes) {
		stroke->End();
		if (stroke->Length() > 0) {
			stroke->Apply(g_undoStack.Push(stroke));
		}
	}

	m_strokes.clear();
}

CDXHitIndexMap CDXBasicBrush::GetHitIndices(CDXPickInfo & pickInfo, CDXEditableMesh * mesh)
{
	return CDXHitIndexMap();
}

float CDXBrush::CalculateFalloff(float & dist)
{
	double radius = m_property[kBrushProperty_Radius][kBrushPropertyValue_Value];
	double falloff = m_property[kBrushProperty_Falloff][kBrushPropertyValue_Value];

	if (falloff > 1.0)
		falloff = 1.0;
	if (falloff < 0)
		falloff = 0;

	// X1 = 0    Y1 = 1.0
	// X2 = 1.0  Y2 = falloff

	// m = Y2 - Y1
	//     X2 - X1
	// y - Y1 = m(x - X1)

	//double y = (falloff - 1.0) * (dist / radius) + 1.0;
	//y = y*y*(3 - 2 * y);

	// X1 = 1.0
	// Y1 = falloff
	// X2 = 0
	// Y2 = 1.0

	// m = Y2 - Y1
	//     X2 - X1
	// y - Y1 = m(x - X1)

	double y = -(1.0 - (1.0 - falloff)) * ((dist / radius) - 1.0) + (1.0 - falloff);
	y = y*y*(3 - 2 * y);

	return y;
	
	/*double p = 1.0f;

	double x = dist / radius; // (radius - dist)/dist;
	if (x <= (falloff - 1.0f) / (falloff)) {
		p = 1.0f;
	}
	else {
		float fx;
		float p1 = 0.0f;
		float p2 = 0.0f;
		if (falloff >= 1.0f) {
			fx = (falloff - 1.0f) - (falloff*x);
			p1 = (1.0f - fx*fx);
			p2 = (1.0f - pow(fx, 4));
		}
		else if (falloff > 0.0f) {
			fx = x / falloff;
			p1 = (1.0f - x*x);
			p2 = (1.0f - pow(fx, 4));
		}

		p = p1*p2;
		if (p < 0.0f) p = 0.0f;
	}

	return p;*/
}

CDXHitIndexMap CDXBasicHitBrush::GetHitIndices(CDXPickInfo & pickInfo, CDXEditableMesh * mesh)
{
	CDXMeshVert * pVertices = mesh->LockVertices();
	CDXHitIndexMap hitVertex;
	for (UInt32 i = 0; i < mesh->GetVertexCount(); i++) {
		if (FilterVertex(mesh, pVertices, i))
			continue;
		CDXVec3 vTest = pVertices[i].Position;
		CDXVec3 vDiff = pickInfo.origin - vTest;
		float testRadius = D3DXVec3Length(&vDiff); // Spherical radius
		if (testRadius <= m_property[kBrushProperty_Radius][kBrushPropertyValue_Value]) {
			hitVertex.emplace(i, CalculateFalloff(testRadius));
		}
	}
	mesh->UnlockVertices();
	return hitVertex;
}

bool CDXBasicHitBrush::FilterVertex(CDXEditableMesh * mesh, CDXMeshVert * pVertices, CDXMeshIndex i)
{
	if (pVertices[i].Color != COLOR_UNSELECTED)
		return true;

	return false;
}

bool CDXBasicHitBrush::BeginStroke(CDXPickInfo & pickInfo, CDXEditableMesh * mesh, bool isMirror)
{
	if (!pickInfo.isHit)
		return false;

	CDXStrokePtr stroke = CreateStroke(this, mesh);
	stroke->SetMirror(isMirror);
	stroke->Begin(pickInfo);
	m_strokes.push_back(stroke);

	// Count the beginning as the first stroke
	return UpdateStroke(pickInfo, mesh, isMirror);
}

bool CDXBrushPickerBegin::Pick(CDXPickInfo & pickInfo, CDXMesh * mesh, bool isMirror)
{
	if (mesh->IsEditable())
		return m_brush->BeginStroke(pickInfo, (CDXEditableMesh*)mesh, isMirror);

	return false;
}

bool CDXBrushPickerUpdate::Pick(CDXPickInfo & pickInfo, CDXMesh * mesh, bool isMirror)
{
	if (mesh->IsEditable())
		return m_brush->UpdateStroke(pickInfo, (CDXEditableMesh*)mesh, isMirror);

	return false;
}

CDXMaskAddBrush::CDXMaskAddBrush() : CDXBasicHitBrush()
{
	for (UInt32 p = 0; p < CDXBrush::kBrushProperties; p++)
		for (UInt32 v = 0; v < CDXBrush::kBrushPropertyValues; v++)
			m_property[p][v] = g_brushProperties[CDXBrush::kBrushType_Mask_Add][p][v];
}

CDXStrokePtr CDXMaskAddBrush::CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh)
{
	return std::make_shared<CDXMaskAddStroke>(this, mesh);
}

bool CDXMaskAddBrush::UpdateStroke(CDXPickInfo & pickInfo, CDXEditableMesh * mesh, bool isMirror)
{
	if (!pickInfo.isHit)
		return false;

	for (auto stroke : m_strokes) {
		if (stroke->IsMirror() != isMirror)
			continue;

		auto hitVertex = GetHitIndices(pickInfo, stroke->GetMesh());
		for (auto i : hitVertex) {
			CDXStroke::Info strokeInfo;
			strokeInfo.index = i.first;
			strokeInfo.strength = m_property[kBrushProperty_Strength][kBrushPropertyValue_Value];
			strokeInfo.falloff = i.second;
			stroke->Update(&strokeInfo);
		}
	}

	return true;
}

CDXMaskSubtractBrush::CDXMaskSubtractBrush() : CDXMaskAddBrush()
{
	for (UInt32 p = 0; p < CDXBrush::kBrushProperties; p++)
		for (UInt32 v = 0; v < CDXBrush::kBrushPropertyValues; v++)
			m_property[p][v] = g_brushProperties[CDXBrush::kBrushType_Mask_Subtract][p][v];
}

CDXStrokePtr CDXMaskSubtractBrush::CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh)
{
	return std::make_shared<CDXMaskSubtractStroke>(this, mesh);
}

bool CDXMaskSubtractBrush::FilterVertex(CDXEditableMesh * mesh, CDXMeshVert * pVertices, CDXMeshIndex i)
{
	return false;
}

CDXInflateBrush::CDXInflateBrush() : CDXBasicHitBrush()
{
	for (UInt32 p = 0; p < CDXBrush::kBrushProperties; p++)
		for (UInt32 v = 0; v < CDXBrush::kBrushPropertyValues; v++)
			m_property[p][v] = g_brushProperties[CDXBrush::kBrushType_Inflate][p][v];
}

CDXStrokePtr CDXInflateBrush::CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh)
{
	return std::make_shared<CDXInflateStroke>(this, mesh);
}

bool CDXInflateBrush::UpdateStroke(CDXPickInfo & pickInfo, CDXEditableMesh * mesh, bool isMirror)
{
	if (!pickInfo.isHit)
		return false;

	for (auto stroke : m_strokes) {
		if (stroke->IsMirror() != isMirror)
			continue;

		auto hitVertex = GetHitIndices(pickInfo, stroke->GetMesh());

		CDXVec3 normal(0, 0, 0);
		for (auto i : hitVertex) {
			normal += mesh->CalculateVertexNormal(i.first);
		}
		D3DXVec3Normalize(&normal, &normal);

		for (auto i : hitVertex) {
			CDXInflateStroke::InflateInfo strokeInfo;
			strokeInfo.index = i.first;
			strokeInfo.strength = m_property[kBrushProperty_Strength][kBrushPropertyValue_Value];
			strokeInfo.falloff = i.second;
			strokeInfo.normal = normal;
			stroke->Update(&strokeInfo);
		}
	}

	return true;
}

CDXDeflateBrush::CDXDeflateBrush() : CDXInflateBrush()
{
	for (UInt32 p = 0; p < CDXBrush::kBrushProperties; p++)
		for (UInt32 v = 0; v < CDXBrush::kBrushPropertyValues; v++)
			m_property[p][v] = g_brushProperties[CDXBrush::kBrushType_Deflate][p][v];
}

CDXStrokePtr CDXDeflateBrush::CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh)
{
	return std::make_shared<CDXDeflateStroke>(this, mesh);
}

CDXSmoothBrush::CDXSmoothBrush() : CDXBasicHitBrush()
{
	for (UInt32 p = 0; p < CDXBrush::kBrushProperties; p++)
		for (UInt32 v = 0; v < CDXBrush::kBrushPropertyValues; v++)
			m_property[p][v] = g_brushProperties[CDXBrush::kBrushType_Smooth][p][v];
}

CDXStrokePtr CDXSmoothBrush::CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh)
{
	return std::make_shared<CDXSmoothStroke>(this, mesh);
}

bool CDXSmoothBrush::FilterVertex(CDXEditableMesh * mesh, CDXMeshVert * pVertices, CDXMeshIndex i)
{
	if (CDXBasicHitBrush::FilterVertex(mesh, pVertices, i))
		return true;

	if (mesh->IsEdgeVertex(i))
		return true;

	return false;
}

bool CDXSmoothBrush::UpdateStroke(CDXPickInfo & pickInfo, CDXEditableMesh * mesh, bool isMirror)
{
	if (!pickInfo.isHit)
		return false;

	for (auto stroke : m_strokes) {
		if (stroke->IsMirror() != isMirror)
			continue;

		auto hitVertex = GetHitIndices(pickInfo, stroke->GetMesh());
		for (auto i : hitVertex) {
			CDXStroke::Info strokeInfo;
			strokeInfo.index = i.first;
			strokeInfo.strength = m_property[kBrushProperty_Strength][kBrushPropertyValue_Value];
			strokeInfo.falloff = i.second;
			stroke->Update(&strokeInfo);
		}
	}

	return true;
}

CDXMoveBrush::CDXMoveBrush() : CDXBasicHitBrush()
{
	for (UInt32 p = 0; p < CDXBrush::kBrushProperties; p++)
		for (UInt32 v = 0; v < CDXBrush::kBrushPropertyValues; v++)
			m_property[p][v] = g_brushProperties[CDXBrush::kBrushType_Move][p][v];
}

CDXStrokePtr CDXMoveBrush::CreateStroke(CDXBrush * brush, CDXEditableMesh * mesh)
{
	return std::make_shared<CDXMoveStroke>(this, mesh);
}

bool CDXMoveBrush::BeginStroke(CDXPickInfo & pickInfo, CDXEditableMesh * mesh, bool isMirror)
{
	if (!pickInfo.isHit)
		return false;

	CDXStrokePtr stroke = CreateStroke(this, mesh);
	stroke->SetMirror(isMirror);
	auto hitIndices = GetHitIndices(pickInfo, mesh);
	auto moveStroke = std::dynamic_pointer_cast<CDXMoveStroke>(stroke);
	if (moveStroke)
		moveStroke->SetHitIndices(hitIndices);
	stroke->Begin(pickInfo);
	m_strokes.push_back(stroke);	
	return true;
}
/*
#ifdef _DEBUG
#include <sstream>
#include <thread>
#include <cstdio>
#include <cstdarg>
#include <cwchar>
#include <cstdlib>
#include <windows.h>

struct DebugOutImpl {
	DebugOutImpl(const char* location, int line, bool enabled_ = true)
		:enabled(enabled_)
	{
		ss << location << line << "): " << std::this_thread::get_id() << " - ";
	}
	~DebugOutImpl()  { ss << '\n'; OutputDebugStringA(ss.str().c_str()); }
	void operator()(const char* msg) { if (enabled) ss << msg; }
	void operator()(const wchar_t* msg) { if (enabled) ss << wtoa(msg); }
	std::ostringstream& operator()() { return ss; }
	template<class char_type>
	void operator()(const char_type* format, ...) {
		if (enabled) {
			char_type buf[4096];
			va_list ap;
			va_start(ap, format);
			vsnprintf_s(buf, 4096, format, ap);
			va_end(ap);
			operator()(buf);
		}
	}
private:
	static std::string wtoa(const wchar_t* ptr, size_t len = -1){
		if (len == -1) len = wcslen(ptr);
		std::string r(WideCharToMultiByte(CP_THREAD_ACP, 0, ptr, len, nullptr, 0, 0, 0), '\0');
		if (r.size() == 0) throw std::system_error(GetLastError(), std::system_category());
		if (0 == WideCharToMultiByte(CP_THREAD_ACP, 0, ptr, len, &r[0], r.size(), 0, 0))
			throw std::system_error(GetLastError(), std::system_category(), "error converting wide string to narrow");
		return r;
	}
	static inline std::string wtoa(const std::wstring& wstr) { return wtoa(&wstr[0], wstr.size()); }
	static int vsnprintf_s(char* buffer, int bufsize, const char* format, va_list ap) { return ::vsnprintf_s(buffer, bufsize, _TRUNCATE, format, ap); }
	static int vsnprintf_s(wchar_t* buffer, int bufsize, const wchar_t* format, va_list ap) { return ::vswprintf_s(buffer, bufsize, format, ap); }

	std::ostringstream ss;
	bool enabled;
};
#define DebugOut DebugOutImpl(__FILE__ "(", __LINE__)
#endif
*/
bool CDXMoveBrush::UpdateStroke(CDXPickInfo & pickInfo, CDXEditableMesh * mesh, bool isMirror)
{
	for (auto stroke : m_strokes) {
		if (stroke->IsMirror() != isMirror)
			continue;

		CDXMoveStroke * moveStroke = static_cast<CDXMoveStroke*>(stroke.get());
		for (auto i : moveStroke->GetHitIndices()) {
			CDXMoveStroke::MoveInfo strokeInfo;
			strokeInfo.index = i.first;
			strokeInfo.strength = m_property[kBrushProperty_Strength][kBrushPropertyValue_Value];
			strokeInfo.falloff = i.second;
/*#ifdef _DEBUG
			DebugOut("%d - %f", strokeInfo.index, strokeInfo.falloff);
#endif*/
			CDXRayInfo rayStart = moveStroke->GetRayInfo();
			CDXVec3 d = pickInfo.ray.point - rayStart.point;
			if (isMirror)
				d.x = -d.x;
			strokeInfo.offset = d;
			stroke->Update(&strokeInfo);
		}
	}

	return true;
}

#endif