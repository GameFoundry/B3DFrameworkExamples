#include "B3DCameraFlyer.h"
#include "Math/B3DVector3.h"
#include "Utility/B3DTime.h"
#include "Math/B3DMath.h"
#include "Scene/B3DSceneObject.h"
#include "Components/B3DCamera.h"
#include "Platform/B3DCursor.h"

namespace b3d
{
	const float CameraFlyer::kStartSpeed = 40.0f;
	const float CameraFlyer::kTopSpeed = 130.0f;
	const float CameraFlyer::kAcceleration = 10.0f;
	const float CameraFlyer::kFastModeMultiplier = 2.0f;
	const float CameraFlyer::kRotationSpeed = 3.0f;

	/** Wraps an angle so it always stays in [0, 360) range. */
	Degree WrapAngle(Degree angle)
	{
		if(angle.GetValueInDegrees() < -360.0f)
			angle += Degree(360.0f);

		if(angle.GetValueInDegrees() > 360.0f)
			angle -= Degree(360.0f);

		return angle;
	}

	CameraFlyer::CameraFlyer(const HSceneObject& parent)
		: Component(parent)
	{
		// Set a name for the component, so we can find it later if needed
		SetName("CameraFlyer");

		// Get handles for key bindings. Actual keys attached to these bindings will be registered during app start-up.
		mMoveForward = VirtualInput::GetOrCreateVirtualButton("Forward");
		mMoveBack = VirtualInput::GetOrCreateVirtualButton("Back");
		mMoveLeft = VirtualInput::GetOrCreateVirtualButton("Left");
		mMoveRight = VirtualInput::GetOrCreateVirtualButton("Right");
		mFastMove = VirtualInput::GetOrCreateVirtualButton("FastMove");
		mRotateCam = VirtualInput::GetOrCreateVirtualButton("RotateCam");
		mHorizontalAxis = VirtualInput::GetOrCreateVirtualAxis("Horizontal");
		mVerticalAxis = VirtualInput::GetOrCreateVirtualAxis("Vertical");
	}

	void CameraFlyer::Update()
	{
		// Check if any movement or rotation keys are being held
		bool goingForward = GetVirtualInput().IsButtonHeld(mMoveForward);
		bool goingBack = GetVirtualInput().IsButtonHeld(mMoveBack);
		bool goingLeft = GetVirtualInput().IsButtonHeld(mMoveLeft);
		bool goingRight = GetVirtualInput().IsButtonHeld(mMoveRight);
		bool fastMove = GetVirtualInput().IsButtonHeld(mFastMove);
		bool camRotating = GetVirtualInput().IsButtonHeld(mRotateCam);

		// If switch to or from rotation mode, hide or show the cursor
		if(camRotating != mLastButtonState)
		{
			if(camRotating)
				Cursor::Instance().Hide();
			else
				Cursor::Instance().Show();

			mLastButtonState = camRotating;
		}

		// If camera is rotating, apply new pitch/yaw rotation values depending on the amount of rotation from the
		// vertical/horizontal axes.
		float frameDelta = GetTime().GetFrameDelta();
		if(camRotating)
		{
			mYaw += Degree(GetVirtualInput().GetAxisValue(mHorizontalAxis) * kRotationSpeed);
			mPitch += Degree(GetVirtualInput().GetAxisValue(mVerticalAxis) * kRotationSpeed);

			mYaw = WrapAngle(mYaw);
			mPitch = WrapAngle(mPitch);

			Quaternion yRot;
			yRot.FromAxisAngle(Vector3::kUnitY, Radian(mYaw));

			Quaternion xRot;
			xRot.FromAxisAngle(Vector3::kUnitX, Radian(mPitch));

			Quaternion camRot = yRot * xRot;
			camRot.Normalize();

			SO()->SetRotation(camRot);
		}

		const Transform& tfrm = SO()->GetTransform();

		// If the movement button is pressed, determine direction to move in
		Vector3 direction = Vector3::kZero;
		if(goingForward) direction += tfrm.GetForward();
		if(goingBack) direction -= tfrm.GetForward();
		if(goingRight) direction += tfrm.GetRight();
		if(goingLeft) direction -= tfrm.GetRight();

		// If a direction is chosen, normalize it to determine final direction.
		if(direction.SquaredLength() != 0)
		{
			direction.Normalize();

			// Apply fast move multiplier if the fast move button is held.
			float multiplier = 1.0f;
			if(fastMove)
				multiplier = kFastModeMultiplier;

			// Calculate current speed of the camera
			mCurrentSpeed = Math::Clamp(mCurrentSpeed + kAcceleration * frameDelta, kStartSpeed, kTopSpeed);
			mCurrentSpeed *= multiplier;
		}
		else
		{
			mCurrentSpeed = 0.0f;
		}

		// If the current speed isn't too small, move the camera in the wanted direction
		float tooSmall = std::numeric_limits<float>::epsilon();
		if(mCurrentSpeed > tooSmall)
		{
			Vector3 velocity = direction * mCurrentSpeed;
			SO()->Move(velocity * frameDelta);
		}
	}
} // namespace b3d
