#include "BsFPSWalker.h"
#include "Math/BsVector3.h"
#include "Math/BsMath.h"
#include "Scene/BsSceneObject.h"
#include "Components/BsCCamera.h"
#include "Components/BsCCharacterController.h"
#include "BsApplication.h"
#include "Physics/BsPhysics.h"
#include "Scene/BsSceneManager.h"
#include "Utility/BsTime.h"

namespace bs
{
	/** Initial movement speed. */
	constexpr float START_SPEED = 4.0f; // m/s

	/** Maximum movement speed. */
	constexpr float TOP_SPEED = 7.0f; // m/s

	/** Acceleration that determines how quickly to go from starting to top speed. */
	constexpr float ACCELERATION = 1.5f;

	/** Multiplier applied to the speed when the fast move button is held. */
	constexpr float FAST_MODE_MULTIPLIER = 2.0f;

	FPSWalker::FPSWalker(const HSceneObject& parent)
		: Component(parent)
	{
		// Set a name for the component, so we can find it later if needed
		SetName("FPSWalker");

		// Find the CharacterController we'll be using for movement
		mController = SO()->GetComponent<CCharacterController>();

		// Get handles for key bindings. Actual keys attached to these bindings will be registered during app start-up.
		mMoveForward = VirtualButton("Forward");
		mMoveBack = VirtualButton("Back");
		mMoveLeft = VirtualButton("Left");
		mMoveRight = VirtualButton("Right");
		mFastMove = VirtualButton("FastMove");
	}

	void FPSWalker::FixedUpdate()
	{
		// Check if any movement keys are being held
		bool goingForward = gVirtualInput().IsButtonHeld(mMoveForward);
		bool goingBack = gVirtualInput().IsButtonHeld(mMoveBack);
		bool goingLeft = gVirtualInput().IsButtonHeld(mMoveLeft);
		bool goingRight = gVirtualInput().IsButtonHeld(mMoveRight);
		bool fastMove = gVirtualInput().IsButtonHeld(mFastMove);

		const Transform& tfrm = SO()->GetTransform();

		// If the movement button is pressed, determine direction to move in
		Vector3 direction = Vector3::kZero;
		if(goingForward) direction += tfrm.GetForward();
		if(goingBack) direction -= tfrm.GetForward();
		if(goingRight) direction += tfrm.GetRight();
		if(goingLeft) direction -= tfrm.GetRight();

		// Eliminate vertical movement
		direction.Y = 0.0f;
		direction.Normalize();

		const float frameDelta = gTime().GetFixedFrameDelta();

		// If a direction is chosen, normalize it to determine final direction.
		if(direction.SquaredLength() != 0)
		{
			direction.Normalize();

			// Apply fast move multiplier if the fast move button is held.
			float multiplier = 1.0f;
			if(fastMove)
				multiplier = FAST_MODE_MULTIPLIER;

			// Calculate current speed of the camera
			mCurrentSpeed = Math::Clamp(mCurrentSpeed + ACCELERATION * frameDelta, START_SPEED, TOP_SPEED);
			mCurrentSpeed *= multiplier;
		}
		else
		{
			mCurrentSpeed = 0.0f;
		}

		// If the current speed isn't too small, move the camera in the wanted direction
		Vector3 velocity(BsZero);
		float tooSmall = std::numeric_limits<float>::epsilon();
		if(mCurrentSpeed > tooSmall)
			velocity = direction * mCurrentSpeed;

		const SPtr<SceneInstance> sceneInstance = SceneManager::Instance().GetMainScene();
		BS_ASSERT(sceneInstance != nullptr);

		const SPtr<PhysicsScene> physicsScene = sceneInstance->GetPhysicsScene();

		// Note: Gravity is acceleration, but since the walker doesn't support falling, just apply it as a velocity
		Vector3 gravity = physicsScene->GetGravity();
		mController->Move((velocity + gravity) * frameDelta);
	}
} // namespace bs
