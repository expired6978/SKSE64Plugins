#include "CDXCamera.h"

#ifdef FIXME

CDXArcBall::CDXArcBall()
{
	Reset();
	m_vDownPt = DirectX::XMVectorZero();
	m_vCurrentPt = DirectX::XMVectorZero();
	m_Offset.x = m_Offset.y = 0;
}

//--------------------------------------------------------------------------------------
void CDXArcBall::Reset()
{
	m_qDown = DirectX::XMQuaternionIdentity();
	m_qNow = DirectX::XMQuaternionIdentity();

	m_mRotation = DirectX::XMMatrixIdentity();
	m_mTranslation = DirectX::XMMatrixIdentity();
	m_mTranslationDelta = DirectX::XMMatrixIdentity();

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
	return DirectX::XMVectorSet(x, y, z, 1);
}

//--------------------------------------------------------------------------------------
CDXVec CDXArcBall::QuatFromBallPoints( const CDXVec& vFrom, const CDXVec& vTo )
{
	float fDot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(vFrom, vTo));
	CDXVec vPart = DirectX::XMVector3Cross(vFrom, vTo);
	return DirectX::XMVectorSet(DirectX::XMVectorGetX(vPart), DirectX::XMVectorGetY(vPart), DirectX::XMVectorGetZ(vPart), fDot );
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
		m_qNow = DirectX::XMVectorMultiply(m_qDown, QuatFromBallPoints( m_vDownPt, m_vCurrentPt ));
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

	m_mProj = DirectX::XMMatrixPerspectiveFovLH(fFOV, fAspect, fNearPlane, fFarPlane );
}
/*
void CDXModelViewerCamera::UpdateMouseDelta(int currentX, int currentY)
{
	POINT ptCurMouseDelta;
	POINT ptCurMousePos;
	ptCurMousePos.x = currentX;
	ptCurMousePos.y = currentY;

	// Calc how far it's moved since last frame
	ptCurMouseDelta.x = ptCurMousePos.x - m_ptLastMousePosition.x;
	ptCurMouseDelta.y = ptCurMousePos.y - m_ptLastMousePosition.y;

	// Record current position for next time
	m_ptLastMousePosition = ptCurMousePos;

	// Smooth the relative mouse data over a few frames so it isn't 
	// jerky when moving slowly at low frame rates.
	float fPercentOfNew = 1.0f / m_fFramesToSmoothMouseData;
	float fPercentOfOld = 1.0f - fPercentOfNew;
	m_vMouseDelta.x = m_vMouseDelta.x * fPercentOfOld + ptCurMouseDelta.x * fPercentOfNew;
	m_vMouseDelta.y = m_vMouseDelta.y * fPercentOfOld + ptCurMouseDelta.y * fPercentOfNew;

	m_vRotVelocity = m_vMouseDelta * m_fRotationScaler;
}
*/
//--------------------------------------------------------------------------------------
// Clamps pV to lie inside m_vMinBoundary & m_vMaxBoundary
//--------------------------------------------------------------------------------------
void CDXModelViewerCamera::ConstrainToBoundary( CDXVec* pV )
{
	// Constrain vector to a bounding box 
	*pV = DirectX::XMVectorClamp(*pV, m_vMinBoundary, m_vMaxBoundary);
}

//--------------------------------------------------------------------------------------
// Constructor 
//--------------------------------------------------------------------------------------
CDXModelViewerCamera::CDXModelViewerCamera()
{
	CDXVec vEyePt = DirectX::XMVectorZero();
	CDXVec vLookatPt = DirectX::XMVectorSet(0, 0, 1.0f, 0);

	// Setup the view matrix
	SetViewParams( &vEyePt, &vLookatPt );

	// Setup the projection matrix
	SetProjParams( DirectX::XM_PI / 4, 1.0f, 1.0f, 1000.0f );

	//m_ptLastMousePosition.x = 0;
	//m_ptLastMousePosition.y = 0;

	m_fCameraYawAngle = 0.0f;
	m_fCameraPitchAngle = 0.0f;

	//SetRect( &m_rcDrag, LONG_MIN, LONG_MIN, LONG_MAX, LONG_MAX );
	m_vVelocity = DirectX::XMVectorZero();
	//m_bMovementDrag = false;
	//m_vVelocityDrag = CDXVec( 0, 0, 0 );
	//m_fDragTimer = 0.0f;
	//m_fTotalDragTimeToZero = 0.25;
	//m_vRotVelocity = D3DXVECTOR2( 0, 0 );

	//m_fRotationScaler = 0.01f;
	//m_fMoveScaler = 5.0f;

	//m_vMouseDelta = D3DXVECTOR2( 0, 0 );
	//m_fFramesToSmoothMouseData = 2.0f;

	m_bClipToBoundary = false;
	m_vMinBoundary = DirectX::XMVectorSet(-1, -1, -1, 0);
	m_vMaxBoundary = DirectX::XMVectorSet(1, 1, 1, 0);

	m_mWorld = DirectX::XMMatrixIdentity();
	m_mModelRot = DirectX::XMMatrixIdentity();
	m_mModelLastRot = DirectX::XMMatrixIdentity();
	m_mCameraRotLast = DirectX::XMMatrixIdentity();
	m_vModelCenter = DirectX::XMVectorZero();
	m_fRadius = 5.0f;
	m_fDefaultRadius = 5.0f;
	m_fMinRadius = 1.0f;
	m_fPanMultiplier = -0.05f;
	m_fMaxRadius = FLT_MAX;
	m_bDragSinceLastUpdate = true;
}

