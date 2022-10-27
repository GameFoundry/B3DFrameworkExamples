#include "BsObjectRotator.h"
#include "Math/BsVector3.h"
#include "Utility/BsTime.h"
#include "Math/BsMath.h"
#include "Scene/BsSceneObject.h"
#include "Platform/BsCursor.h"

namespace bs
{
	const float ObjectRotator::ROTATION_SPEED = 1.0f;

	/** Wraps an angle so it always stays in [0, 360) range. */
	Degree wrapAngle2(Degree angle)
	{
		if(angle.ValueDegrees() < -360.0f)
			angle += Degree(360.0f);

		if(angle.ValueDegrees() > 360.0f)
			angle -= Degree(360.0f);

		return angle;
	}

	ObjectRotator::ObjectRotator(const HSceneObject& parent)
		: Component(parent), mPitch(0.0f), mYaw(0.0f), mLastButtonState(false)
	{
		// Set a name for the component, so we can find it later if needed
		SetName("ObjectRotator");

		// Get handles for key bindings. Actual keys attached to these bindings will be registered during app start-up.
		mRotateObj = VirtualButton("RotateObj");
		mHorizontalAxis = VirtualAxis("Horizontal");
		mVerticalAxis = VirtualAxis("Vertical");

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
		bool isRotating = gVirtualInput().IsButtonHeld(mRotateObj);

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
			mYaw -= Degree(gVirtualInput().GetAxisValue(mHorizontalAxis) * ROTATION_SPEED);
			mPitch -= Degree(gVirtualInput().GetAxisValue(mVerticalAxis) * ROTATION_SPEED);

			mYaw = wrapAngle2(mYaw);
			mPitch = wrapAngle2(mPitch);

			Quaternion yRot;
			yRot.FromAxisAngle(Vector3::kUnitY, Radian(mYaw));

			Quaternion xRot;
			xRot.FromAxisAngle(Vector3::kUnitX, Radian(mPitch));

			Quaternion camRot = yRot * xRot;
			camRot.Normalize();

			SO()->SetRotation(camRot);
		}
	}
} // namespace bs
