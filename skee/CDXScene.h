#ifndef __CDXSCENE__
#define __CDXSCENE__

#pragma once

#include "CDXTypes.h"
#include <vector>

class CDXCamera;
class CDXD3DDevice;
class CDXMesh;
class CDXShader;
class CDXPicker;
class CDXBrush;
class CDXMaskAddBrush;
class CDXInflateBrush;
struct ShaderFileData;

typedef std::vector<CDXMesh*>	CDXMeshList;

class CDXScene
{
public:
	CDXScene();
	~CDXScene();

	virtual bool Setup(const CDXInitParams & initParams);
	virtual void Release();

	virtual void Render(CDXCamera * camera, CDXD3DDevice * device);

	virtual void Begin(CDXCamera * camera, CDXD3DDevice * device) { };
	virtual void End(CDXCamera * camera, CDXD3DDevice * device) { };

	size_t GetNumMeshes() { return m_meshes.size(); }
	CDXMesh * GetNthMesh(UInt32 i) { return m_meshes.at(i); }

	void AddMesh(CDXMesh * mesh);
	bool Pick(CDXCamera * camera, int x, int y, CDXPicker & picker);

	CDXShader	* GetShader() { return m_shader; }
	void SetVisible(bool visible) { m_visible = visible; }
	bool IsVisible() const { return m_visible; }

protected:
	bool m_visible;
	CDXShader * m_shader;
	CDXMeshList m_meshes;
};

#endif
