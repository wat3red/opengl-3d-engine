#include "scene/components.h"
#include "scene/scene.h"
#include "physics/collision.h"
#include <cmath>
#include <algorithm>

void CharacterControllerComponent::Jump(float jumpForce)
{
	if (IsGrounded)
	{
		Velocity.y = jumpForce;
		IsGrounded = false;
	}
}

void CharacterControllerComponent::SimpleMove(Scene& scene, Entity entity, const glm::vec3& horizontalVelocity, float deltaTime)
{
	if (deltaTime <= 0.0f)
		return;

	if (!IsGrounded)
	{
		Velocity.y += Gravity * deltaTime;
	}
	else
	{
		// Downward snap velocity when grounded to track steps and slopes smoothly
		if (Velocity.y <= 0.0f)
		{
			Velocity.y = -2.0f;
		}
	}

	glm::vec3 totalDisplacement = horizontalVelocity * deltaTime + glm::vec3(0.0f, Velocity.y * deltaTime, 0.0f);
	Move(scene, entity, totalDisplacement);

	Velocity.x = horizontalVelocity.x;
	Velocity.z = horizontalVelocity.z;
}

glm::vec3 CharacterControllerComponent::Move(Scene& scene, Entity entity, const glm::vec3& displacement)
{
	if (!entity.HasComponent<TransformComponent>())
		return glm::vec3(0.0f);

	auto& tc = entity.GetComponent<TransformComponent>();
	glm::vec3 currentPos = tc.Position;

	// 1. Depenetration check: push out if already overlapping geometry
	glm::vec3 pushVector(0.0f);
	if (Physics::CheckCapsuleOverlap(scene, currentPos, Radius, Height, Center, pushVector, entity, CollisionMask))
	{
		currentPos += pushVector;
	}

	glm::vec3 remainingMotion = displacement;
	glm::vec3 totalMoved(0.0f);

	auto ProjectOnPlane = [](const glm::vec3& vec, const glm::vec3& normal) -> glm::vec3 {
		float dot = glm::dot(vec, normal);
		return vec - normal * dot;
	};

	// 2. Iterative slide-and-collide loop
	const int maxIterations = 4;
	for (int iter = 0; iter < maxIterations; ++iter)
	{
		float motionLen = glm::length(remainingMotion);
		if (motionLen < 0.0001f)
			break;

		// Check step-up for stairs and curbs
		glm::vec3 horizMotion = glm::vec3(remainingMotion.x, 0.0f, remainingMotion.z);
		bool stepped = false;

		if (StepOffset > 0.01f && glm::length(horizMotion) > 0.001f && IsGrounded)
		{
			// a. Sweep upward
			Physics::RaycastHit upHit;
			bool blockedUp = Physics::SweepCapsule(scene, currentPos, glm::vec3(0.0f, StepOffset, 0.0f), Radius, Height, Center, upHit, entity, CollisionMask);
			float actualStepUp = blockedUp ? std::max(0.0f, upHit.Distance - SkinWidth) : StepOffset;

			if (actualStepUp > 0.05f)
			{
				glm::vec3 elevatedPos = currentPos + glm::vec3(0.0f, actualStepUp, 0.0f);

				// b. Sweep horizontally forward from elevated position
				Physics::RaycastHit stepFwdHit;
				bool blockedFwd = Physics::SweepCapsule(scene, elevatedPos, horizMotion, Radius, Height, Center, stepFwdHit, entity, CollisionMask);
				float fwdDist = blockedFwd ? std::max(0.0f, stepFwdHit.Distance - SkinWidth) : glm::length(horizMotion);
				glm::vec3 fwdElevated = elevatedPos + glm::normalize(horizMotion) * fwdDist;

				// c. Sweep downward to land on top of the step
				Physics::RaycastHit stepDownHit;
				bool hitDown = Physics::SweepCapsule(scene, fwdElevated, glm::vec3(0.0f, -actualStepUp - SkinWidth * 2.0f, 0.0f), Radius, Height, Center, stepDownHit, entity, CollisionMask);

				if (hitDown)
				{
					float slopeAngle = glm::degrees(std::acos(glm::clamp(stepDownHit.Normal.y, -1.0f, 1.0f)));
					if (slopeAngle <= SlopeLimit)
					{
						glm::vec3 landedPos = fwdElevated + glm::vec3(0.0f, -stepDownHit.Distance, 0.0f);
						glm::vec3 stepDelta = landedPos - currentPos;
						totalMoved += stepDelta;
						currentPos = landedPos;

						remainingMotion.x = 0.0f;
						remainingMotion.z = 0.0f;
						stepped = true;
					}
				}
			}
		}

		if (stepped)
		{
			if (std::abs(remainingMotion.y) < 0.0001f)
				break;
		}

		// Standard capsule sweep along remaining motion
		Physics::RaycastHit hit;
		bool hasHit = Physics::SweepCapsule(scene, currentPos, remainingMotion, Radius, Height, Center, hit, entity, CollisionMask);

		if (!hasHit)
		{
			currentPos += remainingMotion;
			totalMoved += remainingMotion;
			remainingMotion = glm::vec3(0.0f);
			break;
		}

		// Move up to the contact point
		float safeDist = std::max(0.0f, hit.Distance - SkinWidth);
		glm::vec3 dir = glm::normalize(remainingMotion);
		glm::vec3 moveStep = dir * safeDist;

		currentPos += moveStep;
		totalMoved += moveStep;

		float consumedFraction = (motionLen > 0.0001f) ? (safeDist / motionLen) : 1.0f;
		remainingMotion *= (1.0f - std::min(1.0f, consumedFraction));

		// Slide along the surface normal
		remainingMotion = ProjectOnPlane(remainingMotion, hit.Normal);
	}

	// 3. Ground check & slope snapping
	float groundProbeDist = SkinWidth + 0.08f;
	Physics::RaycastHit groundHit;
	bool grounded = Physics::SweepCapsule(scene, currentPos, glm::vec3(0.0f, -groundProbeDist, 0.0f), Radius, Height, Center, groundHit, entity, CollisionMask);

	if (grounded)
	{
		float slopeAngle = glm::degrees(std::acos(glm::clamp(groundHit.Normal.y, -1.0f, 1.0f)));
		if (slopeAngle <= SlopeLimit)
		{
			IsGrounded = true;
			GroundNormal = groundHit.Normal;
			currentPos += glm::vec3(0.0f, -groundHit.Distance + SkinWidth, 0.0f);
			if (Velocity.y < 0.0f)
				Velocity.y = 0.0f;
		}
		else
		{
			IsGrounded = false;
			GroundNormal = groundHit.Normal;
		}
	}
	else
	{
		IsGrounded = false;
		GroundNormal = glm::vec3(0.0f, 1.0f, 0.0f);
	}

	tc.Position = currentPos;
	return totalMoved;
}
