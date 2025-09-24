#pragma once

#include "B3DPrerequisites.h"
#include "Scene/B3DComponent.h"
#include "Math/B3DMath.h"
#include "Input/B3DVirtualInput.h"

namespace b3d
{
	/** Component that controls movement and rotation of the scene object it's attached to. */
	class CameraFlyer : public Component
	{
	public:
		CameraFlyer(const HSceneObject& parent);

		/** Triggered once per frame. Allows the component to handle input and move. */
		void Update();

	private:
		float mCurrentSpeed; /**< Current speed of the camera. */

		Degree mPitch = Degree(0.0f); /**< Current pitch rotation of the camera (looking up or down). */
		Degree mYaw = Degree(0.0f); /**< Current yaw rotation of the camera (looking left or right). */
		bool mLastButtonState = false; /**< Determines was the user rotating the camera last frame. */

		VirtualButton mMoveForward; /**< Key binding for moving the camera forward. */
		VirtualButton mMoveBack; /**< Key binding for moving the camera backward. */
		VirtualButton mMoveLeft; /**< Key binding for moving the camera left. */
		VirtualButton mMoveRight; /**< Key binding for moving the camera right. */
		VirtualButton mFastMove; /**< Key that speeds up movement while held. */
		VirtualButton mRotateCam; /**< Key that allows camera to be rotated while held. */
		VirtualAxis mVerticalAxis; /**< Input device axis used for controlling camera's pitch rotation (up/down). */
		VirtualAxis mHorizontalAxis; /**< Input device axis used for controlling camera's yaw rotation (left/right). */

		static const float kStartSpeed; /**< Initial movement speed. */
		static const float kTopSpeed; /**< Maximum movement speed. */
		static const float kAcceleration; /**< Acceleration that determines how quickly to go from starting to top speed. */
		static const float kFastModeMultiplier; /**< Multiplier applied to the speed when the fast move button is held. */
		static const float kRotationSpeed; /**< Determines speed of camera rotation. */
	};
} // namespace b3d