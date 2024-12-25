#include "input_handler.h"
#include <iostream>

InputHandler::InputHandler(Camera& camera, glm::ivec2* windowSize) :
	camera(camera), 
	lastX(windowSize->x / 2.0f),
	lastY(windowSize->y / 2.0f),
	firstMouse(true),
	m_WindowSize(windowSize) {}



void InputHandler::Update(GLFWwindow* window) {
	float currentFrame = static_cast<float>(glfwGetTime());
	deltaTime = currentFrame - lastFrame;
	lastFrame = currentFrame;

	// updating size
	glfwGetFramebufferSize(window, &m_WindowSize->x, &m_WindowSize->y);
	glViewport(0, 0, m_WindowSize->x, m_WindowSize->y);

#define IS_PRESSED(x) glfwGetKey(window, x) == GLFW_PRESS

	if (IS_PRESSED(GLFW_KEY_ESCAPE))
		glfwSetWindowShouldClose(window, true);

	if (IS_PRESSED(GLFW_KEY_W))
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (IS_PRESSED(GLFW_KEY_S))
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (IS_PRESSED(GLFW_KEY_A))
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (IS_PRESSED(GLFW_KEY_D))
		camera.ProcessKeyboard(RIGHT, deltaTime);
	if (IS_PRESSED(GLFW_KEY_SPACE))
		camera.ProcessKeyboard(UP, deltaTime);
	if (IS_PRESSED(GLFW_KEY_LEFT_CONTROL))
		camera.ProcessKeyboard(DOWN, deltaTime);

#undef IS_PRESSED
}

void InputHandler::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) { }

void InputHandler::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
		if (!camera.isCapturingMovements) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

			double xpos, ypos;
			glfwGetCursorPos(window, &xpos, &ypos);
			lastX = static_cast<float>(xpos);
			lastY = static_cast<float>(ypos);
		}
		else {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		camera.isCapturingMovements = !camera.isCapturingMovements;
	}
}

void InputHandler::MouseCallback(GLFWwindow* window, double xposIn, double yposIn)
{
	if (!camera.isCapturingMovements)
		return;

	float xpos = static_cast<float>(xposIn);
	float ypos = static_cast<float>(yposIn);

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos;

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

void InputHandler::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	if (!camera.isCapturingMovements)
		return;

	camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void InputHandler::SetCallbacks(GLFWwindow* window, InputHandler* inputHandler)
{
	glfwSetScrollCallback(window, [](GLFWwindow* win, double xoff, double yoff) {
		static_cast<InputHandler*>(glfwGetWindowUserPointer(win))->ScrollCallback(win, xoff, yoff);
		});
	glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
		static_cast<InputHandler*>(glfwGetWindowUserPointer(win))->MouseCallback(win, xpos, ypos);
		});
	glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
		static_cast<InputHandler*>(glfwGetWindowUserPointer(win))->MouseButtonCallback(win, button, action, mods);
		});
	glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
		static_cast<InputHandler*>(glfwGetWindowUserPointer(win))->KeyCallback(win, key, scancode, action, mods);
		});

	glfwSetWindowUserPointer(window, inputHandler);
}
