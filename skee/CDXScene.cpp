#ifdef FIXME

#include "CDXScene.h"
#include "CDXCamera.h"
#include "CDXMesh.h"
#include "CDXShader.h"
#include "CDXMaterial.h"
#include "CDXPicker.h"
#include "CDXBrush.h"

#include <d3dx9mesh.h>

CDXModelViewerCamera		g_Camera;

CDXScene::CDXScene()
{
	m_shader = new CDXShader;
	m_pStateBlock = NULL;
	m_pMeshDecl = NULL;
	m_visible = true;
	m_width = 1024;
	m_height = 1024;
}

CDXScene::~CDXScene()
{
	if(m_shader) {
		delete m_shader;
		m_shader = NULL;
	}
}

void CDXScene::Release()
{
	if(m_pStateBlock) {
		m_pStateBlock->Release();
		m_pStateBlock = NULL;
	}
	if(m_pMeshDecl) {
		m_pMeshDecl->Release();
		m_pMeshDecl = NULL;
	}
	if(m_shader) {
		m_shader->Release();
	}

	for(auto it : m_meshes) {
		it->Release();
		delete it;
	}

	m_meshes.clear();
}

void CDXScene::Setup(LPDIRECT3DDEVICE9 pDevice)
{
	pDevice->CreateVertexDeclaration(VertexDecl, &m_pMeshDecl);

	m_shader->CreateEffect(pDevice);

	// Setup the camera's view parameters
	CDXVec3 vecEye(30.0f, 0.0f, 0.0f);
	CDXVec3 vecAt (0.0f, 0.0f, 0.0f);
	g_Camera.SetWindow(m_width, m_height);
	g_Camera.SetViewParams(&vecEye, &vecAt);
	g_Camera.Update();
}

void CDXScene::Begin(LPDIRECT3DDEVICE9 pDevice)
{
	if(!m_pStateBlock)
		pDevice->CreateStateBlock(D3DSBT_ALL,&m_pStateBlock);

	m_pStateBlock->Capture();
}

void CDXScene::End(LPDIRECT3DDEVICE9 pDevice)
{
	m_pStateBlock->Apply();
}

