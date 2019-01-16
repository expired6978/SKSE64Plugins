#include "CDXScene.h"
#include "CDXCamera.h"
#include "CDXMesh.h"
#include "CDXShader.h"
#include "CDXMaterial.h"
#include "CDXPicker.h"
#include "CDXBrush.h"

using namespace DirectX;

CDXScene::CDXScene()
{
	m_shader = new CDXShader;
	m_visible = true;
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
	if(m_shader) {
		m_shader->Release();
	}

	for(auto it : m_meshes) {
		it->Release();
		delete it;
	}

	m_meshes.clear();
}

bool CDXScene::Setup(const CDXInitParams & initParams)
{
	if (!m_shader->Initialize(initParams))
	{
		return false;
	}

	return true;
}

void CDXScene::Render(CDXCamera * camera, CDXD3DDevice * device)
{
	CDXMatrix mWorld;
	CDXMatrix mView;
	CDXMatrix mProj;

	mWorld = *camera->GetWorldMatrix();
	mView = camera->GetViewMatrix();
	mProj = camera->GetProjMatrix();

	// Setup the vector that points upwards.
	/*XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	float posX = 0.0f, posY = 32.0f, posZ = -8.0f;

	// Setup the position of the camera in the world.
	XMVECTOR position = XMVectorSet(posX, posY, posZ, 0.0f);

	// Calculate the rotation in radians.
	float radians = 0.0f * 0.0174532925f;

	// Setup where the camera is looking.
	XMVECTOR lookAt = XMVectorSet(0, 0, 0, 0.0f);

	// Create the view matrix from the three vectors.
	mView = XMMatrixLookAtLH(position, lookAt, up);

	float fieldOfView = (float)XM_PI / 4.0f;
	float screenAspect = (float)1280 / (float)720;

	const float SCREEN_DEPTH = 1000.0f;
	const float SCREEN_NEAR = 0.1f;

	// Create the projection matrix for 3D rendering.
	mProj = XMMatrixPerspectiveFovLH(fieldOfView, screenAspect, SCREEN_NEAR, SCREEN_DEPTH);

	// Initialize the world matrix to the identity matrix.
	mWorld = XMMatrixIdentity();*/

	// Create an orthographic projection matrix for 2D rendering.
	//m_orthoMatrix = XMMatrixOrthographicLH((float)screenWidth, (float)screenHeight, screenNear, screenDepth);
	

	CDXShader::VertexBuffer params;
	params.world = mWorld;
	params.view = mView;
	params.projection = mProj;
	params.viewport = XMVectorSet((float)camera->GetWidth(), (float)camera->GetHeight(), 0, 0);

	m_shader->VSSetShaderBuffer(device, params);

	CDXShader::TransformBuffer xform;
	xform.transform = XMMatrixIdentity();

	for(auto mesh : m_meshes) {
		xform.transform = mesh->GetTransform();

		m_shader->VSSetTransformBuffer(device, xform);

		if(mesh->IsVisible()) {
			mesh->Render(device, m_shader);
		}
	}
}

void CDXScene::AddMesh(CDXMesh * mesh)
{
	if(mesh)
		m_meshes.push_back(mesh);
}

bool CDXScene::Pick(CDXCamera * camera, int x, int y, CDXPicker & picker)
{
	CDXRayInfo rayInfo;
	CDXRayInfo mRayInfo;
	CDXVec mousePoint = XMVectorSet((float)x, (float)-y, 1.0f, 0.0f);

	CDXMatrix pmatProj = camera->GetProjMatrix();

	// Compute the vector of the pick ray in screen space
	CDXVec v = XMVectorSet(
		(((2.0f * x) / camera->GetWidth()) - 1) / XMVectorGetX(pmatProj.r[0]),
		-(((2.0f * y) / camera->GetHeight()) - 1) / XMVectorGetY(pmatProj.r[1]),
		1.0f, 
		1.0f
	);

	CDXVec v2 = XMVectorSet((float)x, (float)y,1.0f,0.0f);
	auto vRes = XMVector3Unproject(v2, 0, 0, camera->GetWidth(), camera->GetHeight(), 0.0f, 1.0f, camera->GetProjMatrix(), camera->GetViewMatrix(), *camera->GetWorldMatrix());
	auto dir = XMVector3Normalize(vRes);

	// Get the inverse view matrix
	CDXMatrix matView = camera->GetViewMatrix();
	const CDXMatrix * matWorld = camera->GetWorldMatrix();
	CDXMatrix mWorldView = XMMatrixMultiply(*matWorld, matView);

	CDXMatrix m = XMMatrixInverse(nullptr, mWorldView);

	CDXMatrix mRot = m;
	mRot.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

	//auto rot = XMQuaternionRotationMatrix(m);

	// Transform the screen space pick ray into 3D space
	rayInfo.direction = XMVector3Transform(v, mRot);
	rayInfo.origin = XMVectorSetW(m.r[3], 0.0f);
	rayInfo.point = XMVector3Transform(mousePoint, mRot);

	// Create mirror raycast
	mRayInfo = rayInfo;
	mRayInfo.direction = XMVectorSetX(mRayInfo.direction, -XMVectorGetX(mRayInfo.direction));
	mRayInfo.origin = XMVectorSetX(mRayInfo.origin, -XMVectorGetX(mRayInfo.origin));

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
