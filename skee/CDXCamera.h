#ifndef _CDXCAMERA_H_
#define _CDXCAMERA_H_
#pragma once

#ifdef FIXME

#include "CDXTypes.h"

class CDXArcBall
{
public:
	CDXArcBall();

	// Functions to change behavior
	void Reset();
	void SetTranslationRadius( float fRadiusTranslation ) { m_fRadiusTranslation = fRadiusTranslation; }
	void SetWindow( int nWidth, int nHeight, float fRadius = 0.9f ) { m_nWidth = nWidth; m_nHeight = nHeight; m_fRadius = fRadius; m_vCenter = CDXVec2(m_nWidth/2.0f,m_nHeight/2.0f); }
	void SetOffset( int nX, int nY ) { m_Offset.x = nX; m_Offset.y = nY; }

	// Call these from client and use GetRotationMatrix() to read new rotation matrix
	void OnRotateBegin( int nX, int nY );  // start the rotation (pass current mouse position)
	void OnRotate( int nX, int nY );   // continue the rotation (pass current mouse position)
	void OnRotateEnd();                    // end the rotation

	int GetWidth() const { return m_nWidth; }
	int GetHeight() const { return m_nHeight; }

	// Functions to get/set state
	const CDXMatrix GetRotationMatrix()                     { return DirectX::XMMatrixRotationQuaternion(m_qNow); };
	const CDXMatrix & GetTranslationMatrix() const          { return m_mTranslation; }
	const CDXMatrix & GetTranslationDeltaMatrix() const     { return m_mTranslationDelta; }
	bool        IsBeingDragged() const                      { return m_bDrag; }
	CDXVec & GetQuatNow()                                   { return m_qNow; }
	void        SetQuatNow( CDXVec q ) { m_qNow = q; }

	static CDXVec WINAPI    QuatFromBallPoints( const CDXVec& vFrom, const CDXVec& vTo );


protected:
	CDXMatrix m_mRotation;         // Matrix for arc ball's orientation
	CDXMatrix m_mTranslation;      // Matrix for arc ball's position
	CDXMatrix m_mTranslationDelta; // Matrix for arc ball's position

	POINT m_Offset;   // window offset, or upper-left corner of window
	int m_nWidth;   // arc ball's window width
	int m_nHeight;  // arc ball's window height
	CDXVec2 m_vCenter;  // center of arc ball 
	float m_fRadius;  // arc ball's radius in screen coords
	float m_fRadiusTranslation; // arc ball's radius for translating the target

	CDXVec m_qDown;             // Quaternion before button down
	CDXVec m_qNow;              // Composite quaternion for current drag
	bool m_bDrag;             // Whether user is dragging arc ball

	POINT m_ptLastMouse;      // position of last mouse point
	CDXVec m_vDownPt;           // starting point of rotation arc
	CDXVec m_vCurrentPt;        // current point of rotation arc

	CDXVec                     ScreenToVector( float fScreenPtX, float fScreenPtY );
};

class CDXModelViewerCamera
{
public:
	CDXModelViewerCamera();

	const CDXMatrix&  GetViewMatrix() const { return m_mView; }
	const CDXMatrix&  GetProjMatrix() const { return m_mProj; }
	const CDXVec& GetEyePt() const      { return m_vEye; }
	const CDXVec& GetLookAtPt() const   { return m_vLookAt; }

	void Update();
	void Reset();
	void SetViewParams( CDXVec* pvEyePt, CDXVec* pvLookatPt );
	void SetWindow( int nWidth, int nHeight, float fArcballRadius=0.9f ) { m_WorldArcBall.SetWindow( nWidth, nHeight, fArcballRadius ); m_ViewArcBall.SetWindow( nWidth, nHeight, fArcballRadius ); }
	void SetRadius( float fDefaultRadius=5.0f, float fMinRadius=1.0f, float fMaxRadius=FLT_MAX  ) { m_fDefaultRadius = m_fRadius = fDefaultRadius; m_fMinRadius = fMinRadius; m_fMaxRadius = fMaxRadius; m_bDragSinceLastUpdate = true; }
	void SetModelCenter( CDXVec vModelCenter ) { m_vModelCenter = vModelCenter; }
	void SetViewQuat( CDXVec q ) { m_ViewArcBall.SetQuatNow( q ); m_bDragSinceLastUpdate = true; }
	void SetWorldQuat( CDXVec q ) { m_WorldArcBall.SetQuatNow( q ); m_bDragSinceLastUpdate = true; }
	void SetProjParams( FLOAT fFOV, FLOAT fAspect, FLOAT fNearPlane, FLOAT fFarPlane );
	//void UpdateMouseDelta(int currentX, int currentY);
	void ConstrainToBoundary( CDXVec* pV );
	float GetRadius() const { return m_fRadius; }

