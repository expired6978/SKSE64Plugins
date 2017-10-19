#include "CDXCamera.h"

#ifdef FIXME

CDXArcBall::CDXArcBall()
{
	Reset();
	m_vDownPt = D3DXVECTOR3( 0, 0, 0 );
	m_vCurrentPt = D3DXVECTOR3( 0, 0, 0 );
	m_Offset.x = m_Offset.y = 0;
}

//--------------------------------------------------------------------------------------
void CDXArcBall::Reset()
{
	D3DXQuaternionIdentity( &m_qDown );
	D3DXQuaternionIdentity( &m_qNow );
	D3DXMatrixIdentity( &m_mRotation );
	D3DXMatrixIdentity( &m_mTranslation );
	D3DXMatrixIdentity( &m_mTranslationDelta );
	m_bDrag = FALSE;
	m_fRadiusTranslation = 1.0f;
	m_fRadius = 1.0f;
}

//--------------------------------------------------------------------------------------
D3DXVECTOR3 CDXArcBall::ScreenToVector( float fScreenPtX, float fScreenPtY )
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
	return D3DXVECTOR3( x, y, z );
}

//--------------------------------------------------------------------------------------
D3DXQUATERNION CDXArcBall::QuatFromBallPoints( const D3DXVECTOR3& vFrom, const D3DXVECTOR3& vTo )
{
	D3DXVECTOR3 vPart;
	float fDot = D3DXVec3Dot( &vFrom, &vTo );
	D3DXVec3Cross( &vPart, &vFrom, &vTo );

	return D3DXQUATERNION( vPart.x, vPart.y, vPart.z, fDot );
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
		m_qNow = m_qDown * QuatFromBallPoints( m_vDownPt, m_vCurrentPt );
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

	D3DXMatrixPerspectiveFovLH( &m_mProj, fFOV, fAspect, fNearPlane, fFarPlane );
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
void CDXModelViewerCamera::ConstrainToBoundary( D3DXVECTOR3* pV )
{
	// Constrain vector to a bounding box 
	pV->x = __max( pV->x, m_vMinBoundary.x );
	pV->y = __max( pV->y, m_vMinBoundary.y );
	pV->z = __max( pV->z, m_vMinBoundary.z );

	pV->x = __min( pV->x, m_vMaxBoundary.x );
	pV->y = __min( pV->y, m_vMaxBoundary.y );
	pV->z = __min( pV->z, m_vMaxBoundary.z );
}

//--------------------------------------------------------------------------------------
// Constructor 
//--------------------------------------------------------------------------------------
CDXModelViewerCamera::CDXModelViewerCamera()
{
	D3DXVECTOR3 vEyePt = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
	D3DXVECTOR3 vLookatPt = D3DXVECTOR3( 0.0f, 0.0f, 1.0f );

	// Setup the view matrix
	SetViewParams( &vEyePt, &vLookatPt );

	// Setup the projection matrix
	SetProjParams( D3DX_PI / 4, 1.0f, 1.0f, 1000.0f );

	//m_ptLastMousePosition.x = 0;
	//m_ptLastMousePosition.y = 0;

	m_fCameraYawAngle = 0.0f;
	m_fCameraPitchAngle = 0.0f;

	//SetRect( &m_rcDrag, LONG_MIN, LONG_MIN, LONG_MAX, LONG_MAX );
	m_vVelocity = D3DXVECTOR3( 0, 0, 0 );
	//m_bMovementDrag = false;
	//m_vVelocityDrag = D3DXVECTOR3( 0, 0, 0 );
	//m_fDragTimer = 0.0f;
	//m_fTotalDragTimeToZero = 0.25;
	//m_vRotVelocity = D3DXVECTOR2( 0, 0 );

	//m_fRotationScaler = 0.01f;
	//m_fMoveScaler = 5.0f;

	//m_vMouseDelta = D3DXVECTOR2( 0, 0 );
	//m_fFramesToSmoothMouseData = 2.0f;

	m_bClipToBoundary = false;
	m_vMinBoundary = D3DXVECTOR3( -1, -1, -1 );
	m_vMaxBoundary = D3DXVECTOR3( 1, 1, 1 );

	D3DXMatrixIdentity( &m_mWorld );
	D3DXMatrixIdentity( &m_mModelRot );
	D3DXMatrixIdentity( &m_mModelLastRot );
	D3DXMatrixIdentity( &m_mCameraRotLast );
	m_vModelCenter = D3DXVECTOR3( 0, 0, 0 );
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
	m_vDragStart = D3DXVECTOR2(currentX, currentY);
	m_vDragPos = m_vDragStart;
}

void CDXModelViewerCamera::OnMove(int x, int y)
{
	m_vDragPos = D3DXVECTOR2(x, y);

	m_vLookAt = m_vDragLook;
	
	D3DXVECTOR2 panDelta = (m_vDragPos - m_vDragStart) * m_fPanMultiplier;

	m_vLookAt.x += -panDelta.x;
	m_vLookAt.z += panDelta.y;

	m_bDragSinceLastUpdate = true;
	Update();
}

void CDXModelViewerCamera::OnMoveEnd()
{
	m_vDragLook = m_vLookAt;
	m_vDragStart = D3DXVECTOR2(0, 0);
	m_vDragPos = D3DXVECTOR2(0, 0);
}

void CDXModelViewerCamera::Update()
{
	// If no dragged has happend since last time FrameMove is called,
	// and no camera key is held down, then no need to handle again.
	if( !m_bDragSinceLastUpdate )
		return;
	m_bDragSinceLastUpdate = false;

	// Simple euler method to calculate position delta
	//D3DXVECTOR3 vPosDelta = m_vVelocity;

	// Change the radius from the camera to the model based on wheel scrolling
	m_fRadius = __min( m_fMaxRadius, m_fRadius );
	m_fRadius = __max( m_fMinRadius, m_fRadius );

	// Get the inverse of the arcball's rotation matrix
	D3DXMATRIX mCameraRot;
	D3DXMatrixInverse( &mCameraRot, NULL, m_ViewArcBall.GetRotationMatrix() );

	//D3DXMATRIX mCameraRot;
	//D3DXMatrixRotationYawPitchRoll(&mCameraRot, m_fCameraYawAngle, m_fCameraPitchAngle, 0);

	// Transform vectors based on camera's rotation matrix
	D3DXVECTOR3 vWorldUp, vWorldAhead;
	D3DXVECTOR3 vLocalUp = D3DXVECTOR3( 0, 1, 0 );
	D3DXVECTOR3 vLocalAhead = D3DXVECTOR3( 0, 0, 1 );
	D3DXVec3TransformCoord( &vWorldUp, &vLocalUp, &mCameraRot );
	D3DXVec3TransformCoord( &vWorldAhead, &vLocalAhead, &mCameraRot );

	// Transform the position delta by the camera's rotation 
	/*D3DXVECTOR3 vPosDeltaWorld;
	D3DXVec3TransformCoord( &vPosDeltaWorld, &vPosDelta, &mCameraRot );

	// Move the lookAt position 
	m_vLookAt += vPosDeltaWorld;
	if( m_bClipToBoundary )
		ConstrainToBoundary( &m_vLookAt );*/

	/*m_vEye += vPosDeltaWorld;
	if (m_bClipToBoundary)
		ConstrainToBoundary(&m_vEye);*/

	// Update the eye point based on a radius away from the lookAt position
	m_vEye = m_vLookAt - vWorldAhead * m_fRadius;
	//m_vLookAt = m_vEye + vWorldAhead * m_fRadius;

	// Update the view matrix
	D3DXMatrixLookAtLH( &m_mView, &m_vEye, &m_vLookAt, &vWorldUp );

	D3DXMATRIX mInvView;
	D3DXMatrixInverse( &mInvView, NULL, &m_mView );
	mInvView._41 = mInvView._42 = mInvView._43 = 0;

	D3DXMATRIX mModelLastRotInv;
	D3DXMatrixInverse( &mModelLastRotInv, NULL, &m_mModelLastRot );

	// Accumulate the delta of the arcball's rotation in view space.
	// Note that per-frame delta rotations could be problematic over long periods of time.
	D3DXMATRIX mModelRot;
	mModelRot = *m_WorldArcBall.GetRotationMatrix();
	m_mModelRot *= m_mView * mModelLastRotInv * mModelRot * mInvView;

	m_mCameraRotLast = mCameraRot;
	m_mModelLastRot = mModelRot;

	// Since we're accumulating delta rotations, we need to orthonormalize 
	// the matrix to prevent eventual matrix skew
	D3DXVECTOR3* pXBasis = ( D3DXVECTOR3* )&m_mModelRot._11;
	D3DXVECTOR3* pYBasis = ( D3DXVECTOR3* )&m_mModelRot._21;
	D3DXVECTOR3* pZBasis = ( D3DXVECTOR3* )&m_mModelRot._31;
	D3DXVec3Normalize( pXBasis, pXBasis );
	D3DXVec3Cross( pYBasis, pZBasis, pXBasis );
	D3DXVec3Normalize( pYBasis, pYBasis );
	D3DXVec3Cross( pZBasis, pXBasis, pYBasis );

	// Translate the rotation matrix to the same position as the lookAt position
	//m_mModelRot._41 = m_vLookAt.x;
	//m_mModelRot._42 = m_vLookAt.y;
	//m_mModelRot._43 = m_vLookAt.z;

	m_mModelRot._41 = m_vModelCenter.x;
	m_mModelRot._42 = m_vModelCenter.y;
	m_mModelRot._43 = m_vModelCenter.z;

	// Translate world matrix so its at the center of the model
	D3DXMATRIX mTrans;
	//D3DXMatrixTranslation( &mTrans, -m_vModelCenter.x, -m_vModelCenter.y, -m_vModelCenter.z );
	D3DXMatrixScaling(&mTrans, -1.0, 1.0, 1.0);
	m_mWorld = mTrans * m_mModelRot;
}

//--------------------------------------------------------------------------------------
// Reset the camera's position back to the default
//--------------------------------------------------------------------------------------
VOID CDXModelViewerCamera::Reset()
{
	SetViewParams( &m_vDefaultEye, &m_vDefaultLookAt );

	D3DXMatrixIdentity( &m_mWorld );
	D3DXMatrixIdentity( &m_mModelRot );
	D3DXMatrixIdentity( &m_mModelLastRot );
	D3DXMatrixIdentity( &m_mCameraRotLast );

	m_fRadius = m_fDefaultRadius;
	m_WorldArcBall.Reset();
	m_ViewArcBall.Reset();
}


//--------------------------------------------------------------------------------------
// Override for setting the view parameters
//--------------------------------------------------------------------------------------
void CDXModelViewerCamera::SetViewParams( D3DXVECTOR3* pvEyePt, D3DXVECTOR3* pvLookatPt )
{
	if( NULL == pvEyePt || NULL == pvLookatPt )
		return;

	m_vDefaultEye = m_vEye = *pvEyePt;
	m_vDefaultLookAt = m_vLookAt = *pvLookatPt;

	// Calc the view matrix
	D3DXVECTOR3 vUp( 0,1,0 );
	D3DXMatrixLookAtLH( &m_mView, pvEyePt, pvLookatPt, &vUp );

	D3DXMATRIX mInvView;
	D3DXMatrixInverse( &mInvView, NULL, &m_mView );

	// The axis basis vectors and camera position are stored inside the 
	// position matrix in the 4 rows of the camera's world matrix.
	// To figure out the yaw/pitch of the camera, we just need the Z basis vector
	D3DXVECTOR3* pZBasis = ( D3DXVECTOR3* )&mInvView._31;

	m_fCameraYawAngle = atan2f( pZBasis->x, pZBasis->z );
	float fLen = sqrtf( pZBasis->z * pZBasis->z + pZBasis->x * pZBasis->x );
	m_fCameraPitchAngle = -atan2f( pZBasis->y, fLen );

	// Propogate changes to the member arcball
	D3DXQUATERNION quat;
	D3DXMATRIXA16 mRotation;
	D3DXMatrixLookAtLH( &mRotation, pvEyePt, pvLookatPt, &vUp );
	D3DXMatrixRotationYawPitchRoll(&mRotation, 0, -D3DX_PI/2, 0);
	D3DXQuaternionRotationMatrix( &quat, &mRotation );
	m_ViewArcBall.SetQuatNow( quat );

	// Set the radius according to the distance
	D3DXVECTOR3 vEyeToPoint;
	D3DXVec3Subtract( &vEyeToPoint, pvLookatPt, pvEyePt );
	SetRadius( D3DXVec3Length( &vEyeToPoint ) );

	// View information changed. FrameMove should be called.
	m_bDragSinceLastUpdate = true;
}

#endif