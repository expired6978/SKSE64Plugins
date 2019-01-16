#include "CDXCamera.h"

using namespace DirectX;

CDXArcBall::CDXArcBall()
{
	Reset();
	m_vDownPt = XMVectorZero();
	m_vCurrentPt = XMVectorZero();
	m_Offset.x = m_Offset.y = 0;
}

//--------------------------------------------------------------------------------------
void CDXArcBall::Reset()
{
	m_qDown = XMQuaternionIdentity();
	m_qNow = XMQuaternionIdentity();

	m_mRotation = XMMatrixIdentity();
	m_mTranslation = XMMatrixIdentity();
	m_mTranslationDelta = XMMatrixIdentity();

	m_bDrag = FALSE;
	m_fRadiusTranslation = 1.0f;
	m_fRadius = 1.0f;
}

//--------------------------------------------------------------------------------------
CDXVec CDXArcBall::ScreenToVector( float fScreenPtX, float fScreenPtY )
{
	// Scale to screen
	FLOAT x = -( fScreenPtX - m_Offset.x - m_nWidth / 2 ) / ( m_fRadius * m_nWidth / 2 );
	FLOAT y = ( fScreenPtY - m_Offset.y - m_nHeight / 2 ) / ( m_fRadius * m_nHeight / 2 );

	FLOAT z = 0.0f;
	FLOAT mag = x * x + y * y;

	if( mag > 1.0f )
	{
		FLOAT scale = 1.0f / sqrtf( mag );
		x *= scale;
		y *= scale;
	}
	else
		z = sqrtf( 1.0f - mag );

	// Return vector
	return XMVectorSet(x, y, z, 1);
}

//--------------------------------------------------------------------------------------
CDXVec CDXArcBall::QuatFromBallPoints( const CDXVec& vFrom, const CDXVec& vTo )
{
	float fDot = XMVectorGetX(XMVector3Dot(vFrom, vTo));
	CDXVec vPart = XMVector3Cross(vFrom, vTo);
	return XMVectorSet(XMVectorGetX(vPart), XMVectorGetY(vPart), XMVectorGetZ(vPart), fDot );
}

//--------------------------------------------------------------------------------------
void CDXArcBall::OnRotateBegin( int nX, int nY )
{
	// Only enter the drag state if the click falls
	// inside the click rectangle.
	if( nX >= m_Offset.x &&
		nX < m_Offset.x + m_nWidth &&
		nY >= m_Offset.y &&
		nY < m_Offset.y + m_nHeight )
	{
		m_bDrag = true;
		m_qDown = m_qNow;
		m_vDownPt = ScreenToVector( ( float )nX, ( float )nY );
	}
}

//--------------------------------------------------------------------------------------
void CDXArcBall::OnRotate( int nX, int nY )
{
	if( m_bDrag )
	{
		m_vCurrentPt = ScreenToVector( ( float )nX, ( float )nY );
		m_qNow = XMQuaternionMultiply(m_qDown, QuatFromBallPoints( m_vDownPt, m_vCurrentPt ));
	}
}

//--------------------------------------------------------------------------------------
void CDXArcBall::OnRotateEnd()
{
	m_bDrag = false;
}


//--------------------------------------------------------------------------------------
// Calculates the projection matrix based on input params
//--------------------------------------------------------------------------------------
VOID CDXModelViewerCamera::SetProjParams( FLOAT fFOV, FLOAT fAspect, FLOAT fNearPlane,
								FLOAT fFarPlane )
{
	// Set attributes for the projection matrix
	m_fFOV = fFOV;
	m_fAspect = fAspect;
	m_fNearPlane = fNearPlane;
	m_fFarPlane = fFarPlane;

	m_mProj = XMMatrixPerspectiveFovLH(fFOV, fAspect, fNearPlane, fFarPlane );
}

//--------------------------------------------------------------------------------------
// Constructor 
//--------------------------------------------------------------------------------------
CDXModelViewerCamera::CDXModelViewerCamera()
{
	CDXVec vEyePt = XMVectorZero();
	CDXVec vLookatPt = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	// Setup the view matrix
	SetViewParams( &vEyePt, &vLookatPt );

	// Setup the projection matrix
	SetProjParams(60 * (XM_PI / 180.0f), 1.0f, 1.0f, 1000.0f);

	m_vMinBoundary = XMVectorSet(-1, -1, -1, 1);
	m_vMaxBoundary = XMVectorSet(1, 1, 1, 1);

	m_mWorld = XMMatrixIdentity();
	m_mModelRot = XMMatrixIdentity();
	m_mModelLastRot = XMMatrixIdentity();

	m_vModelCenter = XMVectorSet(0, 0, 0, 1);
	m_fRadius = 5.0f;
	m_fDefaultRadius = 5.0f;
	m_fMinRadius = 1.0f;
	m_fPanMultiplier = -0.05f;
	m_fMaxRadius = FLT_MAX;
	m_bDragSinceLastUpdate = true;
}

void CDXModelViewerCamera::OnRotate(int currentX, int currentY)
{
	m_WorldArcBall.OnRotate(currentX, currentY);
	m_bDragSinceLastUpdate = true;
	Update();
}

void CDXModelViewerCamera::OnMoveBegin(int currentX, int currentY)
{
	m_vDragLook = m_vLookAt;
	m_vDragStart = XMVectorSet((float)currentX, 0, (float)currentY, 0);
	m_vDragPos = m_vDragStart;
}