	int GetWidth() const { return m_WorldArcBall.GetWidth(); }
	int GetHeight() const { return m_WorldArcBall.GetHeight(); }

	void OnRotateBegin(int x, int y) { m_WorldArcBall.OnRotateBegin(x, y); }
	void OnRotate(int x, int y);
	void OnRotateEnd() { m_WorldArcBall.OnRotateEnd(); }

	void OnMoveBegin(int x, int y);
	void OnMove(int x, int y);
	void OnMoveEnd();

	void SetPanSpeed(float speed) { m_fPanMultiplier = speed; };

	// Functions to get state
	const CDXMatrix* GetWorldMatrix() const { return &m_mWorld; }
	void SetWorldMatrix( CDXMatrix &mWorld ) { m_mWorld = mWorld; m_bDragSinceLastUpdate = true; }

protected:
	CDXMatrix m_mView;              // View matrix 
	CDXMatrix m_mProj;              // Projection matrix

	//CDXVec3 m_vKeyboardDirection;   // Direction vector of keyboard input
	//POINT m_ptLastMousePosition;  // Last absolute position of mouse cursor
	//CDXVec2 m_vMouseDelta;          // Mouse relative delta smoothed over a few frames
	//float m_fFramesToSmoothMouseData; // Number of frames to smooth mouse data over

	CDXVec m_vDefaultEye;          // Default camera eye position
	CDXVec m_vDefaultLookAt;       // Default LookAt position
	CDXVec m_vEye;                 // Camera eye position
	CDXVec m_vLookAt;              // LookAt position
	float m_fCameraYawAngle;      // Yaw angle of camera
	float m_fCameraPitchAngle;    // Pitch angle of camera

	float m_fPanMultiplier;
	CDXVec m_vDragLook;
	CDXVec m_vDragStart;
	CDXVec m_vDragPos;

	//RECT m_rcDrag;               // Rectangle within which a drag can be initiated.
	CDXVec m_vVelocity;            // Velocity of camera
	//bool m_bMovementDrag;        // If true, then camera movement will slow to a stop otherwise movement is instant
	//CDXVec3 m_vVelocityDrag;        // Velocity drag force
	//float m_fDragTimer;           // Countdown timer to apply drag
	//float m_fTotalDragTimeToZero; // Time it takes for velocity to go from full to 0
	//CDXVec2 m_vRotVelocity;         // Velocity of camera

	float m_fFOV;                 // Field of view
	float m_fAspect;              // Aspect ratio
	float m_fNearPlane;           // Near plane
	float m_fFarPlane;            // Far plane

	//float m_fRotationScaler;      // Scaler for rotation
	//float m_fMoveScaler;          // Scaler for movement

	bool m_bClipToBoundary;      // If true, then the camera will be clipped to the boundary
	CDXVec m_vMinBoundary;         // Min point in clip boundary
	CDXVec m_vMaxBoundary;         // Max point in clip boundary

	CDXArcBall m_WorldArcBall;
	CDXArcBall m_ViewArcBall;
	CDXVec m_vModelCenter;
	CDXMatrix m_mModelLastRot;        // Last arcball rotation matrix for model 
	CDXMatrix m_mModelRot;            // Rotation matrix of model
	CDXMatrix m_mWorld;               // World matrix of model

	float m_fRadius;              // Distance from the camera to model 
	float m_fDefaultRadius;       // Distance from the camera to model 
	float m_fMinRadius;           // Min radius
	float m_fMaxRadius;           // Max radius
	bool m_bDragSinceLastUpdate; // True if mouse drag has happened since last time FrameMove is called.

	CDXMatrix m_mCameraRotLast;

};
#endif
#endif