void CDXModelViewerCamera::OnRotate(int currentX, int currentY)
{
	//UpdateMouseDelta(currentX, currentY);
	m_WorldArcBall.OnRotate(currentX, currentY);
	m_bDragSinceLastUpdate = true;
	Update();
}

void CDXModelViewerCamera::OnMoveBegin(int currentX, int currentY)
{
	m_vDragLook = m_vLookAt;
	m_vDragStart = DirectX::XMVectorSet(currentX, currentY, 0, 0);
	m_vDragPos = m_vDragStart;
}

void CDXModelViewerCamera::OnMove(int x, int y)
{
	m_vDragPos = DirectX::XMVectorSet(x, y, 0, 0);

	m_vLookAt = m_vDragLook;

	auto multiplier = DirectX::XMVectorMultiply(DirectX::XMVectorReplicate(m_fPanMultiplier), DirectX::XMVectorSet(-1, 1, 1, 1));

	auto panDelta = DirectX::XMVectorMultiply(DirectX::XMVectorSubtract(m_vDragPos, m_vDragStart), multiplier);

	m_vLookAt = DirectX::XMVectorAdd(m_vLookAt, panDelta);

	m_bDragSinceLastUpdate = true;
	Update();
}

void CDXModelViewerCamera::OnMoveEnd()
{
	m_vDragLook = m_vLookAt;
	m_vDragStart = DirectX::XMVectorZero();
	m_vDragPos = DirectX::XMVectorZero();
}

void CDXModelViewerCamera::Update()
{
	// If no dragged has happend since last time FrameMove is called,
	// and no camera key is held down, then no need to handle again.
	if( !m_bDragSinceLastUpdate )
		return;
	m_bDragSinceLastUpdate = false;

	// Simple euler method to calculate position delta
	//CDXVec vPosDelta = m_vVelocity;

	// Change the radius from the camera to the model based on wheel scrolling
	m_fRadius = __min( m_fMaxRadius, m_fRadius );
	m_fRadius = __max( m_fMinRadius, m_fRadius );

	// Get the inverse of the arcball's rotation matrix

	auto det = DirectX::XMMatrixDeterminant(m_ViewArcBall.GetRotationMatrix());
	auto mCameraRot = DirectX::XMMatrixInverse(&det, m_ViewArcBall.GetRotationMatrix());

	//D3DXMATRIX mCameraRot;
	//D3DXMatrixRotationYawPitchRoll(&mCameraRot, m_fCameraYawAngle, m_fCameraPitchAngle, 0);

	// Transform vectors based on camera's rotation matrix
	CDXVec vWorldUp, vWorldAhead;
	CDXVec vLocalUp = DirectX::XMVectorSet( 0, 1, 0, 0 );
	CDXVec vLocalAhead = DirectX::XMVectorSet(0, 0, 1, 0 );
	vWorldUp = XMVector3TransformCoord(vLocalUp, mCameraRot );
	vWorldAhead = XMVector3TransformCoord(vLocalAhead, mCameraRot );

	// Transform the position delta by the camera's rotation 
	/*CDXVec vPosDeltaWorld;
	D3DXVec3TransformCoord( &vPosDeltaWorld, &vPosDelta, &mCameraRot );

	// Move the lookAt position 
	m_vLookAt += vPosDeltaWorld;
	if( m_bClipToBoundary )
		ConstrainToBoundary( &m_vLookAt );*/

	/*m_vEye += vPosDeltaWorld;
	if (m_bClipToBoundary)
		ConstrainToBoundary(&m_vEye);*/

	// Update the eye point based on a radius away from the lookAt position
	m_vEye = DirectX::XMVectorScale(DirectX::XMVectorSubtract(m_vLookAt, vWorldAhead), m_fRadius);
	//m_vLookAt = m_vEye + vWorldAhead * m_fRadius;

	// Update the view matrix
	m_mView = DirectX::XMMatrixLookAtLH( m_vEye, m_vLookAt, vWorldUp );

	auto mInvViewDet = DirectX::XMMatrixDeterminant(m_mView);
	auto mInvView = DirectX::XMMatrixInverse(&mInvViewDet, m_mView);

	mInvView.r[3] = DirectX::XMVectorZero();

	auto mModelLastRotInvDet = DirectX::XMMatrixDeterminant(m_mModelLastRot);
	auto mModelLastRotInv = DirectX::XMMatrixInverse(&mModelLastRotInvDet, m_mModelLastRot);

	// Accumulate the delta of the arcball's rotation in view space.
	// Note that per-frame delta rotations could be problematic over long periods of time.
	auto mModelRot = m_WorldArcBall.GetRotationMatrix();
	m_mModelRot *= m_mView * mModelLastRotInv * mModelRot * mInvView;

	m_mCameraRotLast = mCameraRot;
	m_mModelLastRot = mModelRot;

	// Since we're accumulating delta rotations, we need to orthonormalize 
	// the matrix to prevent eventual matrix skew
	m_mModelRot.r[0] = DirectX::XMVector3Normalize(m_mModelRot.r[0]);
	m_mModelRot.r[1] = DirectX::XMVector3Cross(m_mModelRot.r[2], m_mModelRot.r[0]);
	m_mModelRot.r[1] = DirectX::XMVector3Normalize(m_mModelRot.r[1]);
	m_mModelRot.r[2] = DirectX::XMVector3Cross(m_mModelRot.r[0], m_mModelRot.r[1]);

	m_mModelRot.r[3] = m_vModelCenter;

	// Translate world matrix so its at the center of the model
	//D3DXMatrixTranslation( &mTrans, -m_vModelCenter.x, -m_vModelCenter.y, -m_vModelCenter.z );
	m_mWorld = DirectX::XMMatrixScaling(-1.0, 1.0, 1.0) * m_mModelRot;
}