void CDXModelViewerCamera::OnMove(int x, int y)
{
	m_vDragPos = XMVectorSet((float)x, 0, (float)y, 0);

	m_vLookAt = m_vDragLook;

	auto multiplier = XMVectorSetW(XMVectorReplicate(m_fPanMultiplier), 0.0f);
	multiplier = XMVectorSetZ(multiplier, -XMVectorGetZ(multiplier));

	auto panDelta = XMVectorMultiply(XMVectorSubtract(m_vDragPos, m_vDragStart), multiplier);

	m_vLookAt = XMVectorAdd(m_vLookAt, panDelta);

	m_bDragSinceLastUpdate = true;
	Update();
}

void CDXModelViewerCamera::OnMoveEnd()
{
	m_vDragLook = m_vLookAt;
	m_vDragStart = XMVectorSet(0, 0, 0, 1);
	m_vDragPos = XMVectorSet(0, 0, 0, 1);
}

void CDXModelViewerCamera::Update()
{
	// If no dragged has happend since last time FrameMove is called,
	// and no camera key is held down, then no need to handle again.
	if( !m_bDragSinceLastUpdate )
		return;
	m_bDragSinceLastUpdate = false;

	// Change the radius from the camera to the model based on wheel scrolling
	m_fRadius = __min( m_fMaxRadius, m_fRadius );
	m_fRadius = __max( m_fMinRadius, m_fRadius );

	auto mCameraRot = XMMatrixInverse(nullptr, m_ViewArcBall.GetRotationMatrix());

	// Transform vectors based on camera's rotation matrix
	CDXVec vWorldUp, vWorldAhead;
	CDXVec vLocalUp = XMVectorSet( 0, 1, 0, 1 );
	CDXVec vLocalAhead = XMVectorSet(0, 0, 1, 1 );
	vWorldUp = XMVector3TransformCoord(vLocalUp, mCameraRot );
	vWorldAhead = XMVector3TransformCoord(vLocalAhead, mCameraRot );

	// Update the eye point based on a radius away from the lookAt position
	m_vEye = XMVectorSubtract(m_vLookAt, XMVectorScale(vWorldAhead, m_fRadius));

	// Update the view matrix
	m_mView = XMMatrixLookAtLH( m_vEye, m_vLookAt, vWorldUp );

	auto mInvView = XMMatrixInverse(nullptr, m_mView);

	mInvView.r[3] = XMVectorZero();

	auto mModelLastRotInv = XMMatrixInverse(nullptr, m_mModelLastRot);
	
	auto mModelRot = m_WorldArcBall.GetRotationMatrix();

	m_mModelRot *= m_mView * mModelLastRotInv * mModelRot * mInvView;

	m_mModelLastRot = mModelRot;

	// Since we're accumulating delta rotations, we need to orthonormalize 
	// the matrix to prevent eventual matrix skew
	m_mModelRot.r[0] = XMVector3Normalize(m_mModelRot.r[0]);
	m_mModelRot.r[1] = XMVector3Cross(m_mModelRot.r[2], m_mModelRot.r[0]);
	m_mModelRot.r[1] = XMVector3Normalize(m_mModelRot.r[1]);
	m_mModelRot.r[2] = XMVector3Cross(m_mModelRot.r[0], m_mModelRot.r[1]);

	m_mModelRot.r[3] = m_vModelCenter;

	m_mWorld = m_mModelRot;
}

//--------------------------------------------------------------------------------------
// Reset the camera's position back to the default
//--------------------------------------------------------------------------------------
VOID CDXModelViewerCamera::Reset()
{
	SetViewParams( &m_vDefaultEye, &m_vDefaultLookAt );

	m_mWorld = XMMatrixIdentity();
	m_mModelRot = XMMatrixIdentity();
	m_mModelLastRot = XMMatrixIdentity();

	m_fRadius = m_fDefaultRadius;
	m_WorldArcBall.Reset();
	m_ViewArcBall.Reset();
}


//--------------------------------------------------------------------------------------
// Override for setting the view parameters
//--------------------------------------------------------------------------------------
void CDXModelViewerCamera::SetViewParams( CDXVec* pvEyePt, CDXVec* pvLookatPt )
{
	if( NULL == pvEyePt || NULL == pvLookatPt )
		return;

	m_vDefaultEye = m_vEye = *pvEyePt;
	m_vDefaultLookAt = m_vLookAt = *pvLookatPt;

	// Calc the view matrix
	CDXVec vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
	m_mView = XMMatrixLookAtLH( *pvEyePt, *pvLookatPt, vUp );

	auto mInvView = XMMatrixInverse(nullptr, m_mView);

	// The axis basis vectors and camera position are stored inside the 
	// position matrix in the 4 rows of the camera's world matrix.
	// To figure out the yaw/pitch of the camera, we just need the Z basis vector

	// Propogate changes to the member arcball
	auto mRotation = XMMatrixLookAtLH(*pvEyePt, *pvLookatPt, vUp);
	mRotation = XMMatrixRotationRollPitchYaw(-XM_PI / 2.0f, 0, 0);

	auto quat = XMQuaternionRotationMatrix(mRotation);
	m_ViewArcBall.SetQuatNow( quat );

	// Set the radius according to the distance
	CDXVec vEyeToPoint = XMVectorSubtract(*pvLookatPt, *pvEyePt);
	SetRadius( XMVectorGetX(XMVector3Length(vEyeToPoint)));

	// View information changed. FrameMove should be called.
	m_bDragSinceLastUpdate = true;
}
