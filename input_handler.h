#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "camera.h"

class InputHandler {
public:
	InputHandler(Camera& camera, glm::ivec2* windowSize);

	void Update(GLFWwindow* window);
	void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	void MouseCallback(GLFWwindow* window, double xposIn, double yposIn);
	void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

	void SetCallbacks(GLFWwindow* window, InputHandler* inputHandler);

private:
	Camera& camera;

	glm::ivec2* m_WindowSize;

	float lastX, lastY;
	float deltaTime = 0;
	float lastFrame = 0;
	bool firstMouse;
};