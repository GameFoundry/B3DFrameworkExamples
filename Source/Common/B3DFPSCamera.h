#pragma once

#include "B3DPrerequisites.h"
#include "Scene/B3DComponent.h"
#include "Math/B3DDegree.h"
#include "Input/B3DVirtualInput.h"

namespace b3d
{
	/**
	 * Component that controls rotation of the scene objects it's attached to through mouse input. Used for first person
	 * views.
	 */
	class FPSCamera : public Component
	{
	public:
		FPSCamera(const HSceneObject& parent);

		/**
		 * Sets the character scene object to manipulate during rotations. When set, all yaw rotations will be applied to
		 * the provided scene object, otherwise they will be applied to the current object.
		 */
		void SetCharacter(const HSceneObject& characterSO) { mCharacterSO = characterSO; }

		/** Triggered once per frame. Allows the component to handle input and move. */
		void Update();

	private:
		/** Applies the current yaw and pitch angles, rotating the object. Also wraps and clamps the angles as necessary. */
		void ApplyAngles();

		HSceneObject mCharacterSO; /**< Optional parent object to manipulate. */

		Degree mPitch = Degree(0.0f); /**< Current pitch rotation of the camera (looking up or down). */
		Degree mYaw = Degree(0.0f); /**< Current yaw rotation of the camera (looking left or right). */

		VirtualAxis mVerticalAxis; /**< Input device axis used for controlling camera's pitch rotation (up/down). */
		VirtualAxis mHorizontalAxis; /**< Input device axis used for controlling camera's yaw rotation (left/right). */
	};

	using HFPSCamera = TGameObjectHandle<FPSCamera>;
} // namespace b3d