void CDXScene::Render(LPDIRECT3DDEVICE9 pDevice)
{
	CDXMatrix16 mWorld;
	CDXMatrix16 mView;
	CDXMatrix16 mProj;
	CDXMatrix16 mWorldViewProjection;

	pDevice->Clear(0,NULL,D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,0,1.0f,0);

	ID3DXEffect	* pEffect = m_shader->GetEffect();
	if(!pEffect)
		return;

	mWorld = *g_Camera.GetWorldMatrix();
	mView = *g_Camera.GetViewMatrix();
	mProj = *g_Camera.GetProjMatrix();
	mWorldViewProjection = mWorld * mView * mProj;

	CDXVec3 vAmbient = CDXVec3(0.0f, 0.0f, 0.0f);
	CDXVec3 vDiffuse = CDXVec3(1.0f, 1.0f, 1.0f);
	CDXVec3 vSpecular = CDXVec3(1.0f, 1.0f, 1.0f);
	int nShininess = 0;
	float fAlpha = 1.0f;

	pEffect->SetMatrix(m_shader->m_hWorldViewProjection, &mWorldViewProjection);
	pEffect->SetMatrix(m_shader->m_hWorld, &mWorld);
	pEffect->SetValue(m_shader->m_hCameraPosition, g_Camera.GetEyePt(), sizeof(CDXVec3));
	pEffect->SetValue(m_shader->m_hAmbient, vAmbient, sizeof(CDXVec3));
	pEffect->SetValue(m_shader->m_hDiffuse, vDiffuse, sizeof(CDXVec3));
	pEffect->SetValue(m_shader->m_hSpecular, vSpecular, sizeof(CDXVec3));
	pEffect->SetFloat(m_shader->m_hOpacity, fAlpha);
	pEffect->SetInt(m_shader->m_hSpecularPower, nShininess);

	pDevice->SetRenderState(D3DRS_ZENABLE,					D3DZB_TRUE);
	//pDevice->SetRenderState(D3DRS_FILLMODE,					D3DFILL_WIREFRAME);
	pDevice->SetRenderState(D3DRS_FILLMODE,					D3DFILL_SOLID);
	pDevice->SetRenderState(D3DRS_SHADEMODE,				D3DSHADE_GOURAUD);
	pDevice->SetRenderState(D3DRS_ZWRITEENABLE,				TRUE);
	pDevice->SetRenderState(D3DRS_ALPHATESTENABLE,			FALSE);
	pDevice->SetRenderState(D3DRS_LASTPIXEL,				TRUE);
	pDevice->SetRenderState(D3DRS_SRCBLEND,					D3DBLEND_ONE);
	pDevice->SetRenderState(D3DRS_DESTBLEND,				D3DBLEND_ZERO);
	pDevice->SetRenderState(D3DRS_CULLMODE,					D3DCULL_CCW);
	pDevice->SetRenderState(D3DRS_ZFUNC,					D3DCMP_LESSEQUAL);
	pDevice->SetRenderState(D3DRS_ALPHAREF,					0);
	pDevice->SetRenderState(D3DRS_ALPHAFUNC,				D3DCMP_ALWAYS);
	pDevice->SetRenderState(D3DRS_DITHERENABLE,				FALSE);
	pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,			FALSE);
	pDevice->SetRenderState(D3DRS_FOGENABLE,				FALSE);
	pDevice->SetRenderState(D3DRS_SPECULARENABLE,			FALSE);
	//pDevice->SetRenderState(D3DRS_FOGCOLOR,				0);
	//pDevice->SetRenderState(D3DRS_FOGTABLEMODE,			D3DFOG_NONE);
	//pDevice->SetRenderState(D3DRS_FOGSTART,				*((DWORD*) (&fFogStart)));
	//pDevice->SetRenderState(D3DRS_FOGEND,					*((DWORD*) (&fFogEnd));
	//pDevice->SetRenderState(D3DRS_FOGDENSITY,				*((DWORD*) (&fFogDensity));
	pDevice->SetRenderState(D3DRS_RANGEFOGENABLE,			FALSE);
	pDevice->SetRenderState(D3DRS_STENCILENABLE,			FALSE);
	pDevice->SetRenderState(D3DRS_STENCILFAIL,				D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_STENCILZFAIL,				D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_STENCILPASS,				D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_STENCILFUNC,				D3DCMP_ALWAYS);
	pDevice->SetRenderState(D3DRS_STENCILREF,				0);
	pDevice->SetRenderState(D3DRS_STENCILMASK,				0xFFFFFFFF);
	pDevice->SetRenderState(D3DRS_STENCILWRITEMASK,			0xFFFFFFFF);
	pDevice->SetRenderState(D3DRS_TEXTUREFACTOR,			0xFFFFFFFF);
	pDevice->SetRenderState(D3DRS_CLIPPING,					TRUE);
	pDevice->SetRenderState(D3DRS_LIGHTING,					TRUE);
	pDevice->SetRenderState(D3DRS_AMBIENT,					0);
	pDevice->SetRenderState(D3DRS_COLORVERTEX,				TRUE);
	pDevice->SetRenderState(D3DRS_LOCALVIEWER,				TRUE);
	pDevice->SetRenderState(D3DRS_NORMALIZENORMALS,			FALSE);
	pDevice->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE,	D3DMCS_COLOR1);
	pDevice->SetRenderState(D3DRS_SPECULARMATERIALSOURCE,	D3DMCS_COLOR2);
	pDevice->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE,	D3DMCS_MATERIAL);
	pDevice->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE,	D3DMCS_MATERIAL);
	pDevice->SetRenderState(D3DRS_VERTEXBLEND,				D3DVBF_DISABLE);
	pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE,			0);
	pDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,		TRUE);
	pDevice->SetRenderState(D3DRS_MULTISAMPLEMASK,			0xFFFFFFFF);
	pDevice->SetRenderState(D3DRS_PATCHEDGESTYLE,			D3DPATCHEDGE_DISCRETE);
	pDevice->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE,	FALSE);
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE,			0x0000000F);
	pDevice->SetRenderState(D3DRS_BLENDOP,					D3DBLENDOP_ADD);
	pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE,		FALSE);
	pDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS,		0);
	pDevice->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE,	FALSE);
	pDevice->SetRenderState(D3DRS_CCW_STENCILFAIL,			D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_CCW_STENCILZFAIL,			D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_CCW_STENCILPASS,			D3DSTENCILOP_KEEP);
	pDevice->SetRenderState(D3DRS_CCW_STENCILFUNC,			D3DCMP_ALWAYS);
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE1,		0x0000000f);
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE2,		0x0000000f);
	pDevice->SetRenderState(D3DRS_COLORWRITEENABLE3,		0x0000000f);
	pDevice->SetRenderState(D3DRS_BLENDFACTOR,				0xffffffff);
	pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE,			0);
	pDevice->SetRenderState(D3DRS_DEPTHBIAS,				0);
	pDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,	FALSE);
	pDevice->SetRenderState(D3DRS_SRCBLENDALPHA,			D3DBLEND_ONE);
	pDevice->SetRenderState(D3DRS_DESTBLENDALPHA,			D3DBLEND_ZERO);
	pDevice->SetRenderState(D3DRS_BLENDOPALPHA,				D3DBLENDOP_ADD);

	pDevice->SetVertexDeclaration( m_pMeshDecl );

	for(auto mesh : m_meshes) {
		if(mesh->IsVisible()) {
			mesh->Render(pDevice, m_shader);
		}
	}
}

