#include "B3DObjectRotator.h"
#include "Math/B3DVector3.h"
#include "Utility/B3DTime.h"
#include "Math/B3DMath.h"
#include "Scene/B3DSceneObject.h"
#include "Platform/B3DCursor.h"

namespace b3d
{
	const float ObjectRotator::ROTATION_SPEED = 1.0f;

	/** Wraps an angle so it always stays in [0, 360) range. */
	Degree WrapAngle2(Degree angle)
	{
		if(angle.GetValueInDegrees() < -360.0f)
			angle += Degree(360.0f);

		if(angle.GetValueInDegrees() > 360.0f)
			angle -= Degree(360.0f);

		return angle;
	}

	ObjectRotator::ObjectRotator(const HSceneObject& parent)
		: Component(parent), mPitch(0.0f), mYaw(0.0f), mLastButtonState(false)
	{
		// Set a name for the component, so we can find it later if needed
		SetName("ObjectRotator");

		// Get handles for key bindings. Actual keys attached to these bindings will be registered during app start-up.
		mRotateObj = VirtualInput::GetOrCreateVirtualButton("RotateObj");
		mHorizontalAxis = VirtualInput::GetOrCreateVirtualAxis("Horizontal");
		mVerticalAxis = VirtualInput::GetOrCreateVirtualAxis("Vertical");

		// Determine initial yaw and pitch
		Quaternion rotation = SO()->GetTransform().GetRotation();

		Radian pitch, yaw, roll;
		(void)rotation.ToEulerAngles(pitch, yaw, roll);

		mPitch = pitch;
		mYaw = yaw;
	}

	void ObjectRotator::Update()
	{
		// Check if any movement or rotation keys are being held
		bool isRotating = GetVirtualInput().IsButtonHeld(mRotateObj);

		// If switch to or from rotation mode, hide or show the cursor
		if(isRotating != mLastButtonState)
		{
			if(isRotating)
				Cursor::Instance().Hide();
			else
				Cursor::Instance().Show();

			mLastButtonState = isRotating;
		}

		// If we're rotating, apply new pitch/yaw rotation values depending on the amount of rotation from the
		// vertical/horizontal axes.
		if(isRotating)
		{
			mYaw -= Degree(GetVirtualInput().GetAxisValue(mHorizontalAxis) * ROTATION_SPEED);
			mPitch -= Degree(GetVirtualInput().GetAxisValue(mVerticalAxis) * ROTATION_SPEED);

			mYaw = WrapAngle2(mYaw);
			mPitch = WrapAngle2(mPitch);

			Quaternion yRot;
			yRot.FromAxisAngle(Vector3::kUnitY, Radian(mYaw));

			Quaternion xRot;
			xRot.FromAxisAngle(Vector3::kUnitX, Radian(mPitch));

			Quaternion camRot = yRot * xRot;
			camRot.Normalize();

			SO()->SetRotation(camRot);
		}
	}
} // namespace b3d