//--------------------------------------------------------------------------------------
// Reset the camera's position back to the default
//--------------------------------------------------------------------------------------
VOID CDXModelViewerCamera::Reset()
{
	SetViewParams( &m_vDefaultEye, &m_vDefaultLookAt );

	m_mWorld = DirectX::XMMatrixIdentity();
	m_mModelRot = DirectX::XMMatrixIdentity();
	m_mModelLastRot = DirectX::XMMatrixIdentity();
	m_mCameraRotLast = DirectX::XMMatrixIdentity();

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
	CDXVec vUp = DirectX::XMVectorSet( 0,1,0,1 );
	m_mView = DirectX::XMMatrixLookAtLH( *pvEyePt, *pvLookatPt, vUp );

	auto m_mViewDet = DirectX::XMMatrixDeterminant(m_mView);
	auto mInvView = DirectX::XMMatrixInverse(&m_mViewDet, m_mView);

	// The axis basis vectors and camera position are stored inside the 
	// position matrix in the 4 rows of the camera's world matrix.
	// To figure out the yaw/pitch of the camera, we just need the Z basis vector
	CDXVec* pZBasis = ( CDXVec* )&mInvView.r[3];

	m_fCameraYawAngle = atan2f(DirectX::XMVectorGetX(*pZBasis), DirectX::XMVectorGetZ(*pZBasis));
	float fLen = sqrtf(DirectX::XMVectorGetZ(*pZBasis) * DirectX::XMVectorGetZ(*pZBasis) + DirectX::XMVectorGetX(*pZBasis) * DirectX::XMVectorGetX(*pZBasis));
	m_fCameraPitchAngle = -atan2f(DirectX::XMVectorGetY(*pZBasis), fLen );

	// Propogate changes to the member arcball
	auto mRotation = DirectX::XMMatrixLookAtLH(*pvEyePt, *pvLookatPt, vUp);
	mRotation = DirectX::XMMatrixRotationRollPitchYaw(-DirectX::XM_PI / 2, 0, 0);

	auto quat = DirectX::XMQuaternionRotationMatrix(mRotation);
	m_ViewArcBall.SetQuatNow( quat );

	// Set the radius according to the distance
	CDXVec vEyeToPoint = DirectX::XMVectorSubtract(*pvLookatPt, *pvEyePt);
	SetRadius( DirectX::XMVectorGetX(DirectX::XMVector3Length(vEyeToPoint)));

	// View information changed. FrameMove should be called.
	m_bDragSinceLastUpdate = true;
}
#endif