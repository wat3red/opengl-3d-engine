#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum Camera_Movement {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT, 
	UP,
	DOWN
};

inline constexpr float YAW = -90.0f;
inline constexpr float PITCH = 0.0f;
inline constexpr float SPEED = 12.5f;
inline constexpr float SENSITIVITY = 0.1f;
inline constexpr float ZOOM = 90.0f;

inline constexpr float NEAR_PLANE = 0.05f;
inline constexpr float FAR_PLANE = 1000.0f;

class Camera
{
public:
	glm::vec3 Position;
	glm::vec3 Front;
	glm::vec3 Up;
	glm::vec3 Right;
	glm::vec3 WorldUp;

	float Yaw;
	float Pitch;

	bool isCapturingMovements = false;

	float MovementSpeed;
	float MouseSensitivity;
	float Zoom;
	
	float nearPlane;
	float farPlane;

	Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), 
		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), 
		float yaw = YAW, 
		float pitch = PITCH) : Front(glm::vec3(0.0f, 0.0f, -1.0f)),
		MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM),
		nearPlane(NEAR_PLANE), farPlane(FAR_PLANE)
	{
		Position = position;
		WorldUp = up;
		Yaw = yaw;
		Pitch = pitch;
		updateCameraVectors();
	}

	Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch) : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Zoom(ZOOM)
	{
		Position = glm::vec3(posX, posY, posZ);
		WorldUp = glm::vec3(upX, upY, upZ);
		Yaw = yaw;
		Pitch = pitch;
		updateCameraVectors();
	}

	[[nodiscard]] glm::mat4 GetViewMatrix() const
	{
		return glm::lookAt(Position, Position + Front, Up);
	}

	[[nodiscard]] glm::mat4 GetProjectionMatrix(glm::ivec2 windowSize) const
	{
		float aspectRatio = (windowSize.y == 0) ? 1.0f : static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y);

		return glm::perspective(
			glm::radians(Zoom),
			aspectRatio,
			nearPlane,
			farPlane
		);
	}

	void ProcessKeyboard(Camera_Movement direction, float deltaTime)
	{
		float velocity = MovementSpeed * deltaTime;
		if (direction == FORWARD)
			Position += Front * velocity;
		if (direction == BACKWARD)
			Position -= Front * velocity;
		if (direction == LEFT)
			Position -= Right * velocity;
		if (direction == RIGHT)
			Position += Right * velocity;
		if (direction == UP)
			Position += Up * velocity;
		if (direction == DOWN)
			Position -= Up * velocity;
	}

	void ProcessMouseMovement(float xoffset, float yoffset, unsigned char constrainPitch = true)
	{
		xoffset *= MouseSensitivity;
		yoffset *= MouseSensitivity;

		Yaw += xoffset;
		Pitch += yoffset;

		// Constrain pitch to [-89, 89] degrees to prevent gimbal lock screen flipping
		if (constrainPitch)
		{
			if (Pitch > 89.0f)
				Pitch = 89.0f;
			if (Pitch < -89.0f)
				Pitch = -89.0f;
		}

		updateCameraVectors();
	}

	void ProcessMouseScroll(float yoffset)
	{
		Zoom -= (float)yoffset;
		if (Zoom < 1.f)
			Zoom = 1.f;
		if (Zoom > 180.f)
			Zoom = 180.f;
	}

private:
	void updateCameraVectors()
	{
		glm::vec3 front;
		front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
		front.y = sin(glm::radians(Pitch));
		front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
		Front = glm::normalize(front);

		// Normalize vectors to maintain uniform movement speed regardless of camera orientation
		Right = glm::normalize(glm::cross(Front, WorldUp));
		Up = glm::normalize(glm::cross(Right, Front));
	}
};
