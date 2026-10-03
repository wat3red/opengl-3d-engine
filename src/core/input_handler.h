#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "camera.h"

class InputHandler {
public:
	explicit InputHandler(Camera& camera);
	InputHandler(Camera& camera, glm::ivec2* windowSize);

	void Update(GLFWwindow* window, float deltaTime);
	void Update(GLFWwindow* window);

	void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	void MouseCallback(GLFWwindow* window, double xposIn, double yposIn);
	void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

	void SetCallbacks(GLFWwindow* window);

private:
	Camera& m_Camera;

	float m_LastX = 800.0f;
	float m_LastY = 450.0f;
	float m_LastFrameTime = 0.0f;
	bool m_FirstMouse = true;
};