void CDXScene::AddMesh(CDXMesh * mesh)
{
	if(mesh)
		m_meshes.push_back(mesh);
}

bool CDXScene::Pick(int x, int y, CDXPicker & picker)
{
	CDXRayInfo rayInfo;
	CDXRayInfo mRayInfo;
	CDXVec3 mousePoint;
	mousePoint.x = x;
	mousePoint.y = -y;
	mousePoint.z = 1.0f;

	const CDXMatrix* pmatProj = g_Camera.GetProjMatrix();

	// Compute the vector of the pick ray in screen space
	CDXVec3 v;
	v.x = (((2.0f * x) / g_Camera.GetWidth()) - 1) / pmatProj->_11;
	v.y = -(((2.0f * y) / g_Camera.GetHeight()) - 1) / pmatProj->_22;
	v.z = 1.0f;

	// Get the inverse view matrix
	const CDXMatrix matView = *g_Camera.GetViewMatrix();
	const CDXMatrix matWorld = *g_Camera.GetWorldMatrix();
	CDXMatrix mWorldView = matWorld * matView;
	CDXMatrix m;
	D3DXMatrixInverse( &m, NULL, &mWorldView );

	// Transform the screen space pick ray into 3D space
	rayInfo.direction.x = v.x * m._11 + v.y * m._21 + v.z * m._31;
	rayInfo.direction.y = v.x * m._12 + v.y * m._22 + v.z * m._32;
	rayInfo.direction.z = v.x * m._13 + v.y * m._23 + v.z * m._33;
	rayInfo.origin.x = m._41;
	rayInfo.origin.y = m._42;
	rayInfo.origin.z = m._43;
	rayInfo.point.x = mousePoint.x * m._11 + mousePoint.y * m._21 + mousePoint.z * m._31;
	rayInfo.point.y = mousePoint.x * m._12 + mousePoint.y * m._22 + mousePoint.z * m._32;
	rayInfo.point.z = mousePoint.x * m._13 + mousePoint.y * m._23 + mousePoint.z * m._33;

	// Create mirror raycast
	mRayInfo = rayInfo;
	mRayInfo.direction.x = -mRayInfo.direction.x;
	mRayInfo.origin.x = -mRayInfo.origin.x;

	// Find closest collision points
	bool hitMesh = false;
	CDXPickInfo pickInfo;
	CDXPickInfo mPickInfo;
	for (auto mesh : m_meshes)
	{
		if (mesh->IsLocked())
			continue;

		CDXPickInfo castInfo;
		if (mesh->Pick(rayInfo, castInfo))
			hitMesh = true;

		if ((!pickInfo.isHit && castInfo.isHit) || (castInfo.isHit && castInfo.dist < pickInfo.dist))
			pickInfo = castInfo;

		if (picker.Mirror()) {

			CDXPickInfo mCastInfo;
			if (mesh->Pick(mRayInfo, mCastInfo))
				hitMesh = true;

			if ((!mPickInfo.isHit && mCastInfo.isHit) || (mCastInfo.isHit && mCastInfo.dist < mPickInfo.dist))
				mPickInfo = mCastInfo;
		}
	}

	pickInfo.ray = rayInfo;
	mPickInfo.ray = mRayInfo;

	bool hitVertices = false;
	for (auto mesh : m_meshes)
	{
		if (mesh->IsLocked())
			continue;

		if (picker.Pick(pickInfo, mesh, false))
			hitVertices = true;

		if (picker.Mirror()) {
			if (picker.Pick(mPickInfo, mesh, true))
				hitVertices = true;
		}
	}

	return hitVertices;
}

